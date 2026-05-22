#pragma once

#include <string>
#include <vector>

#include "../../bridge/hubClient.h"
#include "../../bridge/subscriptions/subscriptionManager.h"
#include "../debugging/eventTracer.h"
#include "../debugging/orchestrationConsole.h"
#include "ruleEngine.h"

namespace homeautomationhub::testing {
/**
 * Mock automation runtime. It validates event-driven automation routing by
 * evaluating normalized Hub Protocol categories and returning bridge.command
 * envelopes through HubClient.
 */
class AutomationEngine {
public:
    AutomationEngine(bridge::HubClient& hubClient, EventTracer& tracer, OrchestrationConsole& console);

    void installMockRules();
    void attach(bridge::SubscriptionManager& subscriptions);
    bool addRule(AutomationRule rule);
    std::size_t process(const bridge::HubMessage& message);

    [[nodiscard]] std::vector<std::string> activeRuleIds() const;

private:
    bool sendBrightnessCommand(const bridge::HubMessage& message, const std::string& ruleId);
    [[nodiscard]] static std::string automationTarget(const bridge::HubMessage& message);

    bridge::HubClient& hubClient;
    EventTracer& tracer;
    OrchestrationConsole& console;
    RuleEngine rules;
    bool attached{false};
};
}
