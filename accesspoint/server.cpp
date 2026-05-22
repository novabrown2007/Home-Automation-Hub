#include "server.h"

#include <iostream>
#include <sstream>
#include <string>

#include "../logging/logger.h"
#include "../bridge/hubClient.h"

Server::Server(API& api, homeautomationhub::bridge::HubClient* hubClient) : api(api), hubClient(hubClient) {}

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

std::size_t extractContentLength(const std::string& request) {
    const std::string token = "Content-Length:";
    const std::size_t headerPos = request.find(token);
    if (headerPos == std::string::npos) {
        return 0;
    }

    const std::size_t valueStart = request.find_first_of("0123456789", headerPos + token.size());
    if (valueStart == std::string::npos) {
        return 0;
    }

    const std::size_t valueEnd = request.find_first_not_of("0123456789", valueStart);
    try {
        return static_cast<std::size_t>(std::stoul(request.substr(valueStart, valueEnd - valueStart)));
    } catch (...) {
        return 0;
    }
}

std::string receiveRequest(SOCKET clientSocket) {
    std::string request;
    char buffer[4096];
    std::size_t expectedBodyLength = 0;
    bool headersParsed = false;

    while (true) {
        const int received = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }

        request.append(buffer, received);

        if (!headersParsed) {
            const std::size_t separatorPos = request.find("\r\n\r\n");
            if (separatorPos != std::string::npos) {
                headersParsed = true;
                expectedBodyLength = extractContentLength(request);
                const std::size_t currentBodyLength = request.size() - (separatorPos + 4);
                if (currentBodyLength >= expectedBodyLength) {
                    break;
                }
            }
        } else {
            const std::size_t separatorPos = request.find("\r\n\r\n");
            const std::size_t currentBodyLength = separatorPos == std::string::npos ? 0 : request.size() - (separatorPos + 4);
            if (currentBodyLength >= expectedBodyLength) {
                break;
            }
        }

        if (received < static_cast<int>(sizeof(buffer)) && headersParsed && expectedBodyLength == 0) {
            break;
        }
    }

    return request;
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
         << "\"source_feed_url\":\"" << escapeJson(feed.sourceFeedUrl) << "\","
         << "\"raw_feed_url\":\"" << escapeJson(feed.rawFeedUrl) << "\","
         << "\"stream_url\":\"" << escapeJson(feed.streamUrl) << "\","
         << "\"frame_url\":\"" << escapeJson(feed.frameUrl) << "\","
         << "\"audio_feed_url\":\"" << escapeJson(feed.audioFeedUrl) << "\","
         << "\"resolution\":\"" << escapeJson(feed.resolution) << "\","
         << "\"processing_status\":\"" << escapeJson(feed.processingStatus) << "\","
         << "\"bridge_stream_enabled\":" << (feed.bridgeStreamEnabled ? "true" : "false") << ","
         << "\"mode\":\"" << escapeJson(Cameras::modeToString(feed.modeProfile.mode)) << "\","
         << "\"mode_description\":\"" << escapeJson(feed.modeProfile.description) << "\","
         << "\"raw_feed_visible\":" << (feed.modeProfile.analyzers.rawFeedVisible ? "true" : "false") << ","
         << "\"motion_detection_enabled\":" << (feed.modeProfile.analyzers.motionDetectionEnabled ? "true" : "false") << ","
         << "\"facial_recognition_enabled\":" << (feed.modeProfile.analyzers.facialRecognitionEnabled ? "true" : "false") << ","
         << "\"strange_sound_detection_enabled\":" << (feed.modeProfile.analyzers.strangeSoundDetectionEnabled ? "true" : "false") << ","
         << "\"notify_on_motion\":" << (feed.modeProfile.alerts.notifyOnMotion ? "true" : "false") << ","
         << "\"notify_on_familiar_face\":" << (feed.modeProfile.alerts.notifyOnFamiliarFace ? "true" : "false") << ","
         << "\"notify_on_unknown_face\":" << (feed.modeProfile.alerts.notifyOnUnknownFace ? "true" : "false") << ","
         << "\"notify_on_strange_sound\":" << (feed.modeProfile.alerts.notifyOnStrangeSound ? "true" : "false") << ","
         << "\"motion_detected\":" << (feed.detections.motionDetected ? "true" : "false") << ","
         << "\"familiar_face_detected\":" << (feed.detections.familiarFaceDetected ? "true" : "false") << ","
         << "\"unknown_face_detected\":" << (feed.detections.unknownFaceDetected ? "true" : "false") << ","
         << "\"strange_sound_detected\":" << (feed.detections.strangeSoundDetected ? "true" : "false") << ","
         << "\"motion_score\":" << feed.detections.motionScore << ","
         << "\"sound_score\":" << feed.detections.soundScore << ","
         << "\"last_frame_bytes\":" << feed.detections.lastFrameBytes << ","
         << "\"last_audio_bytes\":" << feed.detections.lastAudioBytes << ","
         << "\"last_event\":\"" << escapeJson(feed.detections.lastEvent) << "\","
         << "\"last_processed_at\":\"" << escapeJson(feed.detections.lastProcessedAt) << "\""
         << "}";
    return body.str();
}

