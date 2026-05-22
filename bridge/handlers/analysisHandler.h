#pragma once

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
class AnalysisHandler {
public:
    bool handle(const HubMessage& message) const;
};
}
