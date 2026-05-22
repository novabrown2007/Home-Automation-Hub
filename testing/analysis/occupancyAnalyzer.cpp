#include "occupancyAnalyzer.h"

#include <algorithm>

namespace homeautomationhub::testing {
OccupancyAnalyzer::OccupancyAnalyzer(EventTracer& tracer, OrchestrationConsole& console)
    : tracer(tracer), console(console) {}

void OccupancyAnalyzer::attach(bridge::SubscriptionManager& subscriptions) {
    subscriptions.subscribe("device.event", [this](const bridge::HubMessage& message) { observe(message); });
    subscriptions.subscribe("analysis.result", [this](const bridge::HubMessage& message) { observe(message); });
    console.subscription("occupancyAnalyzer -> device.event, analysis.result");
}

void OccupancyAnalyzer::observe(const bridge::HubMessage& message) {
    if (message.category == bridge::HubCategory::DeviceEvent && message.data.value("event", "") == "motionDetected") {
        applySignal(roomFrom(message), message.data.value("confidence", 0.65), message);
    }
    if (message.category == bridge::HubCategory::AnalysisResult) {
        const std::string event = message.data.value("event", "");
        if (event == "personDetected" || event == "occupancyDetected") {
            applySignal(roomFrom(message), message.data.value("confidence", 0.8), message);
        }
    }
}

std::optional<OccupancyState> OccupancyAnalyzer::state(const std::string& room) const {
    std::lock_guard lock(mutex);
    const auto found = occupancyByRoom.find(room);
    return found == occupancyByRoom.end() ? std::nullopt : std::optional<OccupancyState>(found->second);
}

std::vector<OccupancyState> OccupancyAnalyzer::states() const {
    std::lock_guard lock(mutex);
    std::vector<OccupancyState> result;
    result.reserve(occupancyByRoom.size());
    for (const auto& [_, state] : occupancyByRoom) {
        result.push_back(state);
    }
    return result;
}

std::string OccupancyAnalyzer::roomFrom(const bridge::HubMessage& message) {
    const std::string sourceRoom = message.source.value("room", "");
    if (!sourceRoom.empty()) return sourceRoom;
    const std::string dataRoom = message.data.value("room", "");
    return dataRoom.empty() ? "unknown" : dataRoom;
}

void OccupancyAnalyzer::applySignal(const std::string& room, double confidence, const bridge::HubMessage& message) {
    const double boundedConfidence = std::clamp(confidence, 0.0, 1.0);
    OccupancyState state;
    {
        std::lock_guard lock(mutex);
        OccupancyState& tracked = occupancyByRoom[room];
        tracked.room = room;
        tracked.signalCount += 1;
        tracked.confidence = tracked.signalCount == 1
            ? boundedConfidence
            : std::clamp((tracked.confidence * 0.55) + (boundedConfidence * 0.45), 0.0, 1.0);
        tracked.occupied = tracked.confidence >= 0.5;
        tracked.updatedAt = bridge::unixTimestamp();
        state = tracked;
    }
    tracer.traceMessage("OCCUPANCY", message, "room=" + state.room + " confidence=" + std::to_string(state.confidence));
    console.analysis("occupancy " + state.room + " confidence=" + std::to_string(state.confidence));
}
}
