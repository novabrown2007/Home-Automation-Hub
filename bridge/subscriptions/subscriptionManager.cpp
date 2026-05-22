#include "subscriptionManager.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace homeautomationhub::bridge {
SubscriptionManager::SubscriptionId SubscriptionManager::subscribe(std::string categoryPattern, Listener listener) {
    if (categoryPattern.empty() || !listener) {
        return 0;
    }
    std::lock_guard lock(mutex);
    const SubscriptionId id = nextId++;
    subscriptions.emplace(id, Subscription{std::move(categoryPattern), std::move(listener)});
    return id;
}

bool SubscriptionManager::unsubscribe(SubscriptionId subscriptionId) {
    std::lock_guard lock(mutex);
    return subscriptions.erase(subscriptionId) > 0;
}

void SubscriptionManager::publish(const HubMessage& message) const {
    std::vector<Listener> listeners;
    const std::string category = toString(message.category);
    {
        std::lock_guard lock(mutex);
        for (const auto& [_, subscription] : subscriptions) {
            if (matches(subscription.categoryPattern, category)) {
                listeners.push_back(subscription.listener);
            }
        }
    }
    for (const auto& listener : listeners) {
        listener(message);
    }
}

std::vector<std::string> SubscriptionManager::categoryPatterns() const {
    std::unordered_set<std::string> uniquePatterns;
    std::lock_guard lock(mutex);
    for (const auto& [_, subscription] : subscriptions) {
        uniquePatterns.insert(subscription.categoryPattern);
    }
    std::vector<std::string> patterns(uniquePatterns.begin(), uniquePatterns.end());
    std::sort(patterns.begin(), patterns.end());
    return patterns;
}

bool SubscriptionManager::hasSubscribers(const std::string& category) const {
    std::lock_guard lock(mutex);
    return std::any_of(subscriptions.begin(), subscriptions.end(), [&category](const auto& entry) {
        return matches(entry.second.categoryPattern, category);
    });
}

bool SubscriptionManager::matches(const std::string& categoryPattern, const std::string& category) {
    if (categoryPattern == "*" || categoryPattern == category) {
        return true;
    }
    if (!categoryPattern.ends_with("*")) {
        return false;
    }
    return category.rfind(categoryPattern.substr(0, categoryPattern.size() - 1), 0) == 0;
}
}
