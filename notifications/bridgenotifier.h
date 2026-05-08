#pragma once

#include <string>

#include "notifications.h"

class BridgeNotifier {
public:
    explicit BridgeNotifier(std::string bridgeNotificationsUrl);

    bool publish(const Notification& notification) const;

private:
    std::string bridgeNotificationsUrl;
};
