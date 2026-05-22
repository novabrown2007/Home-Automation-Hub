#include "deviceEventHandler.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
bool DeviceEventHandler::handle(const HubMessage& message) const {
    if (message.category != HubCategory::DeviceEvent) return false;
    Logger::instance().info(
        "HubProtocol.DeviceEvent",
        "Received event=" + message.data.value("event", "") + " device=" + message.source.value("deviceId", "")
    );
    return true;
}
}
