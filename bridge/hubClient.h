#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "hubProtocol.h"
#include "commands/commandBuilder.h"
#include "routing/messageRouter.h"
#include "subscriptions/subscriptionManager.h"

namespace homeautomationhub::bridge {
struct HubClientConfig {
    std::string bridgeProtocolUrl{"http://127.0.0.1:8080/hub/messages"};
    std::string hubId{"home-automation-hub"};
};

/**
 * Hub Protocol client and transport boundary. Today it posts JSON envelopes to
 * the Bridge protocol endpoint; reconnection, heartbeat, and inbound routing
 * stay here so future pub/sub transports do not leak into orchestration code.
 */
class HubClient {
public:
    using Sender = std::function<bool(const std::string& url, const std::string& jsonBody, std::string& error)>;

    HubClient(HubClientConfig config, MessageRouter& router, SubscriptionManager& subscriptions, Sender sender = {});

    bool connect();
    bool heartbeat();
    bool receive(const std::string& jsonBody);
    bool send(const HubMessage& message);
    bool sendAnalysisResult(const std::string& module, Json resultData);
    bool sendCommand(const HubMessage& command);
    bool sendCommand(const std::string& deviceId, const std::string& command, Json arguments);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] std::string lastError() const;
    [[nodiscard]] const HubClientConfig& config() const;

private:
    bool sendSubscriptionRequest();
    void setFailure(const std::string& error);
    [[nodiscard]] Json hubSource() const;

    HubClientConfig clientConfig;
    MessageRouter& router;
    SubscriptionManager& subscriptions;
    Sender sender;
    std::atomic<bool> connected{false};
    mutable std::mutex errorMutex;
    std::string failure;
};
}
