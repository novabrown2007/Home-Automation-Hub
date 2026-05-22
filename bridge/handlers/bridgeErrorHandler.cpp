#include "bridgeErrorHandler.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
bool BridgeErrorHandler::handle(const HubMessage& message) const {
    if (message.category != HubCategory::BridgeError) return false;
    Logger::instance().warning(
        "HubProtocol.BridgeError",
        "Bridge error messageId=" + message.messageId + " data=" + message.data.dump()
    );
    return true;
}
}
