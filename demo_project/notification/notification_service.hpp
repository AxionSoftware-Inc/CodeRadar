#ifndef NOTIFICATION_SERVICE_HPP
#define NOTIFICATION_SERVICE_HPP

#include "order_service.hpp"
#include "super_god_manager.hpp"

class NotificationService {
public:
    void send_alert(int alert_code) {
        log_event(alert_code);
        if (alert_code == 999) {
            retry_failed_order(alert_code);
        }
    }
};

#endif
