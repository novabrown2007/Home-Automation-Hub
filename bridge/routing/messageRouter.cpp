#include "messageRouter.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
MessageRouter::MessageRouter(SubscriptionManager& subscriptions) : subscriptions(subscriptions) {}

void MessageRouter::registerHandler(HubCategory category, Handler handler) {
    if (!handler) return;
    std::lock_guard lock(mutex);
    handlers[toString(category)] = std::move(handler);
}

bool MessageRouter::route(const HubMessage& message) const {
    Handler handler;
    const std::string category = toString(message.category);
    {
        std::lock_guard lock(mutex);
        const auto found = handlers.find(category);
        if (found != handlers.end()) handler = found->second;
    }
    if (!handler) {
        Logger::instance().warning("HubProtocol.Router", "No handler registered category=" + category);
        return false;
    }
    if (!handler(message)) {
        Logger::instance().warning("HubProtocol.Router", "Handler rejected category=" + category + " messageId=" + message.messageId);
        return false;
    }
    subscriptions.publish(message);
    Logger::instance().debug("HubProtocol.Router", "Routed category=" + category + " messageId=" + message.messageId);
    return true;
}
}
