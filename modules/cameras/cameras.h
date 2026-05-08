#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum class CameraMode {
    Home,
    Away,
    Night,
    Privacy
};

struct CameraAnalyzers {
    bool rawFeedVisible{true};
    bool motionDetectionEnabled{true};
    bool facialRecognitionEnabled{false};
    bool strangeSoundDetectionEnabled{false};
};

struct CameraDetectionState {
    bool motionDetected{false};
    bool familiarFaceDetected{false};
    bool unknownFaceDetected{false};
    bool strangeSoundDetected{false};
    double motionScore{0.0};
    std::size_t lastFrameBytes{0};
    std::string lastEvent = "No events";
    std::string lastProcessedAt;
};

struct CameraModeProfile {
    CameraMode mode{CameraMode::Home};
    CameraAnalyzers analyzers{};
    std::string description{"Occupants home. Minimal alerting."};
};

struct CameraFeed {
    std::string cameraId;
    std::string sourceFeedUrl;
    std::string rawFeedUrl;
    std::string streamUrl;
    std::string frameUrl;
    std::string audioFeedUrl;
    std::string resolution;
    std::string processingStatus = "Pending";
    CameraModeProfile modeProfile{};
    CameraDetectionState detections{};
};

class Cameras {
public:
    bool registerFeed(
        const std::string& cameraId,
        const std::string& sourceFeedUrl,
        const std::string& rawFeedUrl,
        const std::string& streamUrl,
        const std::string& frameUrl,
        const std::string& audioFeedUrl,
        const std::string& resolution
    );

    [[nodiscard]] std::size_t getFeedCount() const;
    [[nodiscard]] CameraFeed getFeed(const std::string& cameraId) const;
    [[nodiscard]] std::vector<CameraFeed> getAllFeeds() const;
    bool updateProcessingStatus(const std::string& cameraId, const std::string& status);
    bool setMode(const std::string& cameraId, CameraMode mode);
    bool updateDetectionState(const std::string& cameraId, const CameraDetectionState& state);

    [[nodiscard]] static CameraModeProfile buildModeProfile(CameraMode mode);
    [[nodiscard]] static std::string modeToString(CameraMode mode);
    [[nodiscard]] static bool tryParseMode(const std::string& value, CameraMode& mode);
    [[nodiscard]] static std::string currentTimestamp();

private:
    mutable std::mutex mutex;
    std::unordered_map<std::string, CameraFeed> feeds;
};
