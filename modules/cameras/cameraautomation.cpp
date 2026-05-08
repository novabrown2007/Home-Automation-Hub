#include "cameraautomation.h"

#include "../../logging/logger.h"
#include "../../network/httpclient.h"

CameraAutomation::CameraAutomation(Cameras& cameras, ThreadManager& threadManager, NotificationCenter& notifications, BridgeNotifier& bridgeNotifier)
    : cameras(cameras), threadManager(threadManager), notifications(notifications), bridgeNotifier(bridgeNotifier) {}

void CameraAutomation::scheduleFeedProcessing(const std::string& cameraId) {
    Logger::instance().debug("CameraAutomation", "Queueing camera feed processing for camera=" + cameraId);
    cameras.updateProcessingStatus(cameraId, "Queued");

    threadManager.submitEventTask("camera-feed-" + cameraId, [this, cameraId]() {
        processFeed(cameraId);
    });
}

void CameraAutomation::processAllFeeds() {
    Logger::instance().debug("CameraAutomation", "Starting background processing sweep for all camera feeds.");
    for (const CameraFeed& feed : cameras.getAllFeeds()) {
        scheduleFeedProcessing(feed.cameraId);
    }
}

void CameraAutomation::processFeed(const std::string& cameraId) {
    cameras.updateProcessingStatus(cameraId, "Processing");

    const CameraFeed feed = cameras.getFeed(cameraId);
    if (feed.cameraId.empty()) {
        Logger::instance().warning("CameraAutomation", "Skipping processing for unknown camera=" + cameraId);
        return;
    }

    Logger::instance().info(
        "CameraAutomation",
        "Processing camera=" + cameraId +
        " sourceFeedUrl=" + feed.sourceFeedUrl +
        " rawFeedUrl=" + feed.rawFeedUrl +
        " streamUrl=" + feed.streamUrl +
        " frameUrl=" + feed.frameUrl +
        " mode=" + Cameras::modeToString(feed.modeProfile.mode) +
        " rawFeedVisible=" + std::string(feed.modeProfile.analyzers.rawFeedVisible ? "true" : "false") +
        " motionDetectionEnabled=" + std::string(feed.modeProfile.analyzers.motionDetectionEnabled ? "true" : "false") +
        " facialRecognitionEnabled=" + std::string(feed.modeProfile.analyzers.facialRecognitionEnabled ? "true" : "false") +
        " strangeSoundDetectionEnabled=" + std::string(feed.modeProfile.analyzers.strangeSoundDetectionEnabled ? "true" : "false")
    );

    CameraDetectionState detectionState = feed.detections;
    detectionState.lastProcessedAt = Cameras::currentTimestamp();
    detectionState.motionDetected = false;
    detectionState.familiarFaceDetected = false;
    detectionState.unknownFaceDetected = false;
    detectionState.strangeSoundDetected = false;
    detectionState.motionScore = 0.0;
    detectionState.lastFrameBytes = 0;
    detectionState.lastEvent = "Analyzers idle";

    const CameraAnalyzers& analyzers = feed.modeProfile.analyzers;
    if (!analyzers.rawFeedVisible) {
        detectionState.lastEvent = "Privacy mode active";
        Logger::instance().info("CameraAutomation", "Camera=" + cameraId + " is in privacy mode; raw feed visibility disabled");
        cameras.updateDetectionState(cameraId, detectionState);
        cameras.updateProcessingStatus(cameraId, "Ready");
        return;
    }

    const HttpResponse frameResponse = HttpClient::get(feed.frameUrl);
    if (!frameResponse.ok()) {
        detectionState.lastEvent = "Frame fetch failed";
        cameras.updateDetectionState(cameraId, detectionState);
        cameras.updateProcessingStatus(cameraId, "Capture Failed");

        const Notification notification = notifications.enqueue(
            "hub",
            "warning",
            "camera",
            "Camera frame fetch failed",
            "The hub could not fetch the latest frame from the bridge.",
            cameraId
        );
        bridgeNotifier.publish(notification);
        Logger::instance().warning(
            "CameraAutomation",
            "Frame fetch failed for camera=" + cameraId +
            " statusCode=" + std::to_string(frameResponse.statusCode) +
            " statusText=" + frameResponse.statusText
        );
        return;
    }

    detectionState.lastFrameBytes = frameResponse.body.size();

    if (analyzers.motionDetectionEnabled) {
        const MotionResult motion = motionDetector.analyzeFrame(cameraId, frameResponse.body);
        detectionState.motionScore = motion.score;
        detectionState.motionDetected = motion.detected;

        Logger::instance().info(
            "CameraAutomation",
            "Motion analysis for camera=" + cameraId +
            " score=" + std::to_string(motion.score) +
            " sampledBytes=" + std::to_string(motion.sampledBytes) +
            " detected=" + std::string(motion.detected ? "true" : "false")
        );

        if (motion.detected) {
            detectionState.lastEvent = "Motion detected";
            const Notification notification = notifications.enqueue(
                "hub",
                "warning",
                "camera",
                "Motion detected",
                "Motion detected for " + cameraId + ".",
                cameraId
            );
            bridgeNotifier.publish(notification);
            Logger::instance().warning("CameraAutomation", "Trigger fired for camera=" + cameraId + ": motion detected");
        }
    }

    if (detectionState.lastEvent == "Analyzers idle") {
        detectionState.lastEvent = "Monitoring";
        Logger::instance().debug("CameraAutomation", "No triggers fired for camera=" + cameraId);
    }

    cameras.updateDetectionState(cameraId, detectionState);
    cameras.updateProcessingStatus(cameraId, "Ready");
    Logger::instance().info("CameraAutomation", "Finished processing camera=" + cameraId + " with lastEvent=\"" + detectionState.lastEvent + "\"");
}
