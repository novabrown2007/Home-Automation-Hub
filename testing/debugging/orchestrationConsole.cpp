#include "orchestrationConsole.h"

#include <utility>

#include "../../logging/logger.h"

namespace homeautomationhub::testing {
OrchestrationConsole::OrchestrationConsole(EventTracer& tracer) : tracer(tracer) {}

void OrchestrationConsole::bridgeEvent(const bridge::HubMessage& message) {
    const std::string detail = bridge::toString(message.category) +
        " from " + message.source.value("deviceId", message.source.value("module", "bridge"));
    tracer.traceMessage("EVENT", message, detail);
    append("EVENT", detail);
}

void OrchestrationConsole::automation(std::string detail) {
    tracer.trace("AUTOMATION", detail);
    append("AUTOMATION", std::move(detail));
}

void OrchestrationConsole::analysis(std::string detail) {
    tracer.trace("ANALYSIS", detail);
    append("ANALYSIS", std::move(detail));
}

void OrchestrationConsole::command(std::string detail) {
    tracer.trace("COMMAND", detail);
    append("COMMAND", std::move(detail));
}

void OrchestrationConsole::stream(std::string detail) {
    tracer.trace("STREAM", detail);
    append("STREAM", std::move(detail));
}

void OrchestrationConsole::subscription(std::string detail) {
    tracer.trace("SUBSCRIPTION", detail);
    append("SUBSCRIPTION", std::move(detail));
}

std::vector<std::string> OrchestrationConsole::lines() const {
    std::lock_guard lock(mutex);
    return output;
}

void OrchestrationConsole::append(const std::string& label, std::string detail) {
    const std::string line = "[" + label + "] " + detail;
    {
        std::lock_guard lock(mutex);
        output.push_back(line);
    }
    Logger::instance().info("OrchestrationConsole", line);
}
}
