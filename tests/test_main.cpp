#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "../accesspoint/api.h"
#include "../bridge/commands/commandBuilder.h"
#include "../bridge/handlers/deviceEventHandler.h"
#include "../bridge/handlers/deviceStateHandler.h"
#include "../bridge/handlers/streamHandler.h"
#include "../bridge/hubClient.h"
#include "../bridge/routing/messageRouter.h"
#include "../bridge/state/bridgeStateCache.h"
#include "../bridge/streams/streamRegistry.h"
#include "../bridge/subscriptions/subscriptionManager.h"
#include "../logging/logger.h"
#include "../modules/cameras/cameraautomation.h"
#include "../modules/cameras/cameras.h"
#include "../modules/cameras/motiondetector.h"
#include "../modules/cameras/sounddetector.h"
#include "../notifications/bridgenotifier.h"
#include "../notifications/notifications.h"
#include "../threading/threadmanager.h"
#include "../testing/analysis/mockVisionAnalyzer.h"
#include "../testing/analysis/occupancyAnalyzer.h"
#include "../testing/analysis/streamAnalysisManager.h"
#include "../testing/automation/automationEngine.h"
#include "../testing/debugging/eventTracer.h"
#include "../testing/debugging/orchestrationConsole.h"
#include "../testing/debugging/stateDebugger.h"
#include "../testing/simulation/orchestrationSimulator.h"
#include "../testing/testing/integrationTester.h"
#include "../testing/testing/workflowTester.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << message << "\n";
    }
}

std::string buildMonoPcm16Wav(const std::vector<int16_t>& samples) {
    std::string wav;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t riffSize = 36u + dataSize;

    auto append16 = [&wav](uint16_t value) {
        wav.push_back(static_cast<char>(value & 0xFF));
        wav.push_back(static_cast<char>((value >> 8) & 0xFF));
    };
    auto append32 = [&wav](uint32_t value) {
        wav.push_back(static_cast<char>(value & 0xFF));
        wav.push_back(static_cast<char>((value >> 8) & 0xFF));
        wav.push_back(static_cast<char>((value >> 16) & 0xFF));
        wav.push_back(static_cast<char>((value >> 24) & 0xFF));
    };

    wav += "RIFF";
    append32(riffSize);
    wav += "WAVE";
    wav += "fmt ";
    append32(16);
    append16(1);
    append16(1);
    append32(8000);
    append32(16000);
    append16(2);
    append16(16);
    wav += "data";
    append32(dataSize);
    for (const int16_t sample : samples) {
        append16(static_cast<uint16_t>(sample));
    }
    return wav;
}

void test_cameras_register_feed_and_defaults() {
    Cameras cameras;

    require(cameras.registerFeed("bedroomcamera", "http://camera/frame.jpg", "http://bridge/stream", "http://bridge/stream", "http://bridge/frame", "", "1080p"),
            "registerFeed should succeed for valid feed.");

    const CameraFeed feed = cameras.getFeed("bedroomcamera");
    require(feed.cameraId == "bedroomcamera", "Camera feed should be stored.");
    require(feed.modeProfile.mode == CameraMode::Home, "New camera feed should default to home mode.");
    require(feed.modeProfile.analyzers.motionDetectionEnabled, "Home mode should enable motion detection.");
    require(feed.modeProfile.analyzers.facialRecognitionEnabled, "Home mode should keep facial recognition enabled for logging.");
    require(!feed.modeProfile.alerts.notifyOnMotion, "Home mode should suppress motion notifications.");
}

