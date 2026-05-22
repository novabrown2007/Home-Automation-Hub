#pragma once

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../bridge/hubProtocol.h"
#include "../../bridge/streams/streamRegistry.h"
#include "../../bridge/subscriptions/subscriptionManager.h"
#include "../debugging/eventTracer.h"
#include "../debugging/orchestrationConsole.h"
#include "mockVisionAnalyzer.h"

namespace homeautomationhub::testing {
/**
 * Simulated stream analysis lifecycle manager. It attaches mock analyzers to
 * Bridge-advertised stream references and never carries video payload bytes.
 */
class StreamAnalysisManager {
public:
    StreamAnalysisManager(
        const bridge::StreamRegistry& streams,
        MockVisionAnalyzer& vision,
        EventTracer& tracer,
        OrchestrationConsole& console
    );

    void attach(bridge::SubscriptionManager& subscriptions);
    void process(const bridge::HubMessage& message);

    [[nodiscard]] std::vector<std::string> attachedStreams() const;

private:
    const bridge::StreamRegistry& streams;
    MockVisionAnalyzer& vision;
    EventTracer& tracer;
    OrchestrationConsole& console;
    mutable std::mutex mutex;
    std::unordered_set<std::string> activeStreamIds;
    bool subscribed{false};
};
}
