#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
struct StreamMetadata {
    std::string streamId;
    std::string deviceId;
    std::string streamType;
    std::string codec;
    std::string resolution;
    std::string endpoint;
    double fps{0.0};
    Json source = Json::object();
    Json metadata = Json::object();
    std::int64_t availableAt{0};
};

/**
 * Registry of stream references advertised by the Bridge. The registry stores
 * metadata only; video transport remains outside the Hub Protocol.
 */
class StreamRegistry {
public:
    bool upsert(const HubMessage& message, std::string* error = nullptr);
    bool close(const HubMessage& message, std::string* error = nullptr);

    [[nodiscard]] std::optional<StreamMetadata> stream(const std::string& streamId) const;
    [[nodiscard]] std::vector<StreamMetadata> activeStreams() const;

private:
    mutable std::mutex mutex;
    std::unordered_map<std::string, StreamMetadata> streams;
};
}
