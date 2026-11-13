#include "Order.h"
#include <iostream>
int main() {
    const matching::Order order(1, "TEST", matching::Side::BUY, 10000, 40, 1);
    std::cout << "stage=1 symbol=" << order.symbol() << " price_ticks=" << order.price()
              << " quantity=" << order.quantity() << " remaining=" << order.remaining_quantity()
              << " status=" << matching::to_string(order.status()) << '\n';
    std::cout << "Matching is not implemented in stage 1.\n";
}
