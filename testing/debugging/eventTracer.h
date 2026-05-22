#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "../../bridge/hubProtocol.h"

namespace homeautomationhub::testing {
struct TraceEntry {
    std::int64_t timestamp{0};
    std::string stage;
    std::string detail;
    std::string category;
    std::string messageId;
};

/**
 * Thread-safe internal trace buffer for orchestration experiments. Entries are
 * intentionally transport-neutral so simulation, automation, and analysis can
 * share the same debugging timeline.
 */
class EventTracer {
public:
    void trace(std::string stage, std::string detail);
    void traceMessage(std::string stage, const bridge::HubMessage& message, std::string detail = "");

    [[nodiscard]] std::vector<TraceEntry> entries() const;
    [[nodiscard]] std::vector<TraceEntry> entriesForStage(const std::string& stage) const;
    [[nodiscard]] std::size_t count() const;
    void clear();

private:
    mutable std::mutex mutex;
    std::vector<TraceEntry> timeline;
};
}
