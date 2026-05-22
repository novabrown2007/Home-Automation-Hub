#include "deviceStateHandler.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
DeviceStateHandler::DeviceStateHandler(BridgeStateCache& stateCache) : stateCache(stateCache) {}

bool DeviceStateHandler::handle(const HubMessage& message) const {
    if (!stateCache.applyDeviceState(message)) return false;
    Logger::instance().info("HubProtocol.DeviceState", "Cached normalized state device=" + message.source.value("deviceId", ""));
    return true;
}
}
