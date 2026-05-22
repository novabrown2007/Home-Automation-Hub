#include "accesspoint/api.h"
#include "accesspoint/server.h"
#include "bridge/handlers/analysisHandler.h"
#include "bridge/handlers/bridgeErrorHandler.h"
#include "bridge/handlers/deviceEventHandler.h"
#include "bridge/handlers/deviceStateHandler.h"
#include "bridge/handlers/deviceTelemetryHandler.h"
#include "bridge/handlers/streamHandler.h"
#include "bridge/hubClient.h"
#include "bridge/routing/messageRouter.h"
#include "bridge/state/bridgeStateCache.h"
#include "bridge/streams/streamRegistry.h"
#include "bridge/subscriptions/subscriptionManager.h"
#include "logging/logger.h"
#include "modules/cameras/cameraautomation.h"
#include "modules/cameras/cameras.h"
#include "notifications/bridgenotifier.h"
#include "notifications/notifications.h"
#include "threading/threadmanager.h"

#include <chrono>

int main() {
    Logger::instance().initialize("logs/home-automation-hub.log");
    Logger::instance().info("Main", "Home Automation Hub starting.");

    using namespace homeautomationhub::bridge;

    SubscriptionManager subscriptions;
    subscriptions.subscribe("device.*", [](const HubMessage&) {});
    subscriptions.subscribe("stream.*", [](const HubMessage&) {});
    subscriptions.subscribe("analysis.result", [](const HubMessage&) {});
    subscriptions.subscribe("bridge.error", [](const HubMessage&) {});

    BridgeStateCache bridgeStateCache;
    StreamRegistry streamRegistry;
    DeviceStateHandler deviceStateHandler(bridgeStateCache);
    DeviceEventHandler deviceEventHandler;
    DeviceTelemetryHandler deviceTelemetryHandler(bridgeStateCache);
    StreamHandler streamHandler(streamRegistry);
    AnalysisHandler analysisHandler;
    BridgeErrorHandler bridgeErrorHandler;

    MessageRouter messageRouter(subscriptions);
    messageRouter.registerHandler(HubCategory::DeviceState, [&deviceStateHandler](const HubMessage& message) {
        return deviceStateHandler.handle(message);
    });
    messageRouter.registerHandler(HubCategory::DeviceEvent, [&deviceEventHandler](const HubMessage& message) {
        return deviceEventHandler.handle(message);
    });
    messageRouter.registerHandler(HubCategory::DeviceTelemetry, [&deviceTelemetryHandler](const HubMessage& message) {
        return deviceTelemetryHandler.handle(message);
    });
    messageRouter.registerHandler(HubCategory::StreamAvailable, [&streamHandler](const HubMessage& message) {
        return streamHandler.handle(message);
    });
    messageRouter.registerHandler(HubCategory::StreamClosed, [&streamHandler](const HubMessage& message) {
        return streamHandler.handle(message);
    });
    messageRouter.registerHandler(HubCategory::AnalysisResult, [&analysisHandler](const HubMessage& message) {
        return analysisHandler.handle(message);
    });
    messageRouter.registerHandler(HubCategory::BridgeError, [&bridgeErrorHandler](const HubMessage& message) {
        return bridgeErrorHandler.handle(message);
    });
    HubClient hubClient({}, messageRouter, subscriptions);
    hubClient.connect();

    ThreadManager threadManager;
    threadManager.registerBackgroundJob({
        .name = "hub-protocol-health",
        .interval = std::chrono::milliseconds(30000),
        .task = [&hubClient]() {
            hubClient.heartbeat();
        },
        .runImmediately = false
    });
    threadManager.start();

    Cameras cameras;
    NotificationCenter notifications;
    BridgeNotifier bridgeNotifier;
    CameraAutomation cameraAutomation(cameras, threadManager, notifications, bridgeNotifier);
    threadManager.registerBackgroundJob({
        .name = "camera-analysis",
        .interval = std::chrono::milliseconds(2000),
        .task = [&cameraAutomation]() {
            cameraAutomation.processAllFeeds();
        },
        .runImmediately = false
    });
    Logger::instance().info("Main", "Camera automation background jobs registered.");
    API api(cameras, cameraAutomation, notifications);
    Server server(api, &hubClient);
    server.start(8081);
    Logger::instance().info("Main", "Home Automation Hub shutting down.");
    return 0;
}
