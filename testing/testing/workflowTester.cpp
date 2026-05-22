#include "workflowTester.h"

namespace homeautomationhub::testing {
WorkflowTester::WorkflowTester(
    OrchestrationSimulator& simulator,
    const bridge::BridgeStateCache& stateCache,
    const bridge::StreamRegistry& streams,
    const OccupancyAnalyzer& occupancy,
    const EventTracer& tracer
) : simulator(simulator), stateCache(stateCache), streams(streams), occupancy(occupancy), tracer(tracer) {}

OrchestrationTestReport WorkflowTester::validateOrchestrationWorkflow() {
    OrchestrationTestReport report{.name = "full-orchestration-workflow"};
    const bool stateAccepted = simulator.deviceState(
        "bedroomLight1",
        "bedroom",
        bridge::Json{{"power", true}, {"brightness", 75}, {"automationTargetDeviceId", "bedroomLight1"}, {"desiredBrightness", 60}}
    );
    const bool eventAccepted = simulator.motionDetected("bedroomMotion1", "bedroom", "bedroomLight1");
    const bool streamAccepted = simulator.streamAvailable("bedroomCamera1", "bedroom", "bedroom-stream-01");
    const bool stateSynchronized = stateCache.deviceState("bedroomLight1").has_value();
    const bool streamRegistered = streams.stream("bedroom-stream-01").has_value();
    const bool occupancyTracked = occupancy.state("bedroom").has_value();
    const bool streamClosed = simulator.streamClosed("bedroomCamera1", "bedroom-stream-01") &&
        !streams.stream("bedroom-stream-01").has_value();
    const bool traced = tracer.count() > 0;
    report.details = {
        stateAccepted ? "state accepted" : "state rejected",
        eventAccepted ? "event accepted" : "event rejected",
        streamAccepted ? "stream accepted" : "stream rejected",
        stateSynchronized ? "state cache synchronized" : "state cache empty",
        streamRegistered ? "stream registered" : "stream missing",
        occupancyTracked ? "occupancy tracked" : "occupancy missing",
        streamClosed ? "stream lifecycle closed" : "stream lifecycle open",
        traced ? "trace timeline populated" : "trace timeline empty"
    };
    report.passed = stateAccepted && eventAccepted && streamAccepted && stateSynchronized &&
        streamRegistered && occupancyTracked && streamClosed && traced;
    return report;
}
}
