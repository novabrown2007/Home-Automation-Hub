#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../modules/cameras/cameraautomation.h"
#include "../modules/cameras/cameras.h"

class API {
public:
    API(Cameras& cameras, CameraAutomation& cameraAutomation);

    [[nodiscard]] std::string getStatus() const;
    bool registerCameraFeed(
        const std::string& cameraId,
        const std::string& rawFeedUrl,
        const std::string& streamUrl,
        const std::string& resolution
    );
    bool setCameraMode(const std::string& cameraId, const std::string& mode);
    [[nodiscard]] std::size_t getRegisteredCameraFeedCount() const;
    [[nodiscard]] CameraFeed getCameraFeed(const std::string& cameraId) const;
    [[nodiscard]] std::vector<CameraFeed> getAllCameraFeeds() const;

private:
    Cameras& cameras;
    CameraAutomation& cameraAutomation;
};
