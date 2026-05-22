#pragma once

#include <optional>
#include <string>

#include "../hubProtocol.h"

namespace homeautomationhub::bridge {
/**
 * Constructs orchestration commands for the Bridge. Command callers express
 * normalized intent and never expose hardware transport or driver details.
 */
class CommandBuilder {
public:
    [[nodiscard]] static std::optional<HubMessage> deviceCommand(
        const std::string& deviceId,
        const std::string& command,
        Json arguments,
        std::string* error = nullptr
    );
};
}
