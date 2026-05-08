#include "motiondetector.h"

#include <algorithm>

MotionResult MotionDetector::analyzeFrame(const std::string& cameraId, const std::string& frameBytes) {
    const std::string sampledFrame = sampleFrame(frameBytes);
    MotionResult result{
        .sampledBytes = sampledFrame.size()
    };

    std::lock_guard lock(mutex);
    auto& previousFrame = previousFrames[cameraId];
    if (previousFrame.empty()) {
        previousFrame = sampledFrame;
        return result;
    }

    const std::size_t comparableSize = std::min(previousFrame.size(), sampledFrame.size());
    if (comparableSize == 0) {
        previousFrame = sampledFrame;
        return result;
    }

    std::size_t differentBytes = 0;
    for (std::size_t index = 0; index < comparableSize; ++index) {
        if (previousFrame[index] != sampledFrame[index]) {
            ++differentBytes;
        }
    }

    result.score = static_cast<double>(differentBytes) / static_cast<double>(comparableSize);
    result.detected = result.score >= 0.12 || previousFrame.size() != sampledFrame.size();
    previousFrame = sampledFrame;
    return result;
}

std::string MotionDetector::sampleFrame(const std::string& frameBytes) {
    if (frameBytes.empty()) {
        return {};
    }

    constexpr std::size_t maxSamples = 4096;
    const std::size_t step = std::max<std::size_t>(1, frameBytes.size() / maxSamples);

    std::string sampled;
    sampled.reserve(std::min(frameBytes.size(), maxSamples));
    for (std::size_t index = 0; index < frameBytes.size(); index += step) {
        sampled.push_back(frameBytes[index]);
    }
    return sampled;
}
