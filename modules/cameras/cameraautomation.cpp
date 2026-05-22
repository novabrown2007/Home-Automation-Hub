#include "cameraautomation.h"

#include "../../logging/logger.h"
#include "../../network/httpclient.h"

namespace {
constexpr auto kSoundAlertLatchWindow = std::chrono::seconds(8);
constexpr auto kSoundNotificationCooldown = std::chrono::seconds(12);
}

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
        " audioFeedUrl=" + feed.audioFeedUrl +
        " mode=" + Cameras::modeToString(feed.modeProfile.mode) +
        " bridgeStreamEnabled=" + std::string(feed.bridgeStreamEnabled ? "true" : "false") +
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
    detectionState.soundScore = 0.0;
    detectionState.lastFrameBytes = 0;
    detectionState.lastAudioBytes = 0;
    detectionState.lastEvent = "Analyzers idle";

    const CameraModeProfile& modeProfile = feed.modeProfile;
    const CameraAnalyzers& analyzers = modeProfile.analyzers;
    const CameraAlertPolicy& alerts = modeProfile.alerts;

    if (!analyzers.rawFeedVisible) {
        ensureBridgeCameraState(feed, false);
        detectionState.lastEvent = "Privacy mode active";
        Logger::instance().info("CameraAutomation", "Camera=" + cameraId + " privacy mode active; bridge stream disabled");
        cameras.updateDetectionState(cameraId, detectionState);
        cameras.updateProcessingStatus(cameraId, "Disabled");
        return;
    }

    ensureBridgeCameraState(feed, true);

    if (analyzers.motionDetectionEnabled) {
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
            Logger::instance().warning("CameraAutomation", "Trigger fired for camera=" + cameraId + ": motion detected");
            if (alerts.notifyOnMotion) {
                const Notification notification = notifications.enqueue(
                    "hub",
                    notificationSeverity(modeProfile, "motion"),
                    "camera",
                    "Motion detected",
                    "Motion detected for " + cameraId + ".",
                    cameraId
                );
                bridgeNotifier.publish(notification);
            }
        }
    }

    if (analyzers.strangeSoundDetectionEnabled) {
        if (feed.audioFeedUrl.empty()) {
            Logger::instance().debug("CameraAutomation", "No audioFeedUrl configured for camera=" + cameraId + "; skipping sound analysis.");
        } else {
            const HttpResponse audioResponse = HttpClient::get(feed.audioFeedUrl);
            if (!audioResponse.ok()) {
                Logger::instance().warning(
                    "CameraAutomation",
                    "Audio fetch failed for camera=" + cameraId +
                    " statusCode=" + std::to_string(audioResponse.statusCode) +
                    " statusText=" + audioResponse.statusText
                );
            } else {
                detectionState.lastAudioBytes = audioResponse.body.size();
                const SoundResult sound = soundDetector.analyzeClip(cameraId, audioResponse.body);
                detectionState.soundScore = sound.score;
                const auto now = std::chrono::steady_clock::now();
                const bool soundAlertAlreadyActive = sound.detected ? activateSoundAlert(cameraId, now) : shouldKeepSoundAlertActive(cameraId, now);
                detectionState.strangeSoundDetected = sound.detected || shouldKeepSoundAlertActive(cameraId, now);
                if (detectionState.strangeSoundDetected && feed.detections.soundScore > detectionState.soundScore) {
                    detectionState.soundScore = feed.detections.soundScore;
                }

                Logger::instance().info(
                    "CameraAutomation",
                    "Sound analysis for camera=" + cameraId +
                    " score=" + std::to_string(sound.score) +
                    " sampledBytes=" + std::to_string(sound.sampledBytes) +
                    " detected=" + std::string(sound.detected ? "true" : "false") +
                    " latched=" + std::string(detectionState.strangeSoundDetected ? "true" : "false") +
                    (sound.error.empty() ? "" : " error=" + sound.error)
                );

                if (!sound.error.empty()) {
                    detectionState.lastEvent = "Sound analyzer unavailable";
                } else if (sound.detected) {
                    detectionState.lastEvent = "Strange sound detected";
                    Logger::instance().warning("CameraAutomation", "Trigger fired for camera=" + cameraId + ": strange sound detected");
                    if (!soundAlertAlreadyActive && alerts.notifyOnStrangeSound && shouldPublishSoundNotification(cameraId, now)) {
                        const Notification notification = notifications.enqueue(
                            "hub",
                            notificationSeverity(modeProfile, "sound"),
                            "camera",
                            "Strange sound detected",
                            "A sound anomaly was detected for " + cameraId + ".",
                            cameraId
                        );
                        bridgeNotifier.publish(notification);
                    }
                } else if (detectionState.strangeSoundDetected) {
                    detectionState.lastEvent = "Strange sound detected";
                    Logger::instance().debug("CameraAutomation", "Sound alert remains latched for camera=" + cameraId);
                }
            }
        }
    }

    if (analyzers.facialRecognitionEnabled) {
        Logger::instance().info(
            "CameraAutomation",
            "Facial recognition requested for camera=" + cameraId + " but no native recognizer is configured yet."
        );
    }

    if (detectionState.lastEvent == "Analyzers idle") {
        detectionState.lastEvent = "Monitoring";
        Logger::instance().debug("CameraAutomation", "No triggers fired for camera=" + cameraId);
    }

    cameras.updateDetectionState(cameraId, detectionState);
    cameras.updateProcessingStatus(cameraId, "Ready");
    Logger::instance().info("CameraAutomation", "Finished processing camera=" + cameraId + " with lastEvent=\"" + detectionState.lastEvent + "\"");
}

void CameraAutomation::ensureBridgeCameraState(const CameraFeed& feed, bool shouldBeStreaming) {
    if (feed.bridgeStreamEnabled == shouldBeStreaming) {
        return;
    }

    cameras.updateBridgeStreamEnabled(feed.cameraId, shouldBeStreaming);
    Logger::instance().debug(
        "CameraAutomation",
        "Compatibility camera stream flag updated camera=" + feed.cameraId +
        "; Bridge stream lifecycle is now driven by Hub Protocol metadata."
    );
}

std::string CameraAutomation::notificationSeverity(const CameraModeProfile& modeProfile, const std::string& category) const {
    if (modeProfile.mode == CameraMode::Night && (category == "motion" || category == "sound" || category == "unknown-face")) {
        return "critical";
    }
    return category == "sound" ? "warning" : "warning";
}

bool CameraAutomation::shouldKeepSoundAlertActive(const std::string& cameraId, std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(soundStateMutex);
    const auto it = soundAlertActiveUntil.find(cameraId);
    return it != soundAlertActiveUntil.end() && now < it->second;
}

bool CameraAutomation::shouldPublishSoundNotification(const std::string& cameraId, std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(soundStateMutex);
    const auto it = soundNotificationCooldownUntil.find(cameraId);
    if (it != soundNotificationCooldownUntil.end() && now < it->second) {
        return false;
    }

    soundNotificationCooldownUntil[cameraId] = now + kSoundNotificationCooldown;
    return true;
}

bool CameraAutomation::activateSoundAlert(const std::string& cameraId, std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(soundStateMutex);
    const auto current = soundAlertActiveUntil.find(cameraId);
    const bool alreadyActive = current != soundAlertActiveUntil.end() && now < current->second;
    soundAlertActiveUntil[cameraId] = now + kSoundAlertLatchWindow;
    return alreadyActive;
}
