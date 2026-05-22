#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../bridge/hubProtocol.h"
#include "../../bridge/subscriptions/subscriptionManager.h"
#include "../debugging/eventTracer.h"
#include "../debugging/orchestrationConsole.h"

namespace homeautomationhub::testing {
struct OccupancyState {
    std::string room;
    bool occupied{false};
    double confidence{0.0};
    std::size_t signalCount{0};
    std::int64_t updatedAt{0};
};

/**
 * Mock occupancy aggregator. It combines simulated motion and analysis signals
 * into room-level confidence without asserting Bridge-authoritative state.
 */
class OccupancyAnalyzer {
public:
    OccupancyAnalyzer(EventTracer& tracer, OrchestrationConsole& console);

    void attach(bridge::SubscriptionManager& subscriptions);
    void observe(const bridge::HubMessage& message);

    [[nodiscard]] std::optional<OccupancyState> state(const std::string& room) const;
    [[nodiscard]] std::vector<OccupancyState> states() const;

private:
    [[nodiscard]] static std::string roomFrom(const bridge::HubMessage& message);
    void applySignal(const std::string& room, double confidence, const bridge::HubMessage& message);

    EventTracer& tracer;
    OrchestrationConsole& console;
    mutable std::mutex mutex;
    std::unordered_map<std::string, OccupancyState> occupancyByRoom;
};
}
