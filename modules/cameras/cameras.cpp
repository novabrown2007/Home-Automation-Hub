#include "cameras.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "../../logging/logger.h"

namespace {
std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}

bool Cameras::registerFeed(
    const std::string& cameraId,
    const std::string& sourceFeedUrl,
    const std::string& rawFeedUrl,
    const std::string& streamUrl,
    const std::string& frameUrl,
    const std::string& audioFeedUrl,
    const std::string& resolution
) {
    if (cameraId.empty() || frameUrl.empty()) {
        Logger::instance().warning("Cameras", "Rejected camera feed registration because cameraId or frameUrl was empty.");
        return false;
    }

    std::lock_guard lock(mutex);
    feeds[cameraId] = CameraFeed{
        .cameraId = cameraId,
        .sourceFeedUrl = sourceFeedUrl,
        .rawFeedUrl = rawFeedUrl,
        .streamUrl = streamUrl.empty() ? rawFeedUrl : streamUrl,
        .frameUrl = frameUrl,
        .audioFeedUrl = audioFeedUrl,
        .resolution = resolution,
        .processingStatus = "Registered",
        .bridgeStreamEnabled = true,
        .modeProfile = buildModeProfile(CameraMode::Home),
        .detections = CameraDetectionState{
            .lastProcessedAt = currentTimestamp(),
        }
    };
    Logger::instance().info(
        "Cameras",
        "Registered feed for camera=" + cameraId +
        " sourceFeedUrl=" + sourceFeedUrl +
        " rawFeedUrl=" + rawFeedUrl +
        " streamUrl=" + (streamUrl.empty() ? rawFeedUrl : streamUrl) +
        " frameUrl=" + frameUrl +
        " audioFeedUrl=" + audioFeedUrl +
        " resolution=" + resolution
    );
    return true;
}

std::size_t Cameras::getFeedCount() const {
    std::lock_guard lock(mutex);
    return feeds.size();
}

CameraFeed Cameras::getFeed(const std::string& cameraId) const {
    std::lock_guard lock(mutex);
    const auto it = feeds.find(cameraId);
    if (it == feeds.end()) {
        return {};
    }
    return it->second;
}

std::vector<CameraFeed> Cameras::getAllFeeds() const {
    std::lock_guard lock(mutex);
    std::vector<CameraFeed> result;
    result.reserve(feeds.size());
    for (const auto& [_, feed] : feeds) {
        result.push_back(feed);
    }
    return result;
}

bool Cameras::updateProcessingStatus(const std::string& cameraId, const std::string& status) {
    std::lock_guard lock(mutex);
    const auto it = feeds.find(cameraId);
    if (it == feeds.end()) {
        Logger::instance().warning("Cameras", "Tried to update processing status for unknown camera=" + cameraId);
        return false;
    }

    Logger::instance().debug(
        "Cameras",
        "Camera=" + cameraId +
        " processingStatus: " + it->second.processingStatus +
        " -> " + status
    );
    it->second.processingStatus = status;
    return true;
}

bool Cameras::setMode(const std::string& cameraId, CameraMode mode) {
    std::lock_guard lock(mutex);
    const auto it = feeds.find(cameraId);
    if (it == feeds.end()) {
        Logger::instance().warning("Cameras", "Tried to set mode for unknown camera=" + cameraId);
        return false;
    }

    it->second.modeProfile = buildModeProfile(mode);
    it->second.detections.lastEvent = "Mode updated to " + modeToString(mode);
    it->second.detections.lastProcessedAt = currentTimestamp();
    Logger::instance().info(
        "Cameras",
        "Camera=" + cameraId +
        " mode set to " + modeToString(mode) +
        " description=\"" + it->second.modeProfile.description + "\""
    );
    return true;
}

bool Cameras::updateDetectionState(const std::string& cameraId, const CameraDetectionState& state) {
    std::lock_guard lock(mutex);
    const auto it = feeds.find(cameraId);
    if (it == feeds.end()) {
        Logger::instance().warning("Cameras", "Tried to update detection state for unknown camera=" + cameraId);
        return false;
    }

    it->second.detections = state;
    Logger::instance().info(
        "Cameras",
        "Camera=" + cameraId +
        " detectionState motion=" + std::string(state.motionDetected ? "true" : "false") +
        " motionScore=" + std::to_string(state.motionScore) +
        " soundScore=" + std::to_string(state.soundScore) +
        " lastFrameBytes=" + std::to_string(state.lastFrameBytes) +
        " lastAudioBytes=" + std::to_string(state.lastAudioBytes) +
        " familiarFace=" + std::string(state.familiarFaceDetected ? "true" : "false") +
        " unknownFace=" + std::string(state.unknownFaceDetected ? "true" : "false") +
        " strangeSound=" + std::string(state.strangeSoundDetected ? "true" : "false") +
        " lastEvent=\"" + state.lastEvent + "\"" +
        " processedAt=" + state.lastProcessedAt
    );
    return true;
}

