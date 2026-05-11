#include "httpclient.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

namespace {
struct ParsedUrl {
    std::string host;
    int port{80};
    std::string path{"/"};
};

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool parseUrl(const std::string& url, ParsedUrl& parsed) {
    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::string withoutScheme = url.substr(prefix.size());
    const std::size_t slashPos = withoutScheme.find('/');
    const std::string hostPort = slashPos == std::string::npos ? withoutScheme : withoutScheme.substr(0, slashPos);
    parsed.path = slashPos == std::string::npos ? "/" : withoutScheme.substr(slashPos);

    const std::size_t colonPos = hostPort.find(':');
    if (colonPos == std::string::npos) {
        parsed.host = hostPort;
        parsed.port = 80;
        return !parsed.host.empty();
    }

    parsed.host = hostPort.substr(0, colonPos);
    if (parsed.host.empty()) {
        return false;
    }

    try {
        parsed.port = std::stoi(hostPort.substr(colonPos + 1));
    } catch (...) {
        return false;
    }

    return parsed.port > 0;
}

HttpResponse parseResponse(const std::string& rawResponse) {
    HttpResponse response;
    const std::string headerSeparator = "\r\n\r\n";
    const std::size_t headerEnd = rawResponse.find(headerSeparator);
    if (headerEnd == std::string::npos) {
        return response;
    }

    const std::string headerBlock = rawResponse.substr(0, headerEnd);
    response.body = rawResponse.substr(headerEnd + headerSeparator.size());

    std::istringstream headersStream(headerBlock);
    std::string statusLine;
    std::getline(headersStream, statusLine);
    if (!statusLine.empty() && statusLine.back() == '\r') {
        statusLine.pop_back();
    }

    {
        std::istringstream statusStream(statusLine);
        std::string httpVersion;
        statusStream >> httpVersion >> response.statusCode;
        std::getline(statusStream, response.statusText);
        response.statusText = trim(response.statusText);
    }

    std::string headerLine;
    while (std::getline(headersStream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }

        const std::size_t colonPos = headerLine.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string name = trim(headerLine.substr(0, colonPos));
        std::string value = trim(headerLine.substr(colonPos + 1));
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        response.headers[name] = value;
    }

    const auto contentTypeIt = response.headers.find("content-type");
    if (contentTypeIt != response.headers.end()) {
        response.contentType = contentTypeIt->second;
    }

    return response;
}

HttpResponse sendRequest(const std::string& method, const std::string& url, const std::string& body, const std::string& contentType) {
    ParsedUrl parsed;
    if (!parseUrl(url, parsed)) {
        return HttpResponse{
            .statusCode = 0,
            .statusText = "Unsupported URL",
        };
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* resolved = nullptr;
    const std::string portText = std::to_string(parsed.port);
    if (getaddrinfo(parsed.host.c_str(), portText.c_str(), &hints, &resolved) != 0) {
        return HttpResponse{
            .statusCode = 0,
            .statusText = "Name resolution failed",
        };
    }

    SOCKET clientSocket = INVALID_SOCKET;
    for (addrinfo* current = resolved; current != nullptr; current = current->ai_next) {
        clientSocket = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        if (connect(clientSocket, current->ai_addr, static_cast<int>(current->ai_addrlen)) != SOCKET_ERROR) {
            break;
        }

        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    freeaddrinfo(resolved);

    if (clientSocket == INVALID_SOCKET) {
        return HttpResponse{
            .statusCode = 0,
            .statusText = "Connect failed",
        };
    }

    std::ostringstream request;
    request << method << " " << parsed.path << " HTTP/1.1\r\n"
            << "Host: " << parsed.host << "\r\n"
            << "Connection: close\r\n";
    if (!body.empty()) {
        request << "Content-Type: " << contentType << "\r\n"
                << "Content-Length: " << body.size() << "\r\n";
    }
    request << "\r\n" << body;

    const std::string requestText = request.str();
    std::size_t sentTotal = 0;
    while (sentTotal < requestText.size()) {
        const int sent = send(
            clientSocket,
            requestText.data() + sentTotal,
            static_cast<int>(requestText.size() - sentTotal),
            0
        );
        if (sent == SOCKET_ERROR || sent <= 0) {
            closesocket(clientSocket);
            return HttpResponse{
                .statusCode = 0,
                .statusText = "Send failed",
            };
        }
        sentTotal += static_cast<std::size_t>(sent);
    }

    shutdown(clientSocket, SD_SEND);

    timeval timeout{};
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    std::string rawResponse;
    char buffer[4096];
    while (true) {
        const int received = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (received > 0) {
            rawResponse.append(buffer, received);
            continue;
        }

        if (received == 0) {
            break;
        }

        const int error = WSAGetLastError();
        closesocket(clientSocket);
        return HttpResponse{
            .statusCode = 0,
            .statusText = error == WSAETIMEDOUT ? "Receive timed out" : "Receive failed",
        };
    }
    closesocket(clientSocket);

    if (rawResponse.empty()) {
        return HttpResponse{
            .statusCode = 0,
            .statusText = "Empty response",
        };
    }

    return parseResponse(rawResponse);
}
}

HttpResponse HttpClient::get(const std::string& url) {
    return sendRequest("GET", url, "", "application/octet-stream");
}

HttpResponse HttpClient::postJson(const std::string& url, const std::string& jsonBody) {
    return sendRequest("POST", url, jsonBody, "application/json");
}
