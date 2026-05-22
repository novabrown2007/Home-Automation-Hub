#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../hubProtocol.h"
#include "../subscriptions/subscriptionManager.h"

namespace homeautomationhub::bridge {
/**
 * Category router for normalized messages. Category handlers own state changes;
 * subscriptions fan out the accepted messages to automation and future clients.
 */
class MessageRouter {
public:
    using Handler = std::function<bool(const HubMessage&)>;

    explicit MessageRouter(SubscriptionManager& subscriptions);

    void registerHandler(HubCategory category, Handler handler);
    bool route(const HubMessage& message) const;

private:
    SubscriptionManager& subscriptions;
    mutable std::mutex mutex;
    std::unordered_map<std::string, Handler> handlers;
};
}
