#ifndef INVENTORY_SERVICE_HPP
#define INVENTORY_SERVICE_HPP

#include "notification_service.hpp"
#include "super_god_manager.hpp"

class InventoryService {
public:
    void check_stock(int item_id) {
        log_event(item_id);
        if (item_id < 0) {
            send_alert(item_id);
        }
    }
};

#endif
