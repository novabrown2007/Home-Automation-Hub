#include "automationEngine.h"

#include <utility>

namespace homeautomationhub::testing {
AutomationEngine::AutomationEngine(bridge::HubClient& hubClient, EventTracer& tracer, OrchestrationConsole& console)
    : hubClient(hubClient), tracer(tracer), console(console) {}

void AutomationEngine::installMockRules() {
    addRule(AutomationRule{
        .id = "motion-to-light-brightness",
        .categoryPattern = "device.event",
        .trigger = AutomationTrigger::DeviceEvent,
        .condition = [](const bridge::HubMessage& message) {
            return message.data.value("event", "") == "motionDetected";
        },
        .action = [this](const bridge::HubMessage& message) {
            return sendBrightnessCommand(message, "motion-to-light-brightness");
        }
    });
    addRule(AutomationRule{
        .id = "state-requested-brightness",
        .categoryPattern = "device.state",
        .trigger = AutomationTrigger::DeviceState,
        .condition = [](const bridge::HubMessage& message) {
            return message.data.contains("automationTargetDeviceId") &&
                message.data.contains("desiredBrightness") &&
                message.data.at("desiredBrightness").is_number_integer();
        },
        .action = [this](const bridge::HubMessage& message) {
            return sendBrightnessCommand(message, "state-requested-brightness");
        }
    });
    addRule(AutomationRule{
        .id = "stream-analysis-ready",
        .categoryPattern = "stream.available",
        .trigger = AutomationTrigger::Stream,
        .action = [this](const bridge::HubMessage& message) {
            tracer.traceMessage("AUTOMATION", message, "stream automation evaluated");
            console.automation("stream ready " + message.data.value("streamId", ""));
            return true;
        }
    });
}

void AutomationEngine::attach(bridge::SubscriptionManager& subscriptions) {
    if (attached) return;
    subscriptions.subscribe("device.event", [this](const bridge::HubMessage& message) { process(message); });
    subscriptions.subscribe("device.state", [this](const bridge::HubMessage& message) { process(message); });
    subscriptions.subscribe("stream.available", [this](const bridge::HubMessage& message) { process(message); });
    attached = true;
    console.subscription("automationEngine -> device.event, device.state, stream.available");
}

bool AutomationEngine::addRule(AutomationRule rule) {
    return rules.addRule(std::move(rule));
}

std::size_t AutomationEngine::process(const bridge::HubMessage& message) {
    const std::size_t executed = rules.evaluate(message);
    if (executed > 0) {
        tracer.traceMessage("AUTOMATION", message, "executed rules=" + std::to_string(executed));
    }
    return executed;
}

std::vector<std::string> AutomationEngine::activeRuleIds() const {
    return rules.ruleIds();
}

bool AutomationEngine::sendBrightnessCommand(const bridge::HubMessage& message, const std::string& ruleId) {
    const std::string target = automationTarget(message);
    const int brightness = message.data.value("desiredBrightness", 80);
    if (target.empty()) {
        console.automation(ruleId + " skipped: no normalized target device");
        return false;
    }
    console.automation(ruleId + " -> " + target + ".setBrightness");
    const bool sent = hubClient.sendCommand(target, "setBrightness", bridge::Json{{"brightness", brightness}});
    if (sent) {
        console.command("bridge.command setBrightness -> " + target);
    }
    return sent;
}

std::string AutomationEngine::automationTarget(const bridge::HubMessage& message) {
    const std::string explicitTarget = message.data.value("automationTargetDeviceId", "");
    if (!explicitTarget.empty()) return explicitTarget;
    const std::string room = message.source.value("room", message.data.value("room", ""));
    return room.empty() ? "" : room + "Light1";
}
}
