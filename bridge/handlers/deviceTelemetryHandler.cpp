#include "deviceTelemetryHandler.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
DeviceTelemetryHandler::DeviceTelemetryHandler(BridgeStateCache& stateCache) : stateCache(stateCache) {}

bool DeviceTelemetryHandler::handle(const HubMessage& message) const {
    if (!stateCache.recordTelemetry(message)) return false;
    Logger::instance().debug("HubProtocol.Telemetry", "Recorded telemetry device=" + message.source.value("deviceId", ""));
    return true;
}
}
