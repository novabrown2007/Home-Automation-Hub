#pragma once

#include "../../bridge/state/bridgeStateCache.h"
#include "../../bridge/streams/streamRegistry.h"
#include "../analysis/occupancyAnalyzer.h"
#include "../debugging/eventTracer.h"
#include "../simulation/orchestrationSimulator.h"
#include "integrationTester.h"

namespace homeautomationhub::testing {
/**
 * Full-chain tester for cache synchronization, subscriptions, occupancy, and
 * stream lifecycle behavior inside the mock Hub environment.
 */
class WorkflowTester {
public:
    WorkflowTester(
        OrchestrationSimulator& simulator,
        const bridge::BridgeStateCache& stateCache,
        const bridge::StreamRegistry& streams,
        const OccupancyAnalyzer& occupancy,
        const EventTracer& tracer
    );

    [[nodiscard]] OrchestrationTestReport validateOrchestrationWorkflow();

private:
    OrchestrationSimulator& simulator;
    const bridge::BridgeStateCache& stateCache;
    const bridge::StreamRegistry& streams;
    const OccupancyAnalyzer& occupancy;
    const EventTracer& tracer;
};
}
