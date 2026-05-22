#pragma once

#include "../hubProtocol.h"
#include "../state/bridgeStateCache.h"

namespace homeautomationhub::bridge {
class DeviceTelemetryHandler {
public:
    explicit DeviceTelemetryHandler(BridgeStateCache& stateCache);
    bool handle(const HubMessage& message) const;

private:
    BridgeStateCache& stateCache;
};
}
