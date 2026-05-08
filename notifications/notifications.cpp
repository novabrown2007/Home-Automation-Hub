#include "notifications.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "../logging/logger.h"

Notification NotificationCenter::enqueue(
    const std::string& source,
    const std::string& severity,
    const std::string& category,
    const std::string& title,
    const std::string& message,
    const std::string& deviceId
) {
    std::lock_guard lock(mutex);
    Notification notification{
        .id = "notification-" + std::to_string(nextId++),
        .source = source,
        .severity = severity,
        .category = category,
        .title = title,
        .message = message,
        .deviceId = deviceId,
        .createdAt = currentTimestamp()
    };
    notifications.push_back(notification);

    Logger::instance().info(
        "Notifications",
        "Queued notification id=" + notification.id +
        " source=" + notification.source +
        " severity=" + notification.severity +
        " category=" + notification.category +
        " title=\"" + notification.title + "\"" +
        " device=" + notification.deviceId
    );

    return notification;
}

std::vector<Notification> NotificationCenter::list() const {
    std::lock_guard lock(mutex);
    return notifications;
}

std::size_t NotificationCenter::count() const {
    std::lock_guard lock(mutex);
    return notifications.size();
}

std::string NotificationCenter::currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream buffer;
    buffer << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return buffer.str();
}
