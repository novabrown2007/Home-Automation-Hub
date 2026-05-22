#pragma once

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
class BridgeErrorHandler {
public:
    bool handle(const HubMessage& message) const;
};
}
