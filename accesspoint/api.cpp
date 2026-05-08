#include "api.h"

#include "../logging/logger.h"

std::string API::getStatus() const {
    return "Home Automation Hub";
}

API::API(Cameras& cameras, CameraAutomation& cameraAutomation, NotificationCenter& notifications)
    : cameras(cameras), cameraAutomation(cameraAutomation), notifications(notifications) {
    Logger::instance().info("API", "Hub API initialized.");
}

bool API::registerCameraFeed(
    const std::string& cameraId,
    const std::string& sourceFeedUrl,
    const std::string& rawFeedUrl,
    const std::string& streamUrl,
    const std::string& frameUrl,
    const std::string& audioFeedUrl,
    const std::string& resolution
) {
    Logger::instance().info(
        "API",
        "Register camera feed requested camera=" + cameraId +
        " sourceFeedUrl=" + sourceFeedUrl +
        " rawFeedUrl=" + rawFeedUrl +
        " streamUrl=" + streamUrl +
        " frameUrl=" + frameUrl +
        " audioFeedUrl=" + audioFeedUrl +
        " resolution=" + resolution
    );
    if (!cameras.registerFeed(cameraId, sourceFeedUrl, rawFeedUrl, streamUrl, frameUrl, audioFeedUrl, resolution)) {
        Logger::instance().warning("API", "Camera feed registration rejected for camera=" + cameraId);
        return false;
    }

    notifications.enqueue("hub", "info", "camera", "Camera feed registered", "Camera feed registered for processing.", cameraId);
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

    notifications.enqueue("hub", "info", "camera", "Camera mode updated", "Camera mode updated to " + mode + ".", cameraId);
    cameraAutomation.scheduleFeedProcessing(cameraId);
    Logger::instance().info("API", "Mode updated for camera=" + cameraId + " mode=" + mode);
    return true;
}

Notification API::queueNotification(
    const std::string& source,
    const std::string& severity,
    const std::string& category,
    const std::string& title,
    const std::string& message,
    const std::string& deviceId
) {
    return notifications.enqueue(source, severity, category, title, message, deviceId);
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

std::vector<Notification> API::getNotifications() const {
    return notifications.list();
}