std::string serializeNotification(const Notification& notification) {
    std::ostringstream body;
    body << "{"
         << "\"id\":\"" << escapeJson(notification.id) << "\","
         << "\"source\":\"" << escapeJson(notification.source) << "\","
         << "\"severity\":\"" << escapeJson(notification.severity) << "\","
         << "\"category\":\"" << escapeJson(notification.category) << "\","
         << "\"title\":\"" << escapeJson(notification.title) << "\","
         << "\"message\":\"" << escapeJson(notification.message) << "\","
         << "\"device_id\":\"" << escapeJson(notification.deviceId) << "\","
         << "\"created_at\":\"" << escapeJson(notification.createdAt) << "\""
         << "}";
    return body.str();
}
}

void Server::start(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::instance().error("Server", "WSAStartup failed.");
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        Logger::instance().error("Server", "socket() failed with error=" + std::to_string(WSAGetLastError()));
        WSACleanup();
        return;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        Logger::instance().error("Server", "bind() failed on port " + std::to_string(port) + " error=" + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        Logger::instance().error("Server", "listen() failed on port " + std::to_string(port) + " error=" + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    Logger::instance().info("Server", "Home Automation Hub API starting on port " + std::to_string(port));

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            Logger::instance().warning("Server", "accept() failed error=" + std::to_string(WSAGetLastError()));
            continue;
        }

        const std::string request = receiveRequest(clientSocket);
        Logger::instance().debug("Server", "Received raw request bytes from client.");

        handleRequest(request, clientSocket);

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

    if (method == "POST" && path == "/hub/messages") {
        if (hubClient == nullptr) {
            sendResponse(clientSocket, "{ \"error\": \"hub protocol client unavailable\" }", 503, "Service Unavailable");
            return;
        }
        if (!hubClient->receive(extractRequestBody(request))) {
            sendResponse(clientSocket, "{ \"error\": \"invalid hub protocol message\" }", 400, "Bad Request");
            return;
        }
        sendResponse(clientSocket, "{ \"status\": \"accepted\" }", 202, "Accepted");
        return;
    }

    if (path.rfind("/camera/", 0) == 0 || path == "/notifications") {
        Logger::instance().warning(
            "Server",
            "Rejected legacy orchestration route path=" + path + "; use Hub Protocol /hub/messages."
        );
        sendResponse(clientSocket, "{ \"error\": \"legacy route disabled; use hub protocol\" }", 410, "Gone");
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
        const std::string sourceFeedUrl = extractJsonStringField(body, "source_feed_url");
        const std::string rawFeedUrl = extractJsonStringField(body, "raw_feed_url");
        std::string streamUrl = extractJsonStringField(body, "stream_url");
        std::string frameUrl = extractJsonStringField(body, "frame_url");
        const std::string audioFeedUrl = extractJsonStringField(body, "audio_feed_url");
        const std::string resolution = extractJsonStringField(body, "resolution");

        if (streamUrl.empty()) {
            streamUrl = rawFeedUrl;
        }

        if (frameUrl.empty()) {
            frameUrl = streamUrl;
        }

        if (!api.registerCameraFeed(cameraId, sourceFeedUrl, rawFeedUrl, streamUrl, frameUrl, audioFeedUrl, resolution)) {
            Logger::instance().warning("Server", "Rejected camera feed POST camera=" + cameraId);
            sendResponse(clientSocket, "{ \"error\": \"invalid camera feed payload\" }", 400, "Bad Request");
            return;
        }

        sendResponse(clientSocket, "{ \"status\": \"ok\" }");
        return;
    }

    if (method == "GET" && path == "/notifications") {
        const auto notifications = api.getNotifications();
        std::string body = "{ \"notifications\": [";
        for (std::size_t index = 0; index < notifications.size(); ++index) {
            body += serializeNotification(notifications[index]);
            if (index + 1 < notifications.size()) {
                body += ",";
            }
        }
        body += "] }";
        sendResponse(clientSocket, body);
        return;
    }

    if (method == "POST" && path == "/notifications") {
        const std::string body = extractRequestBody(request);
        const Notification notification = api.queueNotification(
            extractJsonStringField(body, "source").empty() ? "hub" : extractJsonStringField(body, "source"),
            extractJsonStringField(body, "severity").empty() ? "info" : extractJsonStringField(body, "severity"),
            extractJsonStringField(body, "category").empty() ? "system" : extractJsonStringField(body, "category"),
            extractJsonStringField(body, "title"),
            extractJsonStringField(body, "message"),
            extractJsonStringField(body, "device_id")
        );
        sendResponse(clientSocket, "{ \"status\": \"ok\", \"notification_id\": \"" + escapeJson(notification.id) + "\" }");
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
    const std::string& contentType,
    int statusCode,
    const std::string& statusText
) {
    Logger::instance().debug(
        "Server",
        "Sending response statusCode=" + std::to_string(statusCode) +
        " statusText=" + statusText +
        " contentType=" + contentType +
        " bodyLength=" + std::to_string(body.size())
    );
    const std::string response =
        "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" +
        body;

    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
}

void Server::sendResponse(
    SOCKET clientSocket,
    const std::string& body,
    int statusCode,
    const std::string& statusText
) {
    sendResponse(clientSocket, body, "application/json", statusCode, statusText);
}
