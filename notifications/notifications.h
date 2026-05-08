#pragma once

#include <mutex>
#include <string>
#include <vector>

struct Notification {
    std::string id;
    std::string source;
    std::string severity;
    std::string category;
    std::string title;
    std::string message;
    std::string deviceId;
    std::string createdAt;
};

class NotificationCenter {
public:
    Notification enqueue(
        const std::string& source,
        const std::string& severity,
        const std::string& category,
        const std::string& title,
        const std::string& message,
        const std::string& deviceId = ""
    );

    [[nodiscard]] std::vector<Notification> list() const;
    [[nodiscard]] std::size_t count() const;
    [[nodiscard]] static std::string currentTimestamp();

private:
    mutable std::mutex mutex;
    std::vector<Notification> notifications;
    std::size_t nextId{1};
};
