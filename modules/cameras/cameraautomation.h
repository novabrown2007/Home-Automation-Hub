#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../../notifications/bridgenotifier.h"
#include "../../notifications/notifications.h"
#include "../../threading/threadmanager.h"
#include "cameras.h"
#include "motiondetector.h"
#include "sounddetector.h"

class CameraAutomation {
public:
    CameraAutomation(Cameras& cameras, ThreadManager& threadManager, NotificationCenter& notifications, BridgeNotifier& bridgeNotifier);

    void scheduleFeedProcessing(const std::string& cameraId);
    void processAllFeeds();

private:
    void processFeed(const std::string& cameraId);
    void ensureBridgeCameraState(const CameraFeed& feed, bool shouldBeStreaming);
    std::string notificationSeverity(const CameraModeProfile& modeProfile, const std::string& category) const;
    bool shouldKeepSoundAlertActive(const std::string& cameraId, std::chrono::steady_clock::time_point now);
    bool shouldPublishSoundNotification(const std::string& cameraId, std::chrono::steady_clock::time_point now);
    bool activateSoundAlert(const std::string& cameraId, std::chrono::steady_clock::time_point now);

    Cameras& cameras;
    ThreadManager& threadManager;
    NotificationCenter& notifications;
    BridgeNotifier& bridgeNotifier;
    MotionDetector motionDetector;
    SoundDetector soundDetector;
    std::mutex soundStateMutex;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> soundAlertActiveUntil;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> soundNotificationCooldownUntil;
};
