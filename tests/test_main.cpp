#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "../accesspoint/api.h"
#include "../logging/logger.h"
#include "../modules/cameras/cameraautomation.h"
#include "../modules/cameras/cameras.h"
#include "../modules/cameras/motiondetector.h"
#include "../modules/cameras/sounddetector.h"
#include "../notifications/bridgenotifier.h"
#include "../notifications/notifications.h"
#include "../threading/threadmanager.h"

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
    BridgeNotifier bridgeNotifier("http://127.0.0.1:6553/notifications");
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

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const CameraFeed feed = api.getCameraFeed("bedroomcamera");
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
}

int main() {
    test_cameras_register_feed_and_defaults();
    test_camera_modes_change_analyzer_profile();
    test_api_and_camera_automation_generate_detection_events();
    test_motion_detector_uses_real_frame_bytes();
    test_frame_analyzer_parses_helper_output();

    if (failures > 0) {
        std::cerr << failures << " hub test(s) failed.\n";
        return 1;
    }

    std::cout << "All hub tests passed.\n";
    return 0;
}
