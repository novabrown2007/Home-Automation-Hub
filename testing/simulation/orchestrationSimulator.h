#pragma once

#include <string>

#include "../../bridge/hubClient.h"
#include "../debugging/eventTracer.h"
#include "../debugging/orchestrationConsole.h"

namespace homeautomationhub::testing {
/**
 * Emits deterministic normalized Bridge traffic into HubClient. Scenarios use
 * Hub Protocol envelopes only, mirroring the future mock ecosystem feed.
 */
class OrchestrationSimulator {
public:
    OrchestrationSimulator(bridge::HubClient& hubClient, EventTracer& tracer, OrchestrationConsole& console);

    bool deviceState(const std::string& deviceId, const std::string& room, bridge::Json data);
    bool motionDetected(
        const std::string& sensorId,
        const std::string& room,
        const std::string& targetDeviceId = "",
        double confidence = 0.94
    );
    bool streamAvailable(
        const std::string& cameraId,
        const std::string& room,
        const std::string& streamId,
        bool packageScenario = false
    );
    bool streamClosed(const std::string& cameraId, const std::string& streamId);

    bool runOccupancyScenario();

private:
    bool emit(const bridge::HubMessage& message);

    bridge::HubClient& hubClient;
    EventTracer& tracer;
    OrchestrationConsole& console;
};
}
