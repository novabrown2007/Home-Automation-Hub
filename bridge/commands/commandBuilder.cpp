#include "commandBuilder.h"

namespace homeautomationhub::bridge {
std::optional<HubMessage> CommandBuilder::deviceCommand(
    const std::string& deviceId,
    const std::string& command,
    Json arguments,
    std::string* error
) {
    if (deviceId.empty() || command.empty()) {
        if (error != nullptr) *error = "deviceId and command are required";
        return std::nullopt;
    }
    if (!arguments.is_object()) {
        if (error != nullptr) *error = "command arguments must be a JSON object";
        return std::nullopt;
    }
    HubMessage message = HubMessage::create(
        HubCategory::BridgeCommand,
        Json{{"module", "orchestration"}},
        Json{{"command", command}, {"arguments", std::move(arguments)}},
        Json{{"deviceId", deviceId}}
    );
    const HubValidationResult validation = validateHubMessage(message);
    if (!validation.ok()) {
        if (error != nullptr) *error = validation.errors.front().message;
        return std::nullopt;
    }
    return message;
}
}
