#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace homeautomationhub::bridge {
using Json = nlohmann::json;

enum class HubCategory {
    DeviceState,
    DeviceEvent,
    DeviceTelemetry,
    StreamAvailable,
    StreamClosed,
    AnalysisResult,
    BridgeCommand,
    SubscriptionRequest,
    BridgeError
};

struct HubValidationError {
    std::string code;
    std::string message;
    std::string field;
};

struct HubValidationResult {
    std::vector<HubValidationError> errors;

    [[nodiscard]] bool ok() const {
        return errors.empty();
    }
};

/**
 * Normalized Hub Protocol envelope. Raw hardware packets and transport-specific
 * device data must never enter this boundary.
 */
struct HubMessage {
    std::string messageId;
    std::int64_t timestamp{0};
    HubCategory category{HubCategory::BridgeError};
    Json source = Json::object();
    Json target = Json::object();
    Json data = Json::object();

    [[nodiscard]] static HubMessage create(
        HubCategory category,
        Json source,
        Json data,
        Json target = Json::object()
    );
};

struct HubParseResult {
    std::optional<HubMessage> message;
    HubValidationResult validation;

    [[nodiscard]] bool ok() const {
        return message.has_value() && validation.ok();
    }
};

[[nodiscard]] std::string toString(HubCategory category);
[[nodiscard]] bool tryParseCategory(const std::string& value, HubCategory& category);
[[nodiscard]] Json toJson(const HubMessage& message);
[[nodiscard]] HubParseResult parseHubMessage(const Json& body);
[[nodiscard]] HubParseResult parseHubMessage(const std::string& body);
[[nodiscard]] HubValidationResult validateHubMessage(const HubMessage& message);
[[nodiscard]] std::int64_t unixTimestamp();
[[nodiscard]] std::string generateMessageId();
}
