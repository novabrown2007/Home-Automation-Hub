#include "streamRegistry.h"

namespace homeautomationhub::bridge {
namespace {
void setError(std::string* error, const std::string& value) {
    if (error != nullptr) *error = value;
}
}

bool StreamRegistry::upsert(const HubMessage& message, std::string* error) {
    if (message.category != HubCategory::StreamAvailable) {
        setError(error, "stream registry accepts stream.available messages only");
        return false;
    }
    const std::string streamId = message.data.value("streamId", "");
    const std::string deviceId = message.source.value("deviceId", "");
    const std::string streamType = message.data.value("streamType", "");
    const std::string endpoint = message.data.value("endpoint", "");
    if (streamId.empty() || deviceId.empty() || streamType.empty() || endpoint.empty()) {
        setError(error, "stream.available is missing streamId, deviceId, streamType, or endpoint");
        return false;
    }

    std::lock_guard lock(mutex);
    streams[streamId] = StreamMetadata{
        .streamId = streamId,
        .deviceId = deviceId,
        .streamType = streamType,
        .codec = message.data.value("codec", ""),
        .resolution = message.data.value("resolution", ""),
        .endpoint = endpoint,
        .fps = message.data.contains("fps") && message.data.at("fps").is_number() ? message.data.at("fps").get<double>() : 0.0,
        .source = message.source,
        .metadata = message.data,
        .availableAt = message.timestamp
    };
    return true;
}

bool StreamRegistry::close(const HubMessage& message, std::string* error) {
    if (message.category != HubCategory::StreamClosed) {
        setError(error, "stream registry closes stream.closed messages only");
        return false;
    }
    const std::string streamId = message.data.value("streamId", "");
    if (streamId.empty()) {
        setError(error, "stream.closed is missing streamId");
        return false;
    }
    std::lock_guard lock(mutex);
    return streams.erase(streamId) > 0;
}

std::optional<StreamMetadata> StreamRegistry::stream(const std::string& streamId) const {
    std::lock_guard lock(mutex);
    const auto found = streams.find(streamId);
    if (found == streams.end()) return std::nullopt;
    return found->second;
}

std::vector<StreamMetadata> StreamRegistry::activeStreams() const {
    std::lock_guard lock(mutex);
    std::vector<StreamMetadata> result;
    result.reserve(streams.size());
    for (const auto& [_, stream] : streams) result.push_back(stream);
    return result;
}
}
