#include "bridgenotifier.h"

#include "../network/httpclient.h"

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
}

BridgeNotifier::BridgeNotifier(std::string bridgeNotificationsUrl)
    : bridgeNotificationsUrl(std::move(bridgeNotificationsUrl)) {}

bool BridgeNotifier::publish(const Notification& notification) const {
    const std::string body =
        "{"
        "\"source\":\"" + escapeJson(notification.source) + "\","
        "\"severity\":\"" + escapeJson(notification.severity) + "\","
        "\"category\":\"" + escapeJson(notification.category) + "\","
        "\"title\":\"" + escapeJson(notification.title) + "\","
        "\"message\":\"" + escapeJson(notification.message) + "\","
        "\"device_id\":\"" + escapeJson(notification.deviceId) + "\""
        "}";
    return HttpClient::postJson(bridgeNotificationsUrl, body).ok();
}
