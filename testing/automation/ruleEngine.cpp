#include "ruleEngine.h"

#include <algorithm>
#include <utility>

#include "../../bridge/subscriptions/subscriptionManager.h"

namespace homeautomationhub::testing {
bool RuleEngine::addRule(AutomationRule rule) {
    if (rule.id.empty() || rule.categoryPattern.empty() || !rule.action) {
        return false;
    }
    std::lock_guard lock(mutex);
    const auto duplicate = std::find_if(rules.begin(), rules.end(), [&rule](const AutomationRule& current) {
        return current.id == rule.id;
    });
    if (duplicate != rules.end()) {
        return false;
    }
    rules.push_back(std::move(rule));
    return true;
}

std::size_t RuleEngine::evaluate(const bridge::HubMessage& message) const {
    std::vector<AutomationRule> candidates;
    const std::string category = bridge::toString(message.category);
    {
        std::lock_guard lock(mutex);
        std::copy_if(rules.begin(), rules.end(), std::back_inserter(candidates), [&category](const AutomationRule& rule) {
            return bridge::SubscriptionManager::matches(rule.categoryPattern, category);
        });
    }

    std::size_t executions = 0;
    for (const AutomationRule& rule : candidates) {
        const bool conditionAccepted = !rule.condition || rule.condition(message);
        if (conditionAccepted && rule.action(message)) {
            ++executions;
        }
    }
    return executions;
}

std::vector<std::string> RuleEngine::ruleIds() const {
    std::lock_guard lock(mutex);
    std::vector<std::string> result;
    result.reserve(rules.size());
    for (const AutomationRule& rule : rules) {
        result.push_back(rule.id);
    }
    return result;
}
}