void test_camera_modes_change_analyzer_profile() {
    Cameras cameras;
    cameras.registerFeed("bedroomcamera", "http://camera/frame.jpg", "http://bridge/stream", "http://bridge/stream", "http://bridge/frame", "", "1080p");

    require(cameras.setMode("bedroomcamera", CameraMode::Away), "setMode should succeed for known camera.");
    const CameraFeed awayFeed = cameras.getFeed("bedroomcamera");
    require(awayFeed.modeProfile.analyzers.motionDetectionEnabled, "Away mode should enable motion detection.");
    require(awayFeed.modeProfile.analyzers.facialRecognitionEnabled, "Away mode should enable facial recognition.");
    require(awayFeed.modeProfile.analyzers.strangeSoundDetectionEnabled, "Away mode should enable strange sound detection.");
    require(awayFeed.modeProfile.alerts.notifyOnMotion, "Away mode should notify on motion.");
    require(awayFeed.modeProfile.alerts.notifyOnUnknownFace, "Away mode should notify on unknown faces.");

    require(cameras.setMode("bedroomcamera", CameraMode::Privacy), "Privacy mode should be accepted.");
    const CameraFeed privacyFeed = cameras.getFeed("bedroomcamera");
    require(!privacyFeed.modeProfile.analyzers.rawFeedVisible, "Privacy mode should disable raw feed visibility.");
    require(!privacyFeed.modeProfile.analyzers.motionDetectionEnabled, "Privacy mode should disable motion detection.");
}

