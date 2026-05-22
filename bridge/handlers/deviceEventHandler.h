#pragma once

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
class DeviceEventHandler {
public:
    bool handle(const HubMessage& message) const;
};
}
