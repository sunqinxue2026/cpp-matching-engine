#include "Order.h"
#include <stdexcept>
#include <utility>
namespace matching {
Order::Order(OrderId id, std::string symbol, Side side, Price price,
             Quantity quantity, SequenceId sequence)
    : id_(id), symbol_(std::move(symbol)), side_(side), price_(price),
      quantity_(quantity), remaining_(quantity), sequence_(sequence) {
    if (id_ == 0 || sequence_ == 0) throw std::invalid_argument("id and sequence must be positive");
    if (symbol_.empty() || symbol_.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("symbol must be nonempty without whitespace");
    if (side_ != Side::BUY && side_ != Side::SELL) throw std::invalid_argument("invalid side");
    if (price_ <= 0) throw std::invalid_argument("price ticks must be positive");
    if (quantity_ <= 0) throw std::invalid_argument("quantity must be positive");
}
std::string_view to_string(OrderStatus status) noexcept {
    switch (status) {
    case OrderStatus::NEW: return "NEW";
    case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
    case OrderStatus::FILLED: return "FILLED";
    case OrderStatus::CANCELED: return "CANCELED";
    case OrderStatus::REJECTED: return "REJECTED";
    }
    return "UNKNOWN";
}
}
