#include "stateDebugger.h"

#include <sstream>

#include "../automation/automationEngine.h"

namespace homeautomationhub::testing {
StateDebugger::StateDebugger(
    const bridge::BridgeStateCache& stateCache,
    const OccupancyAnalyzer& occupancy,
    const bridge::StreamRegistry& streams,
    const bridge::SubscriptionManager& subscriptions
) : stateCache(stateCache), occupancy(occupancy), streams(streams), subscriptions(subscriptions) {}

std::string StateDebugger::snapshot(const AutomationEngine* automation) const {
    std::ostringstream result;
    result << "cached_devices=" << stateCache.deviceStates().size()
           << " occupancy_rooms=" << occupancy.states().size()
           << " active_streams=" << streams.activeStreams().size()
           << " subscriptions=" << subscriptions.categoryPatterns().size();
    if (automation != nullptr) {
        result << " automations=" << automation->activeRuleIds().size();
    }
    return result.str();
}
}
