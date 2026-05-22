#pragma once

#include <string>
#include <winsock2.h>

#include "api.h"

namespace homeautomationhub::bridge {
class HubClient;
}

class Server {
public:
    explicit Server(API& api, homeautomationhub::bridge::HubClient* hubClient = nullptr);

    void start(int port);

private:
    API& api;
    homeautomationhub::bridge::HubClient* hubClient;

    void handleRequest(const std::string& request, SOCKET clientSocket);

    void sendResponse(
        SOCKET clientSocket,
        const std::string& body,
        const std::string& contentType = "application/json",
        int statusCode = 200,
        const std::string& statusText = "OK"
    );

    void sendResponse(
        SOCKET clientSocket,
        const std::string& body,
        int statusCode,
        const std::string& statusText
    );
};
