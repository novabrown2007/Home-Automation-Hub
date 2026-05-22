#include "hubClient.h"

#include <utility>

#include "../logging/logger.h"
#include "../network/httpclient.h"

namespace homeautomationhub::bridge {
namespace {
bool postWithHttpClient(const std::string& url, const std::string& jsonBody, std::string& error) {
    const HttpResponse response = HttpClient::postJson(url, jsonBody);
    if (response.ok()) return true;
    error = "statusCode=" + std::to_string(response.statusCode) + " statusText=" + response.statusText;
    return false;
}

std::string validationMessage(const HubValidationResult& validation) {
    if (validation.errors.empty()) return "invalid Hub Protocol message";
    const HubValidationError& error = validation.errors.front();
    return error.code + " field=" + error.field + " message=" + error.message;
}
}

HubClient::HubClient(HubClientConfig config, MessageRouter& router, SubscriptionManager& subscriptions, Sender sender)
    : clientConfig(std::move(config)),
      router(router),
      subscriptions(subscriptions),
      sender(sender ? std::move(sender) : Sender(postWithHttpClient)) {}

bool HubClient::connect() {
    Logger::instance().info("HubProtocol.Client", "Connecting to Bridge Hub Protocol endpoint=" + clientConfig.bridgeProtocolUrl);
    return sendSubscriptionRequest();
}

bool HubClient::heartbeat() {
    if (connected.load()) return true;
    Logger::instance().debug("HubProtocol.Client", "Retrying Bridge Hub Protocol connection.");
    return sendSubscriptionRequest();
}

bool HubClient::receive(const std::string& jsonBody) {
    const HubParseResult parsed = parseHubMessage(jsonBody);
    if (!parsed.ok()) {
        Logger::instance().warning("HubProtocol.Client", "Rejected inbound Hub Protocol envelope " + validationMessage(parsed.validation));
        return false;
    }
    Logger::instance().debug("HubProtocol.Client", "Received category=" + toString(parsed.message->category) + " messageId=" + parsed.message->messageId);
    return router.route(*parsed.message);
}

bool HubClient::send(const HubMessage& message) {
    const HubValidationResult validation = validateHubMessage(message);
    if (!validation.ok()) {
        setFailure(validationMessage(validation));
        Logger::instance().warning("HubProtocol.Client", "Rejected outbound Hub Protocol envelope " + lastError());
        return false;
    }
    if (!connected.load() && message.category != HubCategory::SubscriptionRequest) {
        setFailure("Bridge Hub Protocol is disconnected; outbound envelope is waiting for subscription reconnect.");
        Logger::instance().warning(
            "HubProtocol.Client",
            "Skipped outbound category=" + toString(message.category) + " while Bridge protocol connection is offline."
        );
        return false;
    }
    std::string error;
    const bool accepted = sender(clientConfig.bridgeProtocolUrl, toJson(message).dump(), error);
    connected.store(accepted);
    if (!accepted) {
        setFailure(error);
        Logger::instance().warning(
            "HubProtocol.Client",
            "Bridge rejected outbound category=" + toString(message.category) + " error=" + lastError()
        );
        return false;
    }
    Logger::instance().info(
        "HubProtocol.Client",
        "Sent category=" + toString(message.category) + " messageId=" + message.messageId
    );
    return true;
}

bool HubClient::sendAnalysisResult(const std::string& module, Json resultData) {
    if (!resultData.is_object()) {
        setFailure("analysis result data must be an object");
        return false;
    }
    HubMessage message = HubMessage::create(HubCategory::AnalysisResult, Json{{"module", module}}, std::move(resultData));
    return send(message);
}

bool HubClient::sendCommand(const HubMessage& command) {
    if (command.category != HubCategory::BridgeCommand) {
        setFailure("command envelope must use bridge.command category");
        return false;
    }
    Logger::instance().info(
        "HubProtocol.Command",
        "Sending command=" + command.data.value("command", "") + " target=" + command.target.value("deviceId", "")
    );
    return send(command);
}

bool HubClient::sendCommand(const std::string& deviceId, const std::string& command, Json arguments) {
    std::string error;
    const auto envelope = CommandBuilder::deviceCommand(deviceId, command, std::move(arguments), &error);
    if (!envelope.has_value()) {
        setFailure(error);
        Logger::instance().warning("HubProtocol.Command", "Rejected command error=" + error);
        return false;
    }
    return sendCommand(*envelope);
}

bool HubClient::isConnected() const {
    return connected.load();
}

std::string HubClient::lastError() const {
    std::lock_guard lock(errorMutex);
    return failure;
}

const HubClientConfig& HubClient::config() const {
    return clientConfig;
}

bool HubClient::sendSubscriptionRequest() {
    Json categories = Json::array();
    for (const std::string& pattern : subscriptions.categoryPatterns()) {
        categories.push_back(pattern);
    }
    if (categories.empty()) {
        categories = Json::array({"device.*", "stream.*", "bridge.error"});
    }
    HubMessage subscription = HubMessage::create(
        HubCategory::SubscriptionRequest,
        hubSource(),
        Json{{"categories", std::move(categories)}}
    );
    const bool subscribed = send(subscription);
    Logger::instance().info(
        "HubProtocol.Subscription",
        std::string(subscribed ? "Subscription request accepted" : "Subscription request pending") +
        " endpoint=" + clientConfig.bridgeProtocolUrl
    );
    return subscribed;
}

void HubClient::setFailure(const std::string& error) {
    std::lock_guard lock(errorMutex);
    failure = error;
}

Json HubClient::hubSource() const {
    return Json{{"module", "hub"}, {"hubId", clientConfig.hubId}};
}
}
