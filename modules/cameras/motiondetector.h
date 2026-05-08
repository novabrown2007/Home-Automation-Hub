#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

struct MotionResult {
    bool detected{false};
    double score{0.0};
    std::size_t sampledBytes{0};
};

class MotionDetector {
public:
    MotionResult analyzeFrame(const std::string& cameraId, const std::string& frameBytes);

private:
    static std::string sampleFrame(const std::string& frameBytes);

    std::mutex mutex;
    std::unordered_map<std::string, std::string> previousFrames;
};
