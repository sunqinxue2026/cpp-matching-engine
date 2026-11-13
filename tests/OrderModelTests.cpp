#include "Order.h"
#include <iostream>
#include <stdexcept>
using namespace matching;
void require(bool ok) { if (!ok) throw std::runtime_error("contract failed"); }
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected);
}
int main() {
    const Order buy(1,"TEST",Side::BUY,10000,40,1);
    const Order sell(2,"TEST",Side::SELL,9999,100,2);
    require(buy.id()==1 && buy.symbol()=="TEST" && buy.side()==Side::BUY);
    require(buy.price()==10000 && buy.quantity()==40 && buy.remaining_quantity()==40);
    require(buy.sequence()==1 && buy.status()==OrderStatus::NEW);
    require(sell.side()==Side::SELL && sell.remaining_quantity()==100);
    rejects([]{Order(1,"TEST",Side::BUY,0,1,1);});
    rejects([]{Order(1,"TEST",Side::BUY,-1,1,1);});
    rejects([]{Order(1,"TEST",Side::BUY,1,0,1);});
    rejects([]{Order(1,"TEST",Side::BUY,1,-1,1);});
    rejects([]{Order(0,"TEST",Side::BUY,1,1,1);});
    rejects([]{Order(1,"",Side::BUY,1,1,1);});
    rejects([]{Order(1,"T EST",Side::BUY,1,1,1);});
    rejects([]{Order(1,"TEST",Side::BUY,1,1,0);});
    rejects([]{Order(1,"TEST",static_cast<Side>(42),1,1,1);});
    require(to_string(OrderStatus::CANCELED)=="CANCELED");
    std::cout << "Order model contract checks passed\n";
}