void test_api_and_camera_automation_generate_detection_events() {
    Logger::instance().initialize("logs/test-home-automation-hub.log");

    ThreadManager threadManager;
    threadManager.start(1, 1);

    Cameras cameras;
    NotificationCenter notifications;
    BridgeNotifier bridgeNotifier;
    CameraAutomation automation(cameras, threadManager, notifications, bridgeNotifier);
    API api(cameras, automation, notifications);

    require(api.registerCameraFeed(
                "bedroomcamera",
                "http://camera/frame.jpg",
                "http://bridge/stream",
                "http://bridge/stream",
                "http://127.0.0.1:6553/frame.jpg",
                "",
                "1080p"),
            "API registerCameraFeed should succeed.");

    require(api.setCameraMode("bedroomcamera", "away"), "API setCameraMode should accept away mode.");

    CameraFeed feed;
    const auto processingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        feed = api.getCameraFeed("bedroomcamera");
        if (feed.processingStatus == "Capture Failed") {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (std::chrono::steady_clock::now() < processingDeadline);

    require(feed.cameraId == "bedroomcamera", "API should return stored camera feed.");
    require(feed.processingStatus == "Capture Failed", "Camera processing should record capture failure for unreachable frame URL.");
    require(feed.detections.lastEvent == "Frame fetch failed", "Processing should record frame fetch failures.");
    require(!feed.detections.lastProcessedAt.empty(), "Processing should stamp lastProcessedAt.");
    require(notifications.count() >= 2, "Registration and capture failure should queue notifications.");

    threadManager.stop();
}

void test_motion_detector_uses_real_frame_bytes() {
    MotionDetector detector;

    const MotionResult baseline = detector.analyzeFrame("bedroomcamera", "AAAAABBBBBCCCCCDDDD");
    require(!baseline.detected, "First frame should establish baseline without motion.");

    const MotionResult changed = detector.analyzeFrame("bedroomcamera", "ZZZZZYYYYYXXXXXWWWW");
    require(changed.detected, "Changed frame bytes should detect motion.");
    require(changed.score > 0.12, "Changed frame bytes should exceed the motion threshold.");
}

void test_frame_analyzer_parses_helper_output() {
    SoundDetector detector;

    const std::string quietWav = buildMonoPcm16Wav(std::vector<int16_t>(128, 0));
    const std::string loudWav = buildMonoPcm16Wav(std::vector<int16_t>(128, 30000));

    const SoundResult baseline = detector.analyzeClip("bedroomcamera", quietWav);
    require(baseline.error.empty(), "Sound detector should decode a PCM WAV clip. error=" + baseline.error);
    require(!baseline.detected, "Baseline quiet clip should not trigger sound anomaly.");

    const SoundResult changed = detector.analyzeClip("bedroomcamera", loudWav);
    require(changed.error.empty(), "Sound detector should decode the second PCM WAV clip. error=" + changed.error);
    require(changed.detected, "Louder follow-up clip should trigger sound anomaly detection.");
    require(changed.score > baseline.score, "Louder clip should produce a higher RMS score.");
}

void test_hub_protocol_routes_state_streams_and_outbound_intelligence() {
    using namespace homeautomationhub::bridge;

    SubscriptionManager subscriptions;
    BridgeStateCache stateCache;
    StreamRegistry streamRegistry;
    DeviceStateHandler stateHandler(stateCache);
    DeviceEventHandler eventHandler;
    StreamHandler streamHandler(streamRegistry);
    MessageRouter router(subscriptions);
    router.registerHandler(HubCategory::DeviceState, [&stateHandler](const HubMessage& message) {
        return stateHandler.handle(message);
    });
    router.registerHandler(HubCategory::DeviceEvent, [&eventHandler](const HubMessage& message) {
        return eventHandler.handle(message);
    });
    router.registerHandler(HubCategory::StreamAvailable, [&streamHandler](const HubMessage& message) {
        return streamHandler.handle(message);
    });
    router.registerHandler(HubCategory::StreamClosed, [&streamHandler](const HubMessage& message) {
        return streamHandler.handle(message);
    });

    bool eventSeen = false;
    subscriptions.subscribe("device.*", [&eventSeen](const HubMessage& message) {
        eventSeen = eventSeen || message.category == HubCategory::DeviceEvent;
    });
    subscriptions.subscribe("stream.*", [](const HubMessage&) {});

    std::vector<std::string> outbound;
    HubClient client(
        HubClientConfig{.bridgeProtocolUrl = "memory://bridge"},
        router,
        subscriptions,
        [&outbound](const std::string&, const std::string& jsonBody, std::string&) {
            outbound.push_back(jsonBody);
            return true;
        }
    );

    require(client.connect(), "Hub client should send an initial subscription request.");
    require(!outbound.empty(), "Hub client should emit a subscription envelope.");
    require(parseHubMessage(outbound.front()).message->category == HubCategory::SubscriptionRequest,
            "Initial Hub client envelope should be a subscription request.");

    require(client.receive(R"({
        "category": "device.state",
        "source": { "deviceId": "bedroomLight1", "room": "bedroom" },
        "data": { "power": true, "brightness": 75 }
    })"), "Hub client should accept normalized device state.");
    const auto cachedLight = stateCache.deviceState("bedroomLight1");
    require(cachedLight.has_value(), "Device state should reach orchestration cache.");
    require(cachedLight.has_value() && cachedLight->data.value("brightness", 0) == 75,
            "Cached device state should preserve normalized data.");

    require(client.receive(R"({
        "category": "device.event",
        "source": { "deviceId": "hallwayMotion1" },
        "data": { "event": "motionDetected", "confidence": 0.94 }
    })"), "Hub client should route momentary device events.");
    require(eventSeen, "Device event should fan out through subscriptions.");

    require(client.receive(R"({
        "category": "stream.available",
        "source": { "deviceId": "bedroomCamera1" },
        "data": {
            "streamId": "camera-bedroom-01",
            "streamType": "rtsp",
            "codec": "h264",
            "resolution": "1920x1080",
            "fps": 30,
            "endpoint": "rtsp://bridge/camera-bedroom-01"
        }
    })"), "Hub client should accept stream metadata.");
    require(streamRegistry.stream("camera-bedroom-01").has_value(), "Stream registry should expose active metadata.");

    require(!client.receive(R"({ "category": "stream.available", "source": {}, "data": {} })"),
            "Malformed stream metadata should be rejected without crashing.");
    require(client.sendAnalysisResult("vision", Json{{"event", "personDetected"}, {"confidence", 0.91}, {"cameraId", "bedroomCamera1"}}),
            "Hub client should send analysis.result envelopes.");
    require(client.sendCommand("bedroomLight1", "setBrightness", Json{{"brightness", 20}}),
            "Hub client should send bridge.command envelopes.");
    require(parseHubMessage(outbound[outbound.size() - 2]).message->category == HubCategory::AnalysisResult,
            "Analysis envelope should use analysis.result.");
    require(parseHubMessage(outbound.back()).message->category == HubCategory::BridgeCommand,
            "Command envelope should use bridge.command.");
}

void test_mock_orchestration_environment_validates_workflows() {
    using namespace homeautomationhub;
    using namespace homeautomationhub::bridge;

    testing::EventTracer tracer;
    testing::OrchestrationConsole console(tracer);
    SubscriptionManager subscriptions;
    subscriptions.subscribe("*", [&console](const HubMessage& message) { console.bridgeEvent(message); });

    BridgeStateCache stateCache;
    StreamRegistry streamRegistry;
    DeviceStateHandler stateHandler(stateCache);
    DeviceEventHandler eventHandler;
    StreamHandler streamHandler(streamRegistry);
    MessageRouter router(subscriptions);
    router.registerHandler(HubCategory::DeviceState, [&stateHandler](const HubMessage& message) {
        return stateHandler.handle(message);
    });
    router.registerHandler(HubCategory::DeviceEvent, [&eventHandler](const HubMessage& message) {
        return eventHandler.handle(message);
    });
    router.registerHandler(HubCategory::StreamAvailable, [&streamHandler](const HubMessage& message) {
        return streamHandler.handle(message);
    });
    router.registerHandler(HubCategory::StreamClosed, [&streamHandler](const HubMessage& message) {
        return streamHandler.handle(message);
    });

    std::vector<HubMessage> outbound;
    HubClient client(
        HubClientConfig{.bridgeProtocolUrl = "memory://mock-bridge"},
        router,
        subscriptions,
        [&outbound](const std::string&, const std::string& jsonBody, std::string&) {
            const HubParseResult parsed = parseHubMessage(jsonBody);
            if (!parsed.ok()) return false;
            outbound.push_back(*parsed.message);
            return true;
        }
    );

    testing::OccupancyAnalyzer occupancy(tracer, console);
    occupancy.attach(subscriptions);
    testing::AutomationEngine automation(client, tracer, console);
    automation.installMockRules();
    automation.attach(subscriptions);
    testing::MockVisionAnalyzer vision(client, tracer, console);
    testing::StreamAnalysisManager streams(streamRegistry, vision, tracer, console);
    streams.attach(subscriptions);
    testing::StateDebugger stateDebugger(stateCache, occupancy, streamRegistry, subscriptions);
    require(client.connect(), "Mock Hub client should emit subscriptions before scenarios.");

    testing::OrchestrationSimulator simulator(client, tracer, console);
    testing::IntegrationTester integration(simulator, outbound);
    const testing::OrchestrationTestReport commandFlow = integration.validateMotionCommandFlow();
    const testing::OrchestrationTestReport analysisFlow = integration.validateStreamAnalysisFlow();
    testing::WorkflowTester workflow(simulator, stateCache, streamRegistry, occupancy, tracer);
    const testing::OrchestrationTestReport workflowFlow = workflow.validateOrchestrationWorkflow();

    require(commandFlow.passed, "Motion integration flow should emit a bridge command.");
    require(analysisFlow.passed, "Stream integration flow should emit mock analysis results.");
    require(workflowFlow.passed, "Workflow simulation should synchronize state, occupancy, and streams.");
    require(!streams.attachedStreams().empty(), "Stream manager should retain active analysis attachment for open mock stream.");
    require(!console.lines().empty(), "Orchestration console should capture debugging lines.");
    require(stateDebugger.snapshot(&automation).find("cached_devices=") != std::string::npos,
            "State debugger should provide a mock orchestration snapshot.");
}
}

int main() {
    test_cameras_register_feed_and_defaults();
    test_camera_modes_change_analyzer_profile();
    test_api_and_camera_automation_generate_detection_events();
    test_motion_detector_uses_real_frame_bytes();
    test_frame_analyzer_parses_helper_output();
    test_hub_protocol_routes_state_streams_and_outbound_intelligence();
    test_mock_orchestration_environment_validates_workflows();

    if (failures > 0) {
        std::cerr << failures << " hub test(s) failed.\n";
        return 1;
    }

    std::cout << "All hub tests passed.\n";
    return 0;
}
