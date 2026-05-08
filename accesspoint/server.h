#pragma once

#include <string>
#include <winsock2.h>

#include "api.h"

class Server {
public:
    explicit Server(API& api);

    void start(int port);

private:
    API& api;

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
