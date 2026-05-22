#include "streamHandler.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
StreamHandler::StreamHandler(StreamRegistry& streamRegistry) : streamRegistry(streamRegistry) {}

bool StreamHandler::handle(const HubMessage& message) const {
    std::string error;
    if (message.category == HubCategory::StreamAvailable) {
        if (!streamRegistry.upsert(message, &error)) {
            Logger::instance().warning("HubProtocol.Stream", "Rejected stream metadata error=" + error);
            return false;
        }
        Logger::instance().info(
            "HubProtocol.Stream",
            "Stream available id=" + message.data.value("streamId", "") +
            " device=" + message.source.value("deviceId", "") +
            " type=" + message.data.value("streamType", "")
        );
        return true;
    }
    if (message.category == HubCategory::StreamClosed) {
        const bool closed = streamRegistry.close(message, &error);
        Logger::instance().info("HubProtocol.Stream", "Stream closed id=" + message.data.value("streamId", ""));
        return closed;
    }
    return false;
}
}
