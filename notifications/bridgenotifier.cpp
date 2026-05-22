#include "bridgenotifier.h"

#include "../logging/logger.h"

bool BridgeNotifier::publish(const Notification& notification) const {
    Logger::instance().debug(
        "BridgeNotifier",
        "Legacy bridge notification suppressed id=" + notification.id +
        "; outbound Bridge communication must use Hub Protocol."
    );
    return false;
}