bool Cameras::updateBridgeStreamEnabled(const std::string& cameraId, bool enabled) {
    std::lock_guard lock(mutex);
    const auto it = feeds.find(cameraId);
    if (it == feeds.end()) {
        Logger::instance().warning("Cameras", "Tried to update bridge stream state for unknown camera=" + cameraId);
        return false;
    }

    it->second.bridgeStreamEnabled = enabled;
    Logger::instance().info(
        "Cameras",
        "Camera=" + cameraId + " bridgeStreamEnabled=" + std::string(enabled ? "true" : "false")
    );
    return true;
}

CameraModeProfile Cameras::buildModeProfile(CameraMode mode) {
    switch (mode) {
        case CameraMode::Away:
            return CameraModeProfile{
                .mode = mode,
                .analyzers = CameraAnalyzers{
                    .rawFeedVisible = true,
                    .motionDetectionEnabled = true,
                    .facialRecognitionEnabled = true,
                    .strangeSoundDetectionEnabled = true,
                },
                .alerts = CameraAlertPolicy{
                    .notifyOnMotion = true,
                    .notifyOnFamiliarFace = true,
                    .notifyOnUnknownFace = true,
                    .notifyOnStrangeSound = true,
                },
                .description = "No one expected home. Full monitoring and alerting.",
            };
        case CameraMode::Night:
            return CameraModeProfile{
                .mode = mode,
                .analyzers = CameraAnalyzers{
                    .rawFeedVisible = true,
                    .motionDetectionEnabled = true,
                    .facialRecognitionEnabled = true,
                    .strangeSoundDetectionEnabled = true,
                },
                .alerts = CameraAlertPolicy{
                    .notifyOnMotion = true,
                    .notifyOnFamiliarFace = false,
                    .notifyOnUnknownFace = true,
                    .notifyOnStrangeSound = true,
                },
                .description = "Overnight monitoring with alerts for motion, unknown faces, and strange sounds.",
            };
        case CameraMode::Privacy:
            return CameraModeProfile{
                .mode = mode,
                .analyzers = CameraAnalyzers{
                    .rawFeedVisible = false,
                    .motionDetectionEnabled = false,
                    .facialRecognitionEnabled = false,
                    .strangeSoundDetectionEnabled = false,
                },
                .alerts = CameraAlertPolicy{},
                .description = "Camera turned off and analyzers disabled for privacy.",
            };
        case CameraMode::Home:
        default:
            return CameraModeProfile{
                .mode = CameraMode::Home,
                .analyzers = CameraAnalyzers{
                    .rawFeedVisible = true,
                    .motionDetectionEnabled = true,
                    .facialRecognitionEnabled = true,
                    .strangeSoundDetectionEnabled = true,
                },
                .alerts = CameraAlertPolicy{
                    .notifyOnMotion = false,
                    .notifyOnFamiliarFace = false,
                    .notifyOnUnknownFace = false,
                    .notifyOnStrangeSound = false,
                },
                .description = "Occupants expected home. Suppress alerts but continue logging faces and anomalies.",
            };
    }
}

std::string Cameras::modeToString(CameraMode mode) {
    switch (mode) {
        case CameraMode::Away:
            return "away";
        case CameraMode::Night:
            return "night";
        case CameraMode::Privacy:
            return "privacy";
        case CameraMode::Home:
        default:
            return "home";
    }
}

bool Cameras::tryParseMode(const std::string& value, CameraMode& mode) {
    const std::string normalized = lowercase(value);
    if (normalized == "home") {
        mode = CameraMode::Home;
        return true;
    }
    if (normalized == "away") {
        mode = CameraMode::Away;
        return true;
    }
    if (normalized == "night") {
        mode = CameraMode::Night;
        return true;
    }
    if (normalized == "privacy") {
        mode = CameraMode::Privacy;
        return true;
    }
    return false;
}

std::string Cameras::currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    std::ostringstream buffer;
    buffer << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return buffer.str();
}
