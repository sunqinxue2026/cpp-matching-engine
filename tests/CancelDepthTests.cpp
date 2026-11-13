#include "MatchingEngine.h"
#include <iostream>
#include <stdexcept>
using namespace matching;
void require(bool ok) { if (!ok) throw std::runtime_error("cancel/depth contract failed"); }
int main() {
    MatchingEngine e("TEST");
    auto empty=e.marketDepth();
    require(!empty.best_bid && !empty.best_ask && empty.bids.empty() && empty.asks.empty() && empty.active_order_count==0);
    require(e.cancelOrder(0).error==CancelError::NOT_FOUND);
    require(!e.cancelOrder(99).status);
    e.submitOrder({90,"TEST",Side::BUY,0,10});
    require(e.cancelOrder(90).error==CancelError::REJECTED_ORDER);
    require(e.cancelOrder(90).status==OrderStatus::REJECTED);
    // Cancel middle FIFO node; preserve other nodes and all historical fields.
    e.submitOrder({1,"TEST",Side::BUY,100,10});
    e.submitOrder({2,"TEST",Side::BUY,100,20});
    e.submitOrder({3,"TEST",Side::BUY,100,30});
    e.submitOrder({4,"TEST",Side::BUY,99,5});
    e.submitOrder({5,"TEST",Side::SELL,105,40});
    e.submitOrder({6,"TEST",Side::SELL,104,50});
    const auto before=e.marketDepth();
    require(before.best_bid==100 && before.best_ask==104 && before.active_order_count==6);
    require(before.bids.size()==2 && before.bids[0].quantity==60 && before.bids[0].order_count==3);
    require(before.asks.size()==2 && before.asks[0].price==104 && before.asks[1].price==105);
    const auto seq=e.orders().last_sequence();
    require(e.cancelOrder(2).canceled());
    require(e.orders().find(2)->remaining_quantity()==20 && e.orders().find(2)->quantity()==20);
    require(e.orders().find(2)->status()==OrderStatus::CANCELED);
    require(e.orders().submission(2)->status==OrderStatus::NEW);
    require(e.cancelOrder(2).error==CancelError::ALREADY_CANCELED);
    require(e.orders().last_sequence()==seq && e.trades().empty());
    require(e.orders().book().orders_at(Side::BUY,100)==std::vector<OrderId>({1,3}));
    require(before.bids[0].quantity==60 && before.active_order_count==6);
    require(e.marketDepth().bids[0].quantity==40);
    // Match remaining FIFO and verify canceled ID never trades.
    e.submitOrder({7,"TEST",Side::SELL,100,25});
    require(e.trades().size()==2 && e.trades()[0].buy_order_id==1 && e.trades()[1].buy_order_id==3);
    require(e.cancelOrder(1).error==CancelError::ALREADY_FILLED);
    require(e.orders().find(3)->status()==OrderStatus::PARTIALLY_FILLED);
    require(e.orders().find(3)->remaining_quantity()==15);
    require(e.cancelOrder(3).canceled());
    require(e.orders().find(3)->quantity()==30 && e.orders().find(3)->remaining_quantity()==15);
    require(e.marketDepth().best_bid==99);
    require(e.cancelOrder(4).canceled() && !e.marketDepth().best_bid);
    require(e.cancelOrder(6).canceled() && e.marketDepth().best_ask==105);
    require(e.cancelOrder(5).canceled());
    require(e.marketDepth().active_order_count==0 && e.orders().book().check_invariants());
    const auto count=e.trades().size();
    require(e.submitOrder({2,"TEST",Side::BUY,100,1}).error==SubmitError::DUPLICATE_ID);
    e.submitOrder({8,"TEST",Side::BUY,200,100});
    require(e.trades().size()==count); // Canceled sellers cannot re-enter matching.
    require(e.cancelOrder(8).canceled());
    // Partially filled sell cancellation and new orders cannot hit the remainder.
    MatchingEngine s("TEST");
    s.submitOrder({1,"TEST",Side::SELL,100,100});
    s.submitOrder({2,"TEST",Side::BUY,101,40});
    require(s.marketDepth().asks[0].quantity==60);
    require(s.cancelOrder(1).canceled());
    require(s.orders().find(1)->remaining_quantity()==60 && s.marketDepth().asks.empty());
    s.submitOrder({3,"TEST",Side::BUY,101,60});
    require(s.trades().size()==1 && s.marketDepth().bids[0].quantity==60);
    require(s.orders().book().check_invariants());
    // Rehash and cancel many distinct nodes, checking book/index agreement.
    MatchingEngine many("TEST");
    for(OrderId id=1;id<=1000;++id) require(many.submitOrder({id,"TEST",Side::BUY,100+static_cast<Price>(id%17),1}).accepted());
    for(OrderId id=1;id<=1000;++id) {
        require(many.cancelOrder(id).canceled());
        require(!many.orders().book().contains(id) && many.orders().book().check_invariants());
    }
    require(many.marketDepth().bids.empty() && many.orders().history_count()==1000);
    std::cout << "Cancellation, terminal safety, FIFO, depth snapshots and 1000-node cleanup passed\n";
}
