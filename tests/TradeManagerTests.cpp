#include "MatchingEngine.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
using namespace matching;
void require(bool value) { if (!value) throw std::runtime_error("stage5 trade contract failed"); }
void state(const MatchingEngine& e, OrderId id, Quantity remaining, OrderStatus status) {
    const auto* order = e.orders().find(id);
    require(order && order->remaining_quantity() == remaining && order->status() == status);
    require(e.orders().book().contains(id) == (remaining > 0));
    require(e.orders().book().check_invariants());
}
void conservation(const MatchingEngine& e) {
    std::unordered_map<OrderId,Quantity> executed;
    SequenceId previous = 0;
    for (const auto& t : e.trades()) {
        require(t.trade_id == t.trade_sequence && t.trade_sequence > previous);
        previous = t.trade_sequence;
        require(t.symbol == "TEST" && t.trade_price > 0 && t.trade_quantity > 0);
        const auto* buy=e.orders().find(t.buy_order_id);
        const auto* sell=e.orders().find(t.sell_order_id);
        require(buy && sell && buy->side()==Side::BUY && sell->side()==Side::SELL);
        require(buy->price() >= t.trade_price && sell->price() <= t.trade_price);
        executed[t.buy_order_id] += t.trade_quantity;
        executed[t.sell_order_id] += t.trade_quantity;
    }
    for (const auto& x : executed) {
        const auto* o=e.orders().find(x.first);
        require(o->quantity()==x.second+o->remaining_quantity());
    }
}
int main() {
    // Same passive order receives repeated fills without losing FIFO priority.
    MatchingEngine e("TEST");
    e.submitOrder({1,"TEST",Side::SELL,100,100});
    e.submitOrder({2,"TEST",Side::SELL,100,50});
    require(e.submitOrder({3,"TEST",Side::BUY,101,40}).status==OrderStatus::FILLED);
    state(e,1,60,OrderStatus::PARTIALLY_FILLED); state(e,3,0,OrderStatus::FILLED);
    require(e.orders().submission(1)->status==OrderStatus::NEW);
    e.submitOrder({4,"TEST",Side::BUY,100,70});
    state(e,1,0,OrderStatus::FILLED); state(e,2,40,OrderStatus::PARTIALLY_FILLED);
    require(e.trades().size()==3 && e.trades()[1].sell_order_id==1 && e.trades()[2].sell_order_id==2);
    require(e.trades()[0].trade_price==100 && e.trades()[0].trade_quantity==40);
    e.submitOrder({5,"TEST",Side::BUY,100,60});
    state(e,2,0,OrderStatus::FILLED); state(e,5,20,OrderStatus::PARTIALLY_FILLED);
    e.submitOrder({6,"TEST",Side::SELL,99,20});
    state(e,5,0,OrderStatus::FILLED); state(e,6,0,OrderStatus::FILLED);
    require(e.orders().book().active_count()==0 && e.orders().history_count()==6);
    const auto count=e.trades().size();
    require(e.submitOrder({1,"TEST",Side::SELL,100,1}).error==SubmitError::DUPLICATE_ID);
    require(e.submitOrder({7,"TEST",Side::BUY,0,1}).error==SubmitError::INVALID_ORDER);
    require(e.trades().size()==count); conservation(e);
    // Buy-side partial fills and multiple passive price levels.
    MatchingEngine b("TEST");
    b.submitOrder({1,"TEST",Side::BUY,102,10}); b.submitOrder({2,"TEST",Side::BUY,101,20});
    b.submitOrder({3,"TEST",Side::SELL,100,5}); state(b,1,5,OrderStatus::PARTIALLY_FILLED);
    b.submitOrder({4,"TEST",Side::SELL,100,30});
    state(b,1,0,OrderStatus::FILLED); state(b,2,0,OrderStatus::FILLED); state(b,4,5,OrderStatus::PARTIALLY_FILLED);
    require(b.trades().size()==3 && b.trades()[1].trade_price==102 && b.trades()[2].trade_price==101);
    conservation(b);
    // A batch must fit entirely: no partial publication when sequence runs out.
    const auto max=std::numeric_limits<SequenceId>::max();
    MatchingEngine limited("TEST",0,max-1);
    limited.submitOrder({1,"TEST",Side::SELL,100,1}); limited.submitOrder({2,"TEST",Side::SELL,100,1});
    require(limited.submitOrder({3,"TEST",Side::BUY,100,2}).error==SubmitError::TRADE_SEQUENCE_EXHAUSTED);
    require(limited.trades().empty() && !limited.orders().find(3) && limited.orders().last_sequence()==2);
    state(limited,1,1,OrderStatus::NEW); state(limited,2,1,OrderStatus::NEW);
    require(limited.submitOrder({4,"TEST",Side::BUY,100,1}).accepted());
    require(limited.trades().size()==1 && limited.trades()[0].trade_id==max);
    require(limited.trade_manager().last_sequence()==max);
    require(limited.submitOrder({5,"TEST",Side::BUY,100,1}).error==SubmitError::TRADE_SEQUENCE_EXHAUSTED);
    state(limited,2,1,OrderStatus::NEW);
    require(limited.submitOrder({6,"TEST",Side::BUY,99,1}).accepted());
    require(limited.trades().size()==1); conservation(limited);
    // Grow trade history repeatedly; compare values rather than retaining vector element pointers.
    MatchingEngine growth("TEST");
    for (OrderId i=1;i<=400;++i) {
        growth.submitOrder({i*2-1,"TEST",Side::SELL,100,1});
        growth.submitOrder({i*2,"TEST",Side::BUY,100,1});
    }
    require(growth.trade_manager().size()==400 && growth.trades().front().buy_order_id==2);
    require(growth.trades().back().trade_sequence==400 && growth.orders().book().active_count()==0);
    conservation(growth);
    std::cout << "Trade manager: partial/full fills, FIFO, conservation, terminal cleanup, batch exhaustion and growth passed\n";
}
