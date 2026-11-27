#ifndef ORDER_SERVICE_HPP
#define ORDER_SERVICE_HPP

#include "inventory_service.hpp"

class OrderService {
public:
    void create_order(int order_id) {
        check_stock(order_id);
    }
    
    void retry_failed_order(int order_id) {
        if (order_id > 0) {
            create_order(order_id);
        }
    }
};

#endif
