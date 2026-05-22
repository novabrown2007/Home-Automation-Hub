#include "bridgeStateCache.h"

namespace homeautomationhub::bridge {
namespace {
std::string deviceIdFrom(const HubMessage& message) {
    return message.source.value("deviceId", "");
}
}

bool BridgeStateCache::applyDeviceState(const HubMessage& message) {
    const std::string deviceId = deviceIdFrom(message);
    if (message.category != HubCategory::DeviceState || deviceId.empty()) {
        return false;
    }
    std::lock_guard lock(mutex);
    states[deviceId] = CachedDeviceState{
        .deviceId = deviceId,
        .source = message.source,
        .data = message.data,
        .messageId = message.messageId,
        .timestamp = message.timestamp,
        .online = true
    };
    return true;
}

bool BridgeStateCache::recordTelemetry(const HubMessage& message) {
    const std::string deviceId = deviceIdFrom(message);
    if (message.category != HubCategory::DeviceTelemetry || deviceId.empty()) {
        return false;
    }
    std::lock_guard lock(mutex);
    telemetryByDevice[deviceId].push_back(message);
    return true;
}

bool BridgeStateCache::markDeviceOnline(const std::string& deviceId, bool online) {
    std::lock_guard lock(mutex);
    const auto found = states.find(deviceId);
    if (found == states.end()) {
        return false;
    }
    found->second.online = online;
    return true;
}

std::optional<CachedDeviceState> BridgeStateCache::deviceState(const std::string& deviceId) const {
    std::lock_guard lock(mutex);
    const auto found = states.find(deviceId);
    if (found == states.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<CachedDeviceState> BridgeStateCache::deviceStates() const {
    std::lock_guard lock(mutex);
    std::vector<CachedDeviceState> result;
    result.reserve(states.size());
    for (const auto& [_, state] : states) {
        result.push_back(state);
    }
    return result;
}

std::vector<HubMessage> BridgeStateCache::telemetry(const std::string& deviceId) const {
    std::lock_guard lock(mutex);
    const auto found = telemetryByDevice.find(deviceId);
    return found == telemetryByDevice.end() ? std::vector<HubMessage>{} : found->second;
}
}
