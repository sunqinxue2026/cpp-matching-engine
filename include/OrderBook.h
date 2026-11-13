#pragma once
#include "Order.h"
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace matching {
using OrderStore = std::unordered_map<OrderId, Order>;
struct BookLevel {
    Price price;
    Quantity quantity;
    std::size_t order_count;
};
// The store must outlive the book. Active orders must not be erased or replaced.
// Single-threaded structural API: erase() does not cancel or change Order status.
class OrderBook {
public:
    OrderBook(std::string symbol, const OrderStore& orders);
    OrderBook(std::string, OrderStore&&) = delete;
    OrderBook(std::string, const OrderStore&&) = delete;
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;
    void insert(OrderId id);
    bool erase(OrderId id);
    bool contains(OrderId id) const noexcept;
    std::size_t active_count() const noexcept { return locations_.size(); }
    std::optional<OrderId> best_order(Side side) const;
    std::optional<Price> best_price(Side side) const;
    std::vector<OrderId> orders_at(Side side, Price price) const;
    std::vector<BookLevel> levels(Side side) const;
    bool check_invariants() const;
private:
    using Queue = std::list<OrderId>;
    using Bids = std::map<Price, Queue, std::greater<Price>>;
    using Asks = std::map<Price, Queue>;
    struct Location { Side side; Price price; Queue::iterator node; };
    template<class Map> void insert_into(Map& book, const Order& order);
    template<class Map> bool check_side(const Map& book, Side side) const;
    std::string symbol_;
    const OrderStore& orders_;
    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Location> locations_;
    SequenceId last_sequence_{0};
};
}
