#pragma once

#include <string>
#include <vector>

#include "../../bridge/hubProtocol.h"
#include "../simulation/orchestrationSimulator.h"

namespace homeautomationhub::testing {
struct OrchestrationTestReport {
    bool passed{false};
    std::string name;
    std::vector<std::string> details;
};

/**
 * Scenario assertions for Bridge event -> Hub action -> Bridge envelope flows.
 */
class IntegrationTester {
public:
    IntegrationTester(OrchestrationSimulator& simulator, const std::vector<bridge::HubMessage>& outbound);

    [[nodiscard]] OrchestrationTestReport validateMotionCommandFlow();
    [[nodiscard]] OrchestrationTestReport validateStreamAnalysisFlow();

private:
    [[nodiscard]] bool hasOutbound(std::size_t begin, bridge::HubCategory category, const std::string& eventOrCommand) const;

    OrchestrationSimulator& simulator;
    const std::vector<bridge::HubMessage>& outbound;
};
}
