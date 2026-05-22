#pragma once

#include <string>
#include <vector>

#include "../../bridge/hubClient.h"
#include "../../bridge/streams/streamRegistry.h"
#include "../debugging/eventTracer.h"
#include "../debugging/orchestrationConsole.h"

namespace homeautomationhub::testing {
/**
 * Deterministic mock vision pipeline. It emits protocol-shaped analysis results
 * for person, occupancy, object, and package scenarios while real AI is absent.
 */
class MockVisionAnalyzer {
public:
    MockVisionAnalyzer(bridge::HubClient& hubClient, EventTracer& tracer, OrchestrationConsole& console);

    bool analyze(const bridge::StreamMetadata& stream);
    [[nodiscard]] std::vector<bridge::HubMessage> resultsFor(const bridge::StreamMetadata& stream) const;

private:
    bridge::HubClient& hubClient;
    EventTracer& tracer;
    OrchestrationConsole& console;
};
}
