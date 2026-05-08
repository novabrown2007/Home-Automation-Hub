#pragma once

#include <string>

#include "../../notifications/bridgenotifier.h"
#include "../../notifications/notifications.h"
#include "../../threading/threadmanager.h"
#include "motiondetector.h"
#include "cameras.h"

class CameraAutomation {
public:
    CameraAutomation(Cameras& cameras, ThreadManager& threadManager, NotificationCenter& notifications, BridgeNotifier& bridgeNotifier);

    void scheduleFeedProcessing(const std::string& cameraId);
    void processAllFeeds();

private:
    void processFeed(const std::string& cameraId);

    Cameras& cameras;
    ThreadManager& threadManager;
    NotificationCenter& notifications;
    BridgeNotifier& bridgeNotifier;
    MotionDetector motionDetector;
};
