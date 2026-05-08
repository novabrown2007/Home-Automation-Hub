#include "cameraautomation.h"

#include <algorithm>

#include "../../logging/logger.h"

CameraAutomation::CameraAutomation(Cameras& cameras, ThreadManager& threadManager)
    : cameras(cameras), threadManager(threadManager) {}

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
        " rawFeedUrl=" + feed.rawFeedUrl +
        " streamUrl=" + feed.streamUrl +
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
    detectionState.lastEvent = "Analyzers idle";

    const std::string loweredUrl = [&]() {
        std::string value = feed.rawFeedUrl;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }();

    const CameraAnalyzers& analyzers = feed.modeProfile.analyzers;
    if (analyzers.motionDetectionEnabled && loweredUrl.find("motion") != std::string::npos) {
        detectionState.motionDetected = true;
        detectionState.lastEvent = "Motion detected";
        Logger::instance().warning("CameraAutomation", "Trigger fired for camera=" + cameraId + ": motion detected");
    }

    if (analyzers.facialRecognitionEnabled) {
        if (loweredUrl.find("unknown-face") != std::string::npos) {
            detectionState.unknownFaceDetected = true;
            detectionState.lastEvent = "Unknown face detected";
            Logger::instance().warning("CameraAutomation", "Trigger fired for camera=" + cameraId + ": unknown face detected");
        } else if (loweredUrl.find("face") != std::string::npos || loweredUrl.find("family") != std::string::npos) {
            detectionState.familiarFaceDetected = true;
            detectionState.lastEvent = "Familiar face detected";
            Logger::instance().info("CameraAutomation", "Trigger fired for camera=" + cameraId + ": familiar face detected");
        }
    }

    if (analyzers.strangeSoundDetectionEnabled && loweredUrl.find("sound-alert") != std::string::npos) {
        detectionState.strangeSoundDetected = true;
        detectionState.lastEvent = "Strange sound detected";
        Logger::instance().warning("CameraAutomation", "Trigger fired for camera=" + cameraId + ": strange sound detected");
    }

    if (!analyzers.rawFeedVisible) {
        detectionState.lastEvent = "Privacy mode active";
        Logger::instance().info("CameraAutomation", "Camera=" + cameraId + " is in privacy mode; raw feed visibility disabled");
    }

    if (detectionState.lastEvent == "Analyzers idle") {
        Logger::instance().debug("CameraAutomation", "No triggers fired for camera=" + cameraId);
    }

    cameras.updateDetectionState(cameraId, detectionState);
    cameras.updateProcessingStatus(cameraId, "Ready");
    Logger::instance().info("CameraAutomation", "Finished processing camera=" + cameraId + " with lastEvent=\"" + detectionState.lastEvent + "\"");
}
