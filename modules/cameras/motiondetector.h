#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct MotionResult {
    bool detected{false};
    double score{0.0};
    std::size_t sampledBytes{0};
};

class MotionDetector {
public:
    MotionResult analyzeFrame(const std::string& cameraId, const std::string& frameBytes);

private:
    static std::vector<std::uint8_t> decodeGrayscaleFrame(const std::string& frameBytes, std::size_t& sampleCount);
    static std::string sampleFrame(const std::string& frameBytes);

    std::mutex mutex;
    std::unordered_map<std::string, std::string> previousFrames;
    std::unordered_map<std::string, std::vector<std::uint8_t>> previousDecodedFrames;
};
