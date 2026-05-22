#pragma once

#include <string>

#include "../../bridge/state/bridgeStateCache.h"
#include "../../bridge/streams/streamRegistry.h"
#include "../../bridge/subscriptions/subscriptionManager.h"
#include "../analysis/occupancyAnalyzer.h"

namespace homeautomationhub::testing {
class AutomationEngine;

/**
 * Produces compact state snapshots for simulation and integration assertions.
 */
class StateDebugger {
public:
    StateDebugger(
        const bridge::BridgeStateCache& stateCache,
        const OccupancyAnalyzer& occupancy,
        const bridge::StreamRegistry& streams,
        const bridge::SubscriptionManager& subscriptions
    );

    [[nodiscard]] std::string snapshot(const AutomationEngine* automation = nullptr) const;

private:
    const bridge::BridgeStateCache& stateCache;
    const OccupancyAnalyzer& occupancy;
    const bridge::StreamRegistry& streams;
    const bridge::SubscriptionManager& subscriptions;
};
}
