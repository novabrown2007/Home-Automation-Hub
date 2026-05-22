#pragma once

#include "../hubProtocol.h"
#include "../streams/streamRegistry.h"

namespace homeautomationhub::bridge {
class StreamHandler {
public:
    explicit StreamHandler(StreamRegistry& streamRegistry);
    bool handle(const HubMessage& message) const;

private:
    StreamRegistry& streamRegistry;
};
}
