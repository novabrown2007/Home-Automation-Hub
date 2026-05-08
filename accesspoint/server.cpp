#include "server.h"

#include <iostream>
#include <sstream>
#include <string>

#include "../logging/logger.h"

Server::Server(API& api) : api(api) {}

namespace {
std::string escapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }

    return escaped;
}

std::string extractRequestBody(const std::string& request) {
    const std::string separator = "\r\n\r\n";
    const size_t pos = request.find(separator);
    if (pos == std::string::npos) {
        return "";
    }
    return request.substr(pos + separator.size());
}

std::string extractJsonStringField(const std::string& body, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t keyPos = body.find(token);
    if (keyPos == std::string::npos) {
        return "";
    }

    const size_t colonPos = body.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return "";
    }

    const size_t firstQuote = body.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return "";
    }

    const size_t secondQuote = body.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return "";
    }

    return body.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::string serializeCameraFeed(const CameraFeed& feed) {
    std::ostringstream body;
    body << "{"
         << "\"camera_id\":\"" << escapeJson(feed.cameraId) << "\","
         << "\"raw_feed_url\":\"" << escapeJson(feed.rawFeedUrl) << "\","
         << "\"stream_url\":\"" << escapeJson(feed.streamUrl) << "\","
         << "\"resolution\":\"" << escapeJson(feed.resolution) << "\","
         << "\"processing_status\":\"" << escapeJson(feed.processingStatus) << "\","
         << "\"mode\":\"" << escapeJson(Cameras::modeToString(feed.modeProfile.mode)) << "\","
         << "\"mode_description\":\"" << escapeJson(feed.modeProfile.description) << "\","
         << "\"raw_feed_visible\":" << (feed.modeProfile.analyzers.rawFeedVisible ? "true" : "false") << ","
         << "\"motion_detection_enabled\":" << (feed.modeProfile.analyzers.motionDetectionEnabled ? "true" : "false") << ","
         << "\"facial_recognition_enabled\":" << (feed.modeProfile.analyzers.facialRecognitionEnabled ? "true" : "false") << ","
         << "\"strange_sound_detection_enabled\":" << (feed.modeProfile.analyzers.strangeSoundDetectionEnabled ? "true" : "false") << ","
         << "\"motion_detected\":" << (feed.detections.motionDetected ? "true" : "false") << ","
         << "\"familiar_face_detected\":" << (feed.detections.familiarFaceDetected ? "true" : "false") << ","
         << "\"unknown_face_detected\":" << (feed.detections.unknownFaceDetected ? "true" : "false") << ","
         << "\"strange_sound_detected\":" << (feed.detections.strangeSoundDetected ? "true" : "false") << ","
         << "\"last_event\":\"" << escapeJson(feed.detections.lastEvent) << "\","
         << "\"last_processed_at\":\"" << escapeJson(feed.detections.lastProcessedAt) << "\""
         << "}";
    return body.str();
}
}

void Server::start(int port) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress));
    listen(serverSocket, 10);

    Logger::instance().info("Server", "Home Automation Hub API starting on port " + std::to_string(port));

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

        char buffer[4096] = {};
        recv(clientSocket, buffer, sizeof(buffer), 0);
        Logger::instance().debug("Server", "Received raw request bytes from client.");

        handleRequest(std::string(buffer), clientSocket);

        closesocket(clientSocket);
    }

    closesocket(serverSocket);
    WSACleanup();
}

void Server::handleRequest(const std::string& request, SOCKET clientSocket) {
    std::istringstream stream(request);

    std::string method;
    std::string path;

    stream >> method >> path;
    Logger::instance().info("Server", "Handling request method=" + method + " path=" + path);

    if (method == "GET" && path == "/status") {
        const std::string body =
            "{ \"service\": \"" + escapeJson(api.getStatus()) + "\", \"status\": \"ok\", \"registered_camera_feeds\": "
            + std::to_string(api.getRegisteredCameraFeedCount()) + " }";
        sendResponse(clientSocket, body);
        return;
    }

    if (method == "GET" && path == "/camera/feeds") {
        const auto feeds = api.getAllCameraFeeds();
        std::string body = "{ \"camera_feeds\": [";
        for (std::size_t index = 0; index < feeds.size(); ++index) {
            body += serializeCameraFeed(feeds[index]);
            if (index + 1 < feeds.size()) {
                body += ",";
            }
        }
        body += "] }";
        sendResponse(clientSocket, body);
        return;
    }

    if (method == "GET" && path.rfind("/camera/feed/", 0) == 0) {
        const std::string cameraId = path.substr(std::string("/camera/feed/").length());
        const CameraFeed feed = api.getCameraFeed(cameraId);
        if (feed.cameraId.empty()) {
            Logger::instance().warning("Server", "Requested unknown camera feed camera=" + cameraId);
            sendResponse(clientSocket, "{ \"error\": \"camera not found\" }", 404, "Not Found");
            return;
        }

        sendResponse(clientSocket, serializeCameraFeed(feed));
        return;
    }

    if (method == "POST" && path.rfind("/camera/feed/", 0) == 0) {
        const std::string cameraId = path.substr(std::string("/camera/feed/").length());
        const std::string body = extractRequestBody(request);
        const std::string rawFeedUrl = extractJsonStringField(body, "raw_feed_url");
        std::string streamUrl = extractJsonStringField(body, "stream_url");
        const std::string resolution = extractJsonStringField(body, "resolution");

        if (streamUrl.empty()) {
            streamUrl = rawFeedUrl;
        }

        if (!api.registerCameraFeed(cameraId, rawFeedUrl, streamUrl, resolution)) {
            Logger::instance().warning("Server", "Rejected camera feed POST camera=" + cameraId);
            sendResponse(clientSocket, "{ \"error\": \"invalid camera feed payload\" }", 400, "Bad Request");
            return;
        }

        sendResponse(clientSocket, "{ \"status\": \"ok\" }");
        return;
    }

    if (method == "POST" && path.rfind("/camera/mode/", 0) == 0) {
        const std::string cameraId = path.substr(std::string("/camera/mode/").length());
        const std::string body = extractRequestBody(request);
        const std::string mode = extractJsonStringField(body, "mode");

        if (mode.empty()) {
            Logger::instance().warning("Server", "Rejected camera mode POST camera=" + cameraId + " because mode was missing.");
            sendResponse(clientSocket, "{ \"error\": \"missing mode\" }", 400, "Bad Request");
            return;
        }

        if (!api.setCameraMode(cameraId, mode)) {
            Logger::instance().warning("Server", "Rejected camera mode POST camera=" + cameraId + " mode=" + mode);
            sendResponse(clientSocket, "{ \"error\": \"invalid camera id or mode\" }", 400, "Bad Request");
            return;
        }

        sendResponse(clientSocket, "{ \"status\": \"ok\" }");
        return;
    }

    Logger::instance().warning("Server", "Unknown route method=" + method + " path=" + path);
    sendResponse(clientSocket, "{ \"error\": \"unknown route\" }", 404, "Not Found");
}

void Server::sendResponse(
    SOCKET clientSocket,
    const std::string& body,
    int statusCode,
    const std::string& statusText
) {
    Logger::instance().debug(
        "Server",
        "Sending response statusCode=" + std::to_string(statusCode) +
        " statusText=" + statusText +
        " bodyLength=" + std::to_string(body.size())
    );
    const std::string response =
        "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" +
        body;

    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
}
