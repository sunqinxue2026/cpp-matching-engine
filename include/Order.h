#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace matching {
using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using SequenceId = std::uint64_t;
enum class Side { BUY, SELL };
enum class OrderStatus { NEW, PARTIALLY_FILLED, FILLED, CANCELED, REJECTED };

// Validation creates a NEW order. Global ID uniqueness belongs to OrderManager.
class Order {
public:
    Order(OrderId id, std::string symbol, Side side, Price price,
          Quantity quantity, SequenceId sequence);
    OrderId id() const noexcept { return id_; }
    const std::string& symbol() const noexcept { return symbol_; }
    Side side() const noexcept { return side_; }
    Price price() const noexcept { return price_; }
    Quantity quantity() const noexcept { return quantity_; }
    Quantity remaining_quantity() const noexcept { return remaining_; }
    SequenceId sequence() const noexcept { return sequence_; }
    OrderStatus status() const noexcept { return status_; }
private:
    friend class OrderManager;
    // Only the manager commits prevalidated positive fills.
    void apply_fill(Quantity quantity) noexcept {
        remaining_ -= quantity;
        status_ = remaining_ == 0 ? OrderStatus::FILLED : OrderStatus::PARTIALLY_FILLED;
    }
    OrderId id_;
    std::string symbol_;
    Side side_;
    Price price_;
    Quantity quantity_;
    Quantity remaining_;
    SequenceId sequence_;
    OrderStatus status_{OrderStatus::NEW};
};
std::string_view to_string(OrderStatus status) noexcept;
}
