#include "analysisHandler.h"

#include "../../logging/logger.h"

namespace homeautomationhub::bridge {
bool AnalysisHandler::handle(const HubMessage& message) const {
    if (message.category != HubCategory::AnalysisResult) return false;
    Logger::instance().info(
        "HubProtocol.Analysis",
        "Analysis result event=" + message.data.value("event", "") + " module=" + message.source.value("module", "")
    );
    return true;
}
}
