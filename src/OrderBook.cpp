#include "OrderBook.h"
#include <limits>
#include <stdexcept>
#include <utility>
namespace matching {
namespace {
void validate_side(Side side) {
    if (side != Side::BUY && side != Side::SELL) throw std::invalid_argument("invalid side");
}
Quantity add_quantity(Quantity total, Quantity next) {
    if (next <= 0 || total > std::numeric_limits<Quantity>::max() - next)
        throw std::overflow_error("price level quantity overflow or invalid remaining quantity");
    return total + next;
}
}
OrderBook::OrderBook(std::string symbol, const OrderStore& orders)
    : symbol_(std::move(symbol)), orders_(orders) {
    if (symbol_.empty() || symbol_.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("invalid symbol");
}
template<class Map>
void OrderBook::insert_into(Map& book, const Order& order) {
    // All potentially throwing quantity checks occur before mutation.
    Quantity total = order.remaining_quantity();
    const auto existing = book.find(order.price());
    if (existing != book.end())
        for (auto id : existing->second) total = add_quantity(total, orders_.at(id).remaining_quantity());
    const auto result = book.try_emplace(order.price());
    auto level = result.first;
    try {
        level->second.push_back(order.id());
    } catch (...) {
        if (result.second) book.erase(level);
        throw;
    }
    auto node = std::prev(level->second.end());
    try {
        const auto indexed = locations_.emplace(order.id(), Location{order.side(), order.price(), node});
        if (!indexed.second) throw std::logic_error("duplicate active index");
    } catch (...) {
        level->second.erase(node);
        if (level->second.empty()) book.erase(level);
        throw;
    }
    last_sequence_ = order.sequence();
}
void OrderBook::insert(OrderId id) {
    const auto found = orders_.find(id);
    if (found == orders_.end()) throw std::invalid_argument("unknown order id");
    const auto& order = found->second;
    if (id != order.id() || order.symbol() != symbol_ || contains(id) ||
        order.remaining_quantity() <= 0 || order.sequence() <= last_sequence_ ||
        (order.status() != OrderStatus::NEW && order.status() != OrderStatus::PARTIALLY_FILLED))
        throw std::invalid_argument("order cannot enter book");
    validate_side(order.side());
    if (order.side() == Side::BUY) insert_into(bids_, order);
    else insert_into(asks_, order);
}
bool OrderBook::erase(OrderId id) {
    const auto found = locations_.find(id);
    if (found == locations_.end()) return false;
    const auto location = found->second;
    if (location.side == Side::BUY) {
        auto level = bids_.find(location.price);
        locations_.erase(found); // Never leave an index pointing to a destroyed node.
        level->second.erase(location.node);
        if (level->second.empty()) bids_.erase(level);
    } else {
        auto level = asks_.find(location.price);
        locations_.erase(found);
        level->second.erase(location.node);
        if (level->second.empty()) asks_.erase(level);
    }
    return true;
}
bool OrderBook::contains(OrderId id) const noexcept { return locations_.find(id) != locations_.end(); }
std::optional<OrderId> OrderBook::best_order(Side side) const {
    validate_side(side);
    if (side == Side::BUY) return bids_.empty() ? std::nullopt : std::optional<OrderId>(bids_.begin()->second.front());
    return asks_.empty() ? std::nullopt : std::optional<OrderId>(asks_.begin()->second.front());
}
std::optional<Price> OrderBook::best_price(Side side) const {
    const auto id = best_order(side);
    return id ? std::optional<Price>(orders_.at(*id).price()) : std::nullopt;
}
std::vector<OrderId> OrderBook::orders_at(Side side, Price price) const {
    validate_side(side);
    const Queue* queue = nullptr;
    if (side == Side::BUY) { auto it = bids_.find(price); if (it != bids_.end()) queue = &it->second; }
    else { auto it = asks_.find(price); if (it != asks_.end()) queue = &it->second; }
    return queue ? std::vector<OrderId>(queue->begin(), queue->end()) : std::vector<OrderId>{};
}
std::vector<BookLevel> OrderBook::levels(Side side) const {
    validate_side(side);
    std::vector<BookLevel> result;
    const auto append = [&](const auto& book) {
        for (const auto& level : book) {
            Quantity total = 0;
            for (auto id : level.second) total = add_quantity(total, orders_.at(id).remaining_quantity());
            result.push_back({level.first, total, level.second.size()});
        }
    };
    if (side == Side::BUY) append(bids_); else append(asks_);
    return result;
}
template<class Map>
bool OrderBook::check_side(const Map& book, Side side) const {
    for (const auto& level : book) {
        if (level.second.empty()) return false;
        SequenceId previous = 0;
        for (auto node = level.second.begin(); node != level.second.end(); ++node) {
            auto indexed = locations_.find(*node);
            auto order = orders_.find(*node);
            if (indexed == locations_.end() || order == orders_.end()) return false;
            const auto& o = order->second;
            if (o.id() != *node || o.side() != side || o.price() != level.first || o.symbol() != symbol_ ||
                o.remaining_quantity() <= 0 || o.sequence() <= previous ||
                (o.status() != OrderStatus::NEW && o.status() != OrderStatus::PARTIALLY_FILLED) ||
                indexed->second.side != side || indexed->second.price != level.first || indexed->second.node != node)
                return false;
            previous = o.sequence();
        }
    }
    return true;
}
bool OrderBook::check_invariants() const {
    std::size_t count = 0;
    for (const auto& level : bids_) count += level.second.size();
    for (const auto& level : asks_) count += level.second.size();
    return count == locations_.size() && check_side(bids_, Side::BUY) && check_side(asks_, Side::SELL);
}
}
