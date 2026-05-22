#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
/**
 * Internal pub/sub registry for normalized Hub Protocol categories. The same
 * wildcard matching can later back remote Aura or distributed Hub listeners.
 */
class SubscriptionManager {
public:
    using Listener = std::function<void(const HubMessage&)>;
    using SubscriptionId = std::size_t;

    SubscriptionId subscribe(std::string categoryPattern, Listener listener);
    bool unsubscribe(SubscriptionId subscriptionId);
    void publish(const HubMessage& message) const;

    [[nodiscard]] std::vector<std::string> categoryPatterns() const;
    [[nodiscard]] bool hasSubscribers(const std::string& category) const;
    [[nodiscard]] static bool matches(const std::string& categoryPattern, const std::string& category);

private:
    struct Subscription {
        std::string categoryPattern;
        Listener listener;
    };

    mutable std::mutex mutex;
    SubscriptionId nextId{1};
    std::unordered_map<SubscriptionId, Subscription> subscriptions;
};
}
