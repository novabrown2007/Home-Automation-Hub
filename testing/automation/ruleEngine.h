#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "../../bridge/hubProtocol.h"

namespace homeautomationhub::testing {
enum class AutomationTrigger {
    DeviceEvent,
    DeviceState,
    Stream
};

struct AutomationRule {
    std::string id;
    std::string categoryPattern;
    AutomationTrigger trigger{AutomationTrigger::DeviceEvent};
    std::function<bool(const bridge::HubMessage&)> condition;
    std::function<bool(const bridge::HubMessage&)> action;
};

/**
 * Minimal event-condition-action rule evaluator. It avoids transport and device
 * details so a future rules DSL can replace these callable predicates.
 */
class RuleEngine {
public:
    bool addRule(AutomationRule rule);
    std::size_t evaluate(const bridge::HubMessage& message) const;

    [[nodiscard]] std::vector<std::string> ruleIds() const;

private:
    mutable std::mutex mutex;
    std::vector<AutomationRule> rules;
};
}
