#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
struct CachedDeviceState {
    std::string deviceId;
    Json source = Json::object();
    Json data = Json::object();
    std::string messageId;
    std::int64_t timestamp{0};
    bool online{true};
};

/**
 * Hub-local orchestration cache. The Bridge remains authoritative; this cache is
 * optimized for automation lookup and normalized state fanout inside the Hub.
 */
class BridgeStateCache {
public:
    bool applyDeviceState(const HubMessage& message);
    bool recordTelemetry(const HubMessage& message);
    bool markDeviceOnline(const std::string& deviceId, bool online);

    [[nodiscard]] std::optional<CachedDeviceState> deviceState(const std::string& deviceId) const;
    [[nodiscard]] std::vector<CachedDeviceState> deviceStates() const;
    [[nodiscard]] std::vector<HubMessage> telemetry(const std::string& deviceId) const;

private:
    mutable std::mutex mutex;
    std::unordered_map<std::string, CachedDeviceState> states;
    std::unordered_map<std::string, std::vector<HubMessage>> telemetryByDevice;
};
}
