#include "motiondetector.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include <windows.h>
#include <gdiplus.h>
#include <objidl.h>

namespace {
using namespace Gdiplus;

ULONG_PTR gdiplusToken() {
    static std::once_flag initFlag;
    static ULONG_PTR token = 0;
    std::call_once(initFlag, []() {
        GdiplusStartupInput startupInput;
        GdiplusStartup(&token, &startupInput, nullptr);
    });
    return token;
}

std::mutex& gdiplusMutex() {
    static std::mutex mutex;
    return mutex;
}
}

MotionResult MotionDetector::analyzeFrame(const std::string& cameraId, const std::string& frameBytes) {
    std::size_t decodedSampleCount = 0;
    const std::vector<std::uint8_t> decodedFrame = decodeGrayscaleFrame(frameBytes, decodedSampleCount);
    const std::string sampledFrame = decodedFrame.empty() ? sampleFrame(frameBytes) : std::string(decodedFrame.begin(), decodedFrame.end());
    MotionResult result{
        .sampledBytes = decodedFrame.empty() ? sampledFrame.size() : decodedSampleCount
    };

    std::lock_guard lock(mutex);
    if (!decodedFrame.empty()) {
        auto& previousDecoded = previousDecodedFrames[cameraId];
        if (previousDecoded.empty()) {
            previousDecoded = decodedFrame;
            return result;
        }

        const std::size_t comparableSize = std::min(previousDecoded.size(), decodedFrame.size());
        if (comparableSize == 0) {
            previousDecoded = decodedFrame;
            return result;
        }

        std::size_t totalDelta = 0;
        for (std::size_t index = 0; index < comparableSize; ++index) {
            totalDelta += static_cast<std::size_t>(std::abs(static_cast<int>(previousDecoded[index]) - static_cast<int>(decodedFrame[index])));
        }

        result.score = static_cast<double>(totalDelta) / static_cast<double>(comparableSize * 255ull);
        result.detected = result.score >= 0.08 || previousDecoded.size() != decodedFrame.size();
        previousDecoded = decodedFrame;
        return result;
    }

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

std::vector<std::uint8_t> MotionDetector::decodeGrayscaleFrame(const std::string& frameBytes, std::size_t& sampleCount) {
    sampleCount = 0;
    if (frameBytes.empty() || gdiplusToken() == 0) {
        return {};
    }

    std::lock_guard lock(gdiplusMutex());

    HGLOBAL memoryHandle = GlobalAlloc(GMEM_MOVEABLE, frameBytes.size());
    if (memoryHandle == nullptr) {
        return {};
    }

    void* memory = GlobalLock(memoryHandle);
    if (memory == nullptr) {
        GlobalFree(memoryHandle);
        return {};
    }

    std::memcpy(memory, frameBytes.data(), frameBytes.size());
    GlobalUnlock(memoryHandle);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(memoryHandle, TRUE, &stream) != S_OK) {
        GlobalFree(memoryHandle);
        return {};
    }

    Bitmap bitmap(stream, FALSE);
    stream->Release();
    if (bitmap.GetLastStatus() != Ok) {
        return {};
    }

    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    if (width == 0 || height == 0) {
        return {};
    }

    constexpr UINT targetWidth = 64;
    constexpr UINT targetHeight = 48;
    std::vector<std::uint8_t> grayscale(targetWidth * targetHeight);
    for (UINT y = 0; y < targetHeight; ++y) {
        for (UINT x = 0; x < targetWidth; ++x) {
            const UINT sourceX = std::min(width - 1, (x * width) / targetWidth);
            const UINT sourceY = std::min(height - 1, (y * height) / targetHeight);
            Color pixel{};
            if (bitmap.GetPixel(sourceX, sourceY, &pixel) != Ok) {
                return {};
            }
            const auto gray = static_cast<std::uint8_t>((static_cast<unsigned int>(pixel.GetR()) * 30u +
                static_cast<unsigned int>(pixel.GetG()) * 59u +
                static_cast<unsigned int>(pixel.GetB()) * 11u) / 100u);
            grayscale[y * targetWidth + x] = gray;
        }
    }

    sampleCount = grayscale.size();
    return grayscale;
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
