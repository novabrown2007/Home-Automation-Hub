#include "integrationTester.h"

namespace homeautomationhub::testing {
IntegrationTester::IntegrationTester(OrchestrationSimulator& simulator, const std::vector<bridge::HubMessage>& outbound)
    : simulator(simulator), outbound(outbound) {}

OrchestrationTestReport IntegrationTester::validateMotionCommandFlow() {
    const std::size_t before = outbound.size();
    OrchestrationTestReport report{.name = "motion-command-flow"};
    const bool eventAccepted = simulator.motionDetected("hallwayMotion1", "hallway", "hallwayLight1");
    const bool commandSent = hasOutbound(before, bridge::HubCategory::BridgeCommand, "setBrightness");
    report.details.push_back(eventAccepted ? "motion event accepted" : "motion event rejected");
    report.details.push_back(commandSent ? "bridge.command emitted" : "bridge.command missing");
    report.passed = eventAccepted && commandSent;
    return report;
}

OrchestrationTestReport IntegrationTester::validateStreamAnalysisFlow() {
    const std::size_t before = outbound.size();
    OrchestrationTestReport report{.name = "stream-analysis-flow"};
    const bool streamAccepted = simulator.streamAvailable("doorCamera1", "entry", "entry-stream-01", true);
    const bool analysisSent = hasOutbound(before, bridge::HubCategory::AnalysisResult, "personDetected") &&
        hasOutbound(before, bridge::HubCategory::AnalysisResult, "packageDetected");
    report.details.push_back(streamAccepted ? "stream metadata accepted" : "stream metadata rejected");
    report.details.push_back(analysisSent ? "mock analysis emitted" : "mock analysis missing");
    report.passed = streamAccepted && analysisSent;
    return report;
}

bool IntegrationTester::hasOutbound(std::size_t begin, bridge::HubCategory category, const std::string& eventOrCommand) const {
    for (std::size_t index = begin; index < outbound.size(); ++index) {
        const bridge::HubMessage& message = outbound[index];
        if (message.category != category) continue;
        if (message.data.value("event", message.data.value("command", "")) == eventOrCommand) {
            return true;
        }
    }
    return false;
}
}
