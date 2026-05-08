#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "../accesspoint/api.h"
#include "../logging/logger.h"
#include "../modules/cameras/cameraautomation.h"
#include "../modules/cameras/cameras.h"
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

void test_cameras_register_feed_and_defaults() {
    Cameras cameras;

    require(cameras.registerFeed("bedroomcamera", "http://camera/frame.jpg", "http://bridge/stream", "http://bridge/stream", "http://bridge/frame", "", "1080p"),
            "registerFeed should succeed for valid feed.");

    const CameraFeed feed = cameras.getFeed("bedroomcamera");
    require(feed.cameraId == "bedroomcamera", "Camera feed should be stored.");
    require(feed.modeProfile.mode == CameraMode::Home, "New camera feed should default to home mode.");
    require(feed.modeProfile.analyzers.motionDetectionEnabled, "Home mode should enable motion detection.");
    require(!feed.modeProfile.analyzers.facialRecognitionEnabled, "Home mode should not enable facial recognition by default.");
}

void test_camera_modes_change_analyzer_profile() {
    Cameras cameras;
    cameras.registerFeed("bedroomcamera", "http://camera/frame.jpg", "http://bridge/stream", "http://bridge/stream", "http://bridge/frame", "", "1080p");

    require(cameras.setMode("bedroomcamera", CameraMode::Away), "setMode should succeed for known camera.");
    const CameraFeed awayFeed = cameras.getFeed("bedroomcamera");
    require(awayFeed.modeProfile.analyzers.motionDetectionEnabled, "Away mode should enable motion detection.");
    require(awayFeed.modeProfile.analyzers.facialRecognitionEnabled, "Away mode should enable facial recognition.");
    require(awayFeed.modeProfile.analyzers.strangeSoundDetectionEnabled, "Away mode should enable strange sound detection.");

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
}

int main() {
    test_cameras_register_feed_and_defaults();
    test_camera_modes_change_analyzer_profile();
    test_api_and_camera_automation_generate_detection_events();
    test_motion_detector_uses_real_frame_bytes();

    if (failures > 0) {
        std::cerr << failures << " hub test(s) failed.\n";
        return 1;
    }

    std::cout << "All hub tests passed.\n";
    return 0;
}
