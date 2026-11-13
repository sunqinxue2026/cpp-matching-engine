#include "OrderBook.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace matching;
void require(bool value) { if (!value) throw std::runtime_error("order book contract failed"); }
template<class E, class F> void rejects(F f) {
    bool rejected=false; try { f(); } catch(const E&) { rejected=true; } require(rejected);
}
int main() {
    static_assert(!std::is_copy_constructible_v<OrderBook>);
    static_assert(!std::is_move_constructible_v<OrderBook>);
    static_assert(!std::is_constructible_v<OrderBook, std::string, OrderStore&&>);
    OrderStore store;
    OrderBook book("TEST", store);
    require(!book.best_price(Side::BUY) && !book.best_order(Side::SELL));
    require(book.levels(Side::BUY).empty() && !book.erase(99) && book.check_invariants());
    const auto add = [&](OrderId id, Side side, Price p, Quantity q, SequenceId seq) {
        store.emplace(id, Order(id,"TEST",side,p,q,seq)); book.insert(id); require(book.check_invariants());
    };
    add(1,Side::BUY,100,10,1); add(2,Side::BUY,102,20,2);
    add(3,Side::BUY,100,30,3); add(4,Side::BUY,100,40,4);
    add(5,Side::SELL,105,50,5); add(6,Side::SELL,104,60,6);
    require(book.best_price(Side::BUY)==102 && book.best_order(Side::BUY)==2);
    require(book.best_price(Side::SELL)==104 && book.best_order(Side::SELL)==6);
    require(book.orders_at(Side::BUY,100)==std::vector<OrderId>({1,3,4}));
    auto depth=book.levels(Side::BUY);
    require(depth.size()==2 && depth[1].quantity==80 && depth[1].order_count==3);
    rejects<std::invalid_argument>([&]{book.insert(1);});
    rejects<std::invalid_argument>([&]{book.insert(999);});
    store.emplace(7,Order(7,"OTHER",Side::BUY,100,1,7));
    rejects<std::invalid_argument>([&]{book.insert(7);});
    store.emplace(8,Order(8,"TEST",Side::BUY,100,1,5));
    rejects<std::invalid_argument>([&]{book.insert(8);});
    store.emplace(9,Order(10,"TEST",Side::BUY,100,1,9));
    rejects<std::invalid_argument>([&]{book.insert(9);});
    rejects<std::invalid_argument>([&]{book.best_order(static_cast<Side>(42));});
    require(book.active_count()==6 && book.check_invariants());
    require(book.erase(3) && book.orders_at(Side::BUY,100)==std::vector<OrderId>({1,4}));
    require(book.erase(1) && book.best_order(Side::BUY)==2);
    require(book.erase(4) && book.orders_at(Side::BUY,100).empty());
    require(book.erase(2) && !book.best_order(Side::BUY) && book.levels(Side::BUY).empty());
    require(!book.erase(2) && book.erase(6) && book.best_order(Side::SELL)==5);
    require(book.erase(5) && book.active_count()==0 && book.check_invariants());
    require(store.at(1).status()==OrderStatus::NEW); // structural erase is not cancellation
    rejects<std::invalid_argument>([&]{book.insert(1);});
    // Exercise map and both unordered_map growth while list positions stay live.
    for (OrderId id=100; id<2100; ++id) add(id, id%2 ? Side::BUY : Side::SELL,100+id%31,1,id);
    store.rehash(store.bucket_count()*2);
    require(book.check_invariants());
    for (OrderId id=100; id<2100; ++id) { require(book.erase(id)); require(book.check_invariants()); }
    require(book.active_count()==0 && book.levels(Side::SELL).empty());
    add(3000,Side::BUY,200,std::numeric_limits<Quantity>::max(),3000);
    store.emplace(3001,Order(3001,"TEST",Side::BUY,200,1,3001));
    rejects<std::overflow_error>([&]{book.insert(3001);});
    require(book.active_count()==1 && !book.contains(3001) && book.check_invariants());
    require(book.levels(Side::BUY)[0].quantity==std::numeric_limits<Quantity>::max());
    require(book.erase(3000)); book.insert(3001); require(book.check_invariants());
    rejects<std::invalid_argument>([&]{OrderBook invalid("",store);});
    std::cout << "Order book: ordering, FIFO, erase, rehash, rollback, overflow and invariant checks passed\n";
}
