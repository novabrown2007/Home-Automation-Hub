#pragma once

#include <string>

#include "notifications.h"

class BridgeNotifier {
public:
    BridgeNotifier() = default;

    bool publish(const Notification& notification) const;
};
