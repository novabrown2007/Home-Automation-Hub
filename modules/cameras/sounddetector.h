#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct SoundResult {
    bool detected{false};
    double score{0.0};
    std::size_t sampledBytes{0};
    std::string error;
};

class SoundDetector {
public:
    SoundResult analyzeClip(const std::string& cameraId, const std::string& audioBytes);

private:
    struct WavPcmData {
        int sampleRate{0};
        int channels{0};
        int bitsPerSample{0};
        std::vector<int16_t> samples;
    };

    static std::optional<WavPcmData> decodeWavPcm(const std::string& audioBytes, std::string& error);
    static double computeNormalizedRms(const std::vector<int16_t>& samples);

    std::mutex mutex;
    std::unordered_map<std::string, double> previousLevels;
};
