#pragma once

#include "../hubProtocol.h"
#include "../state/bridgeStateCache.h"

namespace homeautomationhub::bridge {
class DeviceStateHandler {
public:
    explicit DeviceStateHandler(BridgeStateCache& stateCache);
    bool handle(const HubMessage& message) const;

private:
    BridgeStateCache& stateCache;
};
}
