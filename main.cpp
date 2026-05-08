#include "accesspoint/api.h"
#include "accesspoint/server.h"
#include "logging/logger.h"
#include "modules/cameras/cameraautomation.h"
#include "modules/cameras/cameras.h"
#include "threading/threadmanager.h"

#include <chrono>
#include <iostream>

int main() {
    Logger::instance().initialize("logs/home-automation-hub.log");
    Logger::instance().info("Main", "Home Automation Hub starting.");

    ThreadManager threadManager;
    threadManager.registerBackgroundJob({
        .name = "bridge-poll",
        .interval = std::chrono::milliseconds(1000),
        .task = []() {
            Logger::instance().debug("Main", "Background bridge poll tick.");
        },
        .runImmediately = true
    });
    threadManager.start();

    Cameras cameras;
    CameraAutomation cameraAutomation(cameras, threadManager);
    threadManager.registerBackgroundJob({
        .name = "camera-analysis",
        .interval = std::chrono::milliseconds(2000),
        .task = [&cameraAutomation]() {
            cameraAutomation.processAllFeeds();
        },
        .runImmediately = false
    });
    Logger::instance().info("Main", "Camera automation background jobs registered.");
    API api(cameras, cameraAutomation);
    Server server(api);
    server.start(8081);
    Logger::instance().info("Main", "Home Automation Hub shutting down.");
    return 0;
}
