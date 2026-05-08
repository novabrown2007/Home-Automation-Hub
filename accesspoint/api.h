#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../modules/cameras/cameraautomation.h"
#include "../modules/cameras/cameras.h"
#include "../notifications/notifications.h"

class API {
public:
    API(Cameras& cameras, CameraAutomation& cameraAutomation, NotificationCenter& notifications);

    [[nodiscard]] std::string getStatus() const;
    bool registerCameraFeed(
        const std::string& cameraId,
        const std::string& sourceFeedUrl,
        const std::string& rawFeedUrl,
        const std::string& streamUrl,
        const std::string& frameUrl,
        const std::string& audioFeedUrl,
        const std::string& resolution
    );
    bool setCameraMode(const std::string& cameraId, const std::string& mode);
    Notification queueNotification(
        const std::string& source,
        const std::string& severity,
        const std::string& category,
        const std::string& title,
        const std::string& message,
        const std::string& deviceId = ""
    );
    [[nodiscard]] std::size_t getRegisteredCameraFeedCount() const;
    [[nodiscard]] CameraFeed getCameraFeed(const std::string& cameraId) const;
    [[nodiscard]] std::vector<CameraFeed> getAllCameraFeeds() const;
    [[nodiscard]] std::vector<Notification> getNotifications() const;

private:
    Cameras& cameras;
    CameraAutomation& cameraAutomation;
    NotificationCenter& notifications;
};
