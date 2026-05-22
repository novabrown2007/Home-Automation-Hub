#include "streamAnalysisManager.h"

namespace homeautomationhub::testing {
StreamAnalysisManager::StreamAnalysisManager(
    const bridge::StreamRegistry& streams,
    MockVisionAnalyzer& vision,
    EventTracer& tracer,
    OrchestrationConsole& console
) : streams(streams), vision(vision), tracer(tracer), console(console) {}

void StreamAnalysisManager::attach(bridge::SubscriptionManager& subscriptions) {
    if (subscribed) return;
    subscriptions.subscribe("stream.*", [this](const bridge::HubMessage& message) { process(message); });
    subscribed = true;
    console.subscription("streamAnalysisManager -> stream.*");
}

void StreamAnalysisManager::process(const bridge::HubMessage& message) {
    const std::string streamId = message.data.value("streamId", "");
    if (message.category == bridge::HubCategory::StreamAvailable) {
        const auto stream = streams.stream(streamId);
        if (!stream.has_value()) {
            console.stream("analyzer attach skipped unknown stream=" + streamId);
            return;
        }
        {
            std::lock_guard lock(mutex);
            activeStreamIds.insert(streamId);
        }
        tracer.traceMessage("STREAM", message, "mock analyzers attached stream=" + streamId);
        console.stream("analyzers attached stream=" + streamId);
        vision.analyze(*stream);
    }
    if (message.category == bridge::HubCategory::StreamClosed) {
        std::lock_guard lock(mutex);
        activeStreamIds.erase(streamId);
        console.stream("analyzers detached stream=" + streamId);
    }
}

std::vector<std::string> StreamAnalysisManager::attachedStreams() const {
    std::lock_guard lock(mutex);
    return std::vector<std::string>(activeStreamIds.begin(), activeStreamIds.end());
}
}
