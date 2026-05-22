#include "orchestrationSimulator.h"

namespace homeautomationhub::testing {
OrchestrationSimulator::OrchestrationSimulator(bridge::HubClient& hubClient, EventTracer& tracer, OrchestrationConsole& console)
    : hubClient(hubClient), tracer(tracer), console(console) {}

bool OrchestrationSimulator::deviceState(const std::string& deviceId, const std::string& room, bridge::Json data) {
    return emit(bridge::HubMessage::create(
        bridge::HubCategory::DeviceState,
        bridge::Json{{"deviceId", deviceId}, {"room", room}},
        std::move(data)
    ));
}

bool OrchestrationSimulator::motionDetected(
    const std::string& sensorId,
    const std::string& room,
    const std::string& targetDeviceId,
    double confidence
) {
    bridge::Json data{{"event", "motionDetected"}, {"confidence", confidence}, {"room", room}};
    if (!targetDeviceId.empty()) data["automationTargetDeviceId"] = targetDeviceId;
    return emit(bridge::HubMessage::create(
        bridge::HubCategory::DeviceEvent,
        bridge::Json{{"deviceId", sensorId}, {"room", room}},
        std::move(data)
    ));
}

bool OrchestrationSimulator::streamAvailable(
    const std::string& cameraId,
    const std::string& room,
    const std::string& streamId,
    bool packageScenario
) {
    return emit(bridge::HubMessage::create(
        bridge::HubCategory::StreamAvailable,
        bridge::Json{{"deviceId", cameraId}, {"room", room}},
        bridge::Json{
            {"streamId", streamId},
            {"streamType", "rtsp"},
            {"codec", "h264"},
            {"resolution", "1920x1080"},
            {"fps", 30},
            {"endpoint", "rtsp://mock-bridge/" + streamId},
            {"packageScenario", packageScenario},
            {"room", room}
        }
    ));
}

bool OrchestrationSimulator::streamClosed(const std::string& cameraId, const std::string& streamId) {
    return emit(bridge::HubMessage::create(
        bridge::HubCategory::StreamClosed,
        bridge::Json{{"deviceId", cameraId}},
        bridge::Json{{"streamId", streamId}}
    ));
}

bool OrchestrationSimulator::runOccupancyScenario() {
    return deviceState("hallwayLight1", "hallway", bridge::Json{{"power", false}, {"brightness", 0}}) &&
        motionDetected("hallwayMotion1", "hallway", "hallwayLight1") &&
        streamAvailable("hallwayCamera1", "hallway", "hallway-stream-01");
}

bool OrchestrationSimulator::emit(const bridge::HubMessage& message) {
    tracer.traceMessage("SIMULATION", message, "simulated bridge traffic");
    console.bridgeEvent(message);
    return hubClient.receive(bridge::toJson(message).dump());
}
}
