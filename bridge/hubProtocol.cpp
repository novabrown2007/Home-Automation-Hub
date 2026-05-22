#include "hubProtocol.h"

#include <array>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>

namespace homeautomationhub::bridge {
namespace {
void addError(HubValidationResult& result, std::string code, std::string message, std::string field) {
    result.errors.push_back(HubValidationError{std::move(code), std::move(message), std::move(field)});
}

bool hasString(const Json& value, const char* key) {
    return value.is_object() && value.contains(key) && value.at(key).is_string() && !value.at(key).get<std::string>().empty();
}

void requireObject(HubValidationResult& result, const Json& value, const char* field) {
    if (!value.is_object()) {
        addError(result, "INVALID_FIELD_TYPE", std::string(field) + " must be an object.", field);
    }
}

void requireString(HubValidationResult& result, const Json& value, const char* field, const char* key) {
    if (!hasString(value, key)) {
        addError(result, "MISSING_FIELD", std::string(field) + "." + key + " must be a non-empty string.", std::string(field) + "." + key);
    }
}
}

std::string toString(HubCategory category) {
    switch (category) {
        case HubCategory::DeviceState: return "device.state";
        case HubCategory::DeviceEvent: return "device.event";
        case HubCategory::DeviceTelemetry: return "device.telemetry";
        case HubCategory::StreamAvailable: return "stream.available";
        case HubCategory::StreamClosed: return "stream.closed";
        case HubCategory::AnalysisResult: return "analysis.result";
        case HubCategory::BridgeCommand: return "bridge.command";
        case HubCategory::SubscriptionRequest: return "subscription.request";
        case HubCategory::BridgeError: return "bridge.error";
    }
    return "bridge.error";
}

bool tryParseCategory(const std::string& value, HubCategory& category) {
    static const std::unordered_map<std::string, HubCategory> categories{
        {"device.state", HubCategory::DeviceState},
        {"device.event", HubCategory::DeviceEvent},
        {"device.telemetry", HubCategory::DeviceTelemetry},
        {"stream.available", HubCategory::StreamAvailable},
        {"stream.closed", HubCategory::StreamClosed},
        {"analysis.result", HubCategory::AnalysisResult},
        {"bridge.command", HubCategory::BridgeCommand},
        {"subscription.request", HubCategory::SubscriptionRequest},
        {"bridge.error", HubCategory::BridgeError}
    };
    const auto found = categories.find(value);
    if (found == categories.end()) {
        return false;
    }
    category = found->second;
    return true;
}

HubMessage HubMessage::create(HubCategory category, Json source, Json data, Json target) {
    return HubMessage{
        .messageId = generateMessageId(),
        .timestamp = unixTimestamp(),
        .category = category,
        .source = source.is_object() ? std::move(source) : Json::object(),
        .target = target.is_object() ? std::move(target) : Json::object(),
        .data = data.is_object() ? std::move(data) : Json::object()
    };
}

Json toJson(const HubMessage& message) {
    Json body{{"messageId", message.messageId}, {"timestamp", message.timestamp}, {"category", toString(message.category)},
              {"source", message.source}, {"data", message.data}};
    if (!message.target.empty()) {
        body["target"] = message.target;
    }
    return body;
}

HubParseResult parseHubMessage(const Json& body) {
    HubParseResult parsed;
    if (!body.is_object()) {
        addError(parsed.validation, "INVALID_JSON", "Hub Protocol message must be a JSON object.", "");
        return parsed;
    }
    if (!hasString(body, "category")) {
        addError(parsed.validation, "MISSING_FIELD", "category must be a non-empty string.", "category");
        return parsed;
    }
    HubCategory category{};
    if (!tryParseCategory(body.at("category").get<std::string>(), category)) {
        addError(parsed.validation, "INVALID_CATEGORY", "category is not recognized by the Hub Protocol.", "category");
        return parsed;
    }
    HubMessage message{
        .messageId = hasString(body, "messageId") ? body.at("messageId").get<std::string>() : generateMessageId(),
        .timestamp = body.contains("timestamp") && body.at("timestamp").is_number_integer() ? body.at("timestamp").get<std::int64_t>() : unixTimestamp(),
        .category = category,
        .source = body.value("source", Json::object()),
        .target = body.value("target", Json::object()),
        .data = body.value("data", Json::object())
    };
    parsed.validation = validateHubMessage(message);
    if (parsed.validation.ok()) {
        parsed.message = std::move(message);
    }
    return parsed;
}

HubParseResult parseHubMessage(const std::string& body) {
    try {
        return parseHubMessage(Json::parse(body));
    } catch (const Json::exception& ex) {
        HubParseResult parsed;
        addError(parsed.validation, "INVALID_JSON", ex.what(), "");
        return parsed;
    }
}

HubValidationResult validateHubMessage(const HubMessage& message) {
    HubValidationResult result;
    if (message.messageId.empty()) addError(result, "MISSING_FIELD", "messageId must be present.", "messageId");
    if (message.timestamp <= 0) addError(result, "INVALID_FIELD_VALUE", "timestamp must be a positive unix timestamp.", "timestamp");
    requireObject(result, message.source, "source");
    requireObject(result, message.target, "target");
    requireObject(result, message.data, "data");
    if (!result.ok()) return result;

    switch (message.category) {
        case HubCategory::DeviceState:
        case HubCategory::DeviceTelemetry:
            requireString(result, message.source, "source", "deviceId");
            break;
        case HubCategory::DeviceEvent:
            requireString(result, message.source, "source", "deviceId");
            requireString(result, message.data, "data", "event");
            break;
        case HubCategory::StreamAvailable:
            requireString(result, message.source, "source", "deviceId");
            requireString(result, message.data, "data", "streamId");
            requireString(result, message.data, "data", "streamType");
            requireString(result, message.data, "data", "endpoint");
            break;
        case HubCategory::StreamClosed:
            requireString(result, message.data, "data", "streamId");
            break;
        case HubCategory::AnalysisResult:
            requireString(result, message.source, "source", "module");
            requireString(result, message.data, "data", "event");
            break;
        case HubCategory::BridgeCommand:
            requireString(result, message.target, "target", "deviceId");
            requireString(result, message.data, "data", "command");
            if (message.data.contains("arguments") && !message.data.at("arguments").is_object()) {
                addError(result, "INVALID_FIELD_TYPE", "data.arguments must be an object.", "data.arguments");
            }
            break;
        case HubCategory::SubscriptionRequest:
            if (!message.data.contains("categories") || !message.data.at("categories").is_array()) {
                addError(result, "MISSING_FIELD", "data.categories must be an array.", "data.categories");
            }
            break;
        case HubCategory::BridgeError:
            break;
    }
    return result;
}

std::int64_t unixTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string generateMessageId() {
    std::array<unsigned char, 16> bytes{};
    std::random_device random;
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
    std::ostringstream buffer;
    buffer << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        buffer << std::setw(2) << static_cast<int>(bytes[index]);
        if (index == 3 || index == 5 || index == 7 || index == 9) buffer << "-";
    }
    return buffer.str();
}
}
