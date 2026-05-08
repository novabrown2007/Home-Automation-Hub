#pragma once

#include <string>
#include <unordered_map>

struct HttpResponse {
    int statusCode{0};
    std::string statusText;
    std::string contentType{"application/octet-stream"};
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    [[nodiscard]] bool ok() const {
        return statusCode >= 200 && statusCode < 300;
    }
};

class HttpClient {
public:
    static HttpResponse get(const std::string& url);
    static HttpResponse postJson(const std::string& url, const std::string& jsonBody);
};
