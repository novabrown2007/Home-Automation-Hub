#include "eventTracer.h"

#include <algorithm>
#include <utility>

namespace homeautomationhub::testing {
void EventTracer::trace(std::string stage, std::string detail) {
    std::lock_guard lock(mutex);
    timeline.push_back(TraceEntry{
        .timestamp = bridge::unixTimestamp(),
        .stage = std::move(stage),
        .detail = std::move(detail)
    });
}

void EventTracer::traceMessage(std::string stage, const bridge::HubMessage& message, std::string detail) {
    if (detail.empty()) {
        detail = "message routed";
    }
    std::lock_guard lock(mutex);
    timeline.push_back(TraceEntry{
        .timestamp = bridge::unixTimestamp(),
        .stage = std::move(stage),
        .detail = std::move(detail),
        .category = bridge::toString(message.category),
        .messageId = message.messageId
    });
}

std::vector<TraceEntry> EventTracer::entries() const {
    std::lock_guard lock(mutex);
    return timeline;
}

std::vector<TraceEntry> EventTracer::entriesForStage(const std::string& stage) const {
    std::lock_guard lock(mutex);
    std::vector<TraceEntry> result;
    std::copy_if(timeline.begin(), timeline.end(), std::back_inserter(result), [&stage](const TraceEntry& entry) {
        return entry.stage == stage;
    });
    return result;
}

std::size_t EventTracer::count() const {
    std::lock_guard lock(mutex);
    return timeline.size();
}

void EventTracer::clear() {
    std::lock_guard lock(mutex);
    timeline.clear();
}
}
