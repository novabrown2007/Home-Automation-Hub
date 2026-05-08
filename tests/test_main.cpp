#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "../accesspoint/api.h"
#include "../logging/logger.h"
#include "../modules/cameras/cameraautomation.h"
#include "../modules/cameras/cameras.h"
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

    require(cameras.registerFeed("bedroomcamera", "http://camera/motion-face", "http://camera/motion-face", "1080p"),
            "registerFeed should succeed for valid feed.");

    const CameraFeed feed = cameras.getFeed("bedroomcamera");
    require(feed.cameraId == "bedroomcamera", "Camera feed should be stored.");
    require(feed.modeProfile.mode == CameraMode::Home, "New camera feed should default to home mode.");
    require(feed.modeProfile.analyzers.motionDetectionEnabled, "Home mode should enable motion detection.");
    require(!feed.modeProfile.analyzers.facialRecognitionEnabled, "Home mode should not enable facial recognition by default.");
}

void test_camera_modes_change_analyzer_profile() {
    Cameras cameras;
    cameras.registerFeed("bedroomcamera", "http://camera/feed", "http://camera/feed", "1080p");

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
    CameraAutomation automation(cameras, threadManager);
    API api(cameras, automation);

    require(api.registerCameraFeed(
                "bedroomcamera",
                "http://camera/unknown-face-sound-alert-motion",
                "http://camera/unknown-face-sound-alert-motion",
                "1080p"),
            "API registerCameraFeed should succeed.");

    require(api.setCameraMode("bedroomcamera", "away"), "API setCameraMode should accept away mode.");

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const CameraFeed feed = api.getCameraFeed("bedroomcamera");
    require(feed.cameraId == "bedroomcamera", "API should return stored camera feed.");
    require(feed.processingStatus == "Ready", "Camera processing should eventually mark feed as Ready.");
    require(feed.detections.motionDetected, "Processing should record motion detection.");
    require(feed.detections.unknownFaceDetected, "Processing should record unknown face detection.");
    require(feed.detections.strangeSoundDetected, "Processing should record strange sound detection.");
    require(!feed.detections.lastProcessedAt.empty(), "Processing should stamp lastProcessedAt.");

    threadManager.stop();
}
}

int main() {
    test_cameras_register_feed_and_defaults();
    test_camera_modes_change_analyzer_profile();
    test_api_and_camera_automation_generate_detection_events();

    if (failures > 0) {
        std::cerr << failures << " hub test(s) failed.\n";
        return 1;
    }

    std::cout << "All hub tests passed.\n";
    return 0;
}
