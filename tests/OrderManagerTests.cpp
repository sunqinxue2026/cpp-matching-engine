#include "MatchingEngine.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace matching;
void require(bool value) { if (!value) throw std::runtime_error("submission contract failed"); }
int main() {
    static_assert(!std::is_move_constructible_v<OrderManager>);
    static_assert(!std::is_copy_constructible_v<MatchingEngine>);
    OrderManager manager("TEST");
    const auto send = [&](OrderId id, Side side, Price price, Quantity quantity) {
        return manager.submit({id,"TEST",side,price,quantity});
    };
    require(send(1,Side::BUY,100,10).accepted());
    require(send(2,Side::SELL,99,20).sequence==2);
    require(send(3,Side::BUY,100,30).sequence==3);
    const auto* original=manager.find(1);
    require(original && original->status()==OrderStatus::NEW && original->remaining_quantity()==10);
    require(manager.book().orders_at(Side::BUY,100)==std::vector<OrderId>({1,3}));
    require(manager.book().active_count()==3); // Crossing does not match in stage 3.
    require(send(1,Side::SELL,200,100).error==SubmitError::DUPLICATE_ID);
    require(manager.find(1)==original && original->price()==100);
    require(manager.submission(1)->accepted()); // Duplicate does not overwrite original result.
    require(send(0,Side::BUY,1,1).error==SubmitError::INVALID_ID);
    require(!manager.submission(0));
    require(manager.submit({4,"OTHER",Side::BUY,1,1}).error==SubmitError::SYMBOL_MISMATCH);
    require(send(4,Side::BUY,1,1).error==SubmitError::DUPLICATE_ID);
    OrderId next=5;
    for (auto price : {Price(0),Price(-1)}) require(send(next++,Side::BUY,price,1).error==SubmitError::INVALID_ORDER);
    for (auto qty : {Quantity(0),Quantity(-1)}) require(send(next++,Side::SELL,1,qty).error==SubmitError::INVALID_ORDER);
    require(send(next++,static_cast<Side>(42),1,1).error==SubmitError::INVALID_ORDER);
    require(!manager.find(5) && manager.submission(5)->status==OrderStatus::REJECTED);
    require(send(5,Side::BUY,1,1).error==SubmitError::DUPLICATE_ID);
    require(manager.last_sequence()==3 && manager.history_count()==3);
    require(send(20,Side::BUY,200,std::numeric_limits<Quantity>::max()).sequence==4);
    require(send(21,Side::BUY,200,1).error==SubmitError::LEVEL_OVERFLOW);
    require(!manager.find(21) && manager.last_sequence()==4);
    require(send(22,Side::SELL,201,1).sequence==5);
    for(OrderId id=100;id<2100;++id) require(send(id,Side::BUY,300+id%31,1).accepted());
    require(original==manager.find(1) && original->remaining_quantity()==10);
    require(manager.book().check_invariants());
    require(manager.find(99999)==nullptr && !manager.submission(99999));
    MatchingEngine edge("TEST",std::numeric_limits<SequenceId>::max()-1);
    require(edge.submitOrder({1,"TEST",Side::BUY,1,1}).sequence==std::numeric_limits<SequenceId>::max());
    require(edge.submitOrder({2,"TEST",Side::SELL,1,1}).error==SubmitError::SEQUENCE_EXHAUSTED);
    require(edge.orders().history_count()==1 && edge.orders().book().check_invariants());
    require(edge.submitOrder({2,"TEST",Side::BUY,1,1}).error==SubmitError::DUPLICATE_ID);
    bool rejected=false; try { MatchingEngine bad(""); } catch(const std::invalid_argument&) {rejected=true;}
    require(rejected);
    std::cout << "Submission: buy/sell, FIFO, rejection, reserved IDs, overflow, rehash, sequence exhaustion passed\n";
}
