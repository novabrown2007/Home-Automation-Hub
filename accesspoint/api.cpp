#include "api.h"

#include "../logging/logger.h"

std::string API::getStatus() const {
    return "Home Automation Hub";
}

API::API(Cameras& cameras, CameraAutomation& cameraAutomation)
    : cameras(cameras), cameraAutomation(cameraAutomation) {
    Logger::instance().info("API", "Hub API initialized.");
}

bool API::registerCameraFeed(
    const std::string& cameraId,
    const std::string& rawFeedUrl,
    const std::string& streamUrl,
    const std::string& resolution
) {
    Logger::instance().info(
        "API",
        "Register camera feed requested camera=" + cameraId +
        " rawFeedUrl=" + rawFeedUrl +
        " streamUrl=" + streamUrl +
        " resolution=" + resolution
    );
    if (!cameras.registerFeed(cameraId, rawFeedUrl, streamUrl, resolution)) {
        Logger::instance().warning("API", "Camera feed registration rejected for camera=" + cameraId);
        return false;
    }

    cameraAutomation.scheduleFeedProcessing(cameraId);
    Logger::instance().info("API", "Camera feed registration accepted for camera=" + cameraId);
    return true;
}

bool API::setCameraMode(const std::string& cameraId, const std::string& mode) {
    Logger::instance().info("API", "Set camera mode requested camera=" + cameraId + " mode=" + mode);
    CameraMode parsedMode{};
    if (!Cameras::tryParseMode(mode, parsedMode)) {
        Logger::instance().warning("API", "Invalid mode supplied for camera=" + cameraId + " mode=" + mode);
        return false;
    }

    if (!cameras.setMode(cameraId, parsedMode)) {
        Logger::instance().warning("API", "Failed to set mode for camera=" + cameraId + " mode=" + mode);
        return false;
    }

    cameraAutomation.scheduleFeedProcessing(cameraId);
    Logger::instance().info("API", "Mode updated for camera=" + cameraId + " mode=" + mode);
    return true;
}

std::size_t API::getRegisteredCameraFeedCount() const {
    return cameras.getFeedCount();
}

CameraFeed API::getCameraFeed(const std::string& cameraId) const {
    return cameras.getFeed(cameraId);
}

std::vector<CameraFeed> API::getAllCameraFeeds() const {
    return cameras.getAllFeeds();
}
