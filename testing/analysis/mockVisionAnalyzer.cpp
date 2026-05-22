#include "mockVisionAnalyzer.h"

namespace homeautomationhub::testing {
MockVisionAnalyzer::MockVisionAnalyzer(bridge::HubClient& hubClient, EventTracer& tracer, OrchestrationConsole& console)
    : hubClient(hubClient), tracer(tracer), console(console) {}

bool MockVisionAnalyzer::analyze(const bridge::StreamMetadata& stream) {
    bool sentAll = true;
    for (const bridge::HubMessage& result : resultsFor(stream)) {
        tracer.traceMessage("ANALYSIS", result, "mock vision result " + result.data.value("event", ""));
        console.analysis(result.data.value("event", "") + " camera=" + stream.deviceId);
        sentAll = hubClient.send(result) && sentAll;
    }
    return sentAll;
}

std::vector<bridge::HubMessage> MockVisionAnalyzer::resultsFor(const bridge::StreamMetadata& stream) const {
    const std::string room = stream.source.value("room", stream.metadata.value("room", "unknown"));
    std::vector<bridge::HubMessage> results;
    results.push_back(bridge::HubMessage::create(
        bridge::HubCategory::AnalysisResult,
        bridge::Json{{"module", "mockVision"}, {"streamId", stream.streamId}},
        bridge::Json{{"event", "personDetected"}, {"confidence", 0.91}, {"cameraId", stream.deviceId}, {"room", room}}
    ));
    results.push_back(bridge::HubMessage::create(
        bridge::HubCategory::AnalysisResult,
        bridge::Json{{"module", "occupancy"}, {"streamId", stream.streamId}},
        bridge::Json{{"event", "occupancyDetected"}, {"confidence", 0.86}, {"cameraId", stream.deviceId}, {"room", room}}
    ));
    results.push_back(bridge::HubMessage::create(
        bridge::HubCategory::AnalysisResult,
        bridge::Json{{"module", "mockVision"}, {"streamId", stream.streamId}},
        bridge::Json{{"event", stream.metadata.value("packageScenario", false) ? "packageDetected" : "objectDetected"},
                     {"confidence", 0.78}, {"cameraId", stream.deviceId}, {"room", room}}
    ));
    return results;
}
}
