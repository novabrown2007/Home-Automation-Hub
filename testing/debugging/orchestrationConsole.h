#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "../../bridge/hubProtocol.h"
#include "eventTracer.h"

namespace homeautomationhub::testing {
/**
 * Human-readable debugging console for the simulated Hub environment. It writes
 * concise flow lines while retaining a snapshot for tests and state dumps.
 */
class OrchestrationConsole {
public:
    explicit OrchestrationConsole(EventTracer& tracer);

    void bridgeEvent(const bridge::HubMessage& message);
    void automation(std::string detail);
    void analysis(std::string detail);
    void command(std::string detail);
    void stream(std::string detail);
    void subscription(std::string detail);

    [[nodiscard]] std::vector<std::string> lines() const;

private:
    void append(const std::string& label, std::string detail);

    EventTracer& tracer;
    mutable std::mutex mutex;
    std::vector<std::string> output;
};
}
