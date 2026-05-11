#include "sounddetector.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr double kMinimumAbsoluteLevel = 0.15;
constexpr double kRelativeSpikeThreshold = 1.8;

uint16_t readLe16(const unsigned char* bytes) {
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t readLe32(const unsigned char* bytes) {
    return static_cast<uint32_t>(bytes[0] |
        (bytes[1] << 8) |
        (bytes[2] << 16) |
        (bytes[3] << 24));
}
}

SoundResult SoundDetector::analyzeClip(const std::string& cameraId, const std::string& audioBytes) {
    SoundResult result{
        .sampledBytes = audioBytes.size()
    };
    if (audioBytes.empty()) {
        result.error = "audio payload was empty";
        return result;
    }

    std::string error;
    const std::optional<WavPcmData> decoded = decodeWavPcm(audioBytes, error);
    if (!decoded.has_value()) {
        result.error = error;
        return result;
    }

    result.score = computeNormalizedRms(decoded->samples);

    std::lock_guard lock(mutex);
    double& previousLevel = previousLevels[cameraId];
    if (previousLevel <= 0.0) {
        previousLevel = result.score;
        result.detected = result.score >= kMinimumAbsoluteLevel;
        return result;
    }

    const double ratio = previousLevel > 0.0 ? result.score / previousLevel : 0.0;
    result.detected = result.score >= kMinimumAbsoluteLevel && ratio >= kRelativeSpikeThreshold;
    previousLevel = (previousLevel * 0.7) + (result.score * 0.3);
    return result;
}

std::optional<SoundDetector::WavPcmData> SoundDetector::decodeWavPcm(const std::string& audioBytes, std::string& error) {
    if (audioBytes.size() < 44) {
        error = "audio clip too small to be a WAV file";
        return std::nullopt;
    }

    const auto* bytes = reinterpret_cast<const unsigned char*>(audioBytes.data());
    if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0) {
        error = "audio clip is not a RIFF/WAVE stream";
        return std::nullopt;
    }

    std::size_t offset = 12;
    int channels = 0;
    int bitsPerSample = 0;
    int sampleRate = 0;
    const unsigned char* dataChunk = nullptr;
    std::size_t dataSize = 0;

    while (offset + 8 <= audioBytes.size()) {
        const unsigned char* chunk = bytes + offset;
        const uint32_t chunkSize = readLe32(chunk + 4);
        const std::size_t nextOffset = offset + 8ull + chunkSize + (chunkSize % 2ull);
        if (nextOffset > audioBytes.size()) {
            break;
        }

        if (std::memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16) {
            const uint16_t audioFormat = readLe16(chunk + 8);
            channels = static_cast<int>(readLe16(chunk + 10));
            sampleRate = static_cast<int>(readLe32(chunk + 12));
            bitsPerSample = static_cast<int>(readLe16(chunk + 22));
            if (audioFormat != 1) {
                error = "only PCM WAV audio is supported";
                return std::nullopt;
            }
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataChunk = chunk + 8;
            dataSize = chunkSize;
        }

        offset = nextOffset;
    }

    if (channels <= 0 || sampleRate <= 0 || (bitsPerSample != 8 && bitsPerSample != 16) || dataChunk == nullptr || dataSize == 0) {
        error = "wav header missing PCM format details or audio data chunk";
        return std::nullopt;
    }

    WavPcmData decoded{
        .sampleRate = sampleRate,
        .channels = channels,
        .bitsPerSample = bitsPerSample
    };

    if (bitsPerSample == 8) {
        decoded.samples.reserve(dataSize);
        for (std::size_t index = 0; index < dataSize; ++index) {
            decoded.samples.push_back(static_cast<int16_t>((static_cast<int>(dataChunk[index]) - 128) << 8));
        }
    } else {
        decoded.samples.reserve(dataSize / 2);
        for (std::size_t index = 0; index + 1 < dataSize; index += 2) {
            decoded.samples.push_back(static_cast<int16_t>(readLe16(dataChunk + index)));
        }
    }

    if (decoded.samples.empty()) {
        error = "wav file did not contain decodable PCM samples";
        return std::nullopt;
    }

    return decoded;
}

double SoundDetector::computeNormalizedRms(const std::vector<int16_t>& samples) {
    if (samples.empty()) {
        return 0.0;
    }

    long double sumSquares = 0.0;
    for (const int16_t sample : samples) {
        const long double normalized = static_cast<long double>(sample) / 32768.0L;
        sumSquares += normalized * normalized;
    }

    const long double meanSquares = sumSquares / static_cast<long double>(samples.size());
    return std::sqrt(static_cast<double>(meanSquares));
}
