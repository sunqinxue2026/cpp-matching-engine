#include "MatchingEngine.h"
#include <iostream>
#include <stdexcept>
using namespace matching;
void require(bool x) { if (!x) throw std::runtime_error("matching contract failed"); }
int main() {
    MatchingEngine e("TEST");
    auto send = [&](OrderId id, Side side, Price p, Quantity q) {
        auto result=e.submitOrder({id,"TEST",side,p,q});
        require(e.orders().book().check_invariants()); return result;
    };
    send(1,Side::SELL,102,30); send(2,Side::SELL,100,20); send(3,Side::SELL,100,40);
    require(e.trades().empty());
    require(send(4,Side::BUY,101,35).status==OrderStatus::FILLED);
    require(e.trades().size()==2);
    require(e.trades()[0].sell_order_id==2 && e.trades()[0].trade_price==100 && e.trades()[0].trade_quantity==20);
    require(e.trades()[1].sell_order_id==3 && e.trades()[1].trade_quantity==15);
    require(e.orders().find(3)->remaining_quantity()==25 && e.orders().find(3)->status()==OrderStatus::PARTIALLY_FILLED);
    require(!e.orders().book().contains(2) && !e.orders().book().contains(4));
    require(e.orders().book().orders_at(Side::SELL,100)==std::vector<OrderId>{3});
    require(send(5,Side::BUY,103,70).status==OrderStatus::PARTIALLY_FILLED);
    require(e.trades().size()==4 && e.trades()[2].sell_order_id==3 && e.trades()[3].sell_order_id==1);
    require(e.trades()[3].trade_price==102 && e.orders().find(5)->remaining_quantity()==15);
    require(!e.orders().book().best_price(Side::SELL));
    require(send(6,Side::SELL,99,15).status==OrderStatus::FILLED);
    require(e.trades().back().trade_price==103 && e.trades().back().buy_order_id==5);
    require(e.orders().book().active_count()==0 && e.orders().history_count()==6);
    require(send(5,Side::BUY,103,1).error==SubmitError::DUPLICATE_ID && e.trades().size()==5);
    for(std::size_t i=0;i<e.trades().size();++i) require(e.trades()[i].trade_id==i+1 && e.trades()[i].trade_sequence==i+1);
    MatchingEngine b("TEST");
    b.submitOrder({1,"TEST",Side::BUY,99,10}); b.submitOrder({2,"TEST",Side::BUY,101,10});
    b.submitOrder({3,"TEST",Side::BUY,101,10});
    b.submitOrder({4,"TEST",Side::SELL,99,25});
    require(b.trades().size()==3 && b.trades()[0].buy_order_id==2 && b.trades()[1].buy_order_id==3 && b.trades()[2].buy_order_id==1);
    require(b.trades()[0].trade_price==101 && b.trades()[2].trade_price==99);
    require(b.orders().find(1)->remaining_quantity()==5 && b.orders().book().check_invariants());
    b.submitOrder({5,"TEST",Side::SELL,100,5});
    require(b.trades().size()==3 && b.orders().book().best_price(Side::SELL)==100);
    require(b.submitOrder({6,"TEST",Side::BUY,0,5}).error==SubmitError::INVALID_ORDER);
    require(b.orders().book().check_invariants() && b.trades().size()==3);
    std::cout << "Matching: passive price, both sides, price/FIFO, partial/residual, multi-level, terminal cleanup passed\n";
}
