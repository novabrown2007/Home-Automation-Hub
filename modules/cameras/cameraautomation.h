#pragma once

#include <string>

#include "../../threading/threadmanager.h"
#include "cameras.h"

class CameraAutomation {
public:
    CameraAutomation(Cameras& cameras, ThreadManager& threadManager);

    void scheduleFeedProcessing(const std::string& cameraId);
    void processAllFeeds();

private:
    void processFeed(const std::string& cameraId);

    Cameras& cameras;
    ThreadManager& threadManager;
};
