#pragma once
#include "OrderBook.h"
#include "TradeManager.h"
namespace matching {
struct OrderRequest {
    OrderId order_id;
    std::string symbol;
    Side side;
    Price price;
    Quantity quantity;
};
enum class SubmitError { NONE, INVALID_ID, DUPLICATE_ID, INVALID_ORDER,
                         SYMBOL_MISMATCH, LEVEL_OVERFLOW, TRADE_SEQUENCE_EXHAUSTED, SEQUENCE_EXHAUSTED };
struct SubmitResult {
    OrderId order_id;
    OrderStatus status;
    SubmitError error;
    SequenceId sequence;
    bool accepted() const noexcept { return error == SubmitError::NONE; }
};
std::string_view to_string(SubmitError error) noexcept;
enum class CancelError { NONE, NOT_FOUND, REJECTED_ORDER, ALREADY_FILLED, ALREADY_CANCELED };
struct CancelResult {
    OrderId order_id;
    std::optional<OrderStatus> status;
    CancelError error;
    bool canceled() const noexcept { return error == CancelError::NONE; }
};
std::string_view to_string(CancelError error) noexcept;
// Owns all orders before the book is constructed; book is destroyed first.
// Rejected nonzero IDs are reserved, but are not malformed Order objects.
class OrderManager {
public:
    explicit OrderManager(std::string symbol, SequenceId last_sequence = 0);
    OrderManager(const OrderManager&) = delete;
    OrderManager& operator=(const OrderManager&) = delete;
    OrderManager(OrderManager&&) = delete;
    OrderManager& operator=(OrderManager&&) = delete;
    SubmitResult submit(const OrderRequest& request);
    CancelResult cancelOrder(OrderId id);
    const Order* find(OrderId id) const noexcept;
    std::optional<SubmitResult> submission(OrderId id) const;
    const OrderBook& book() const noexcept { return book_; }
    std::size_t history_count() const noexcept { return orders_.size(); }
    SequenceId last_sequence() const noexcept { return last_sequence_; }
private:
    friend class MatchingEngine;
    SubmitResult submit_impl(const OrderRequest& request, TradeManager* trades);
    std::string symbol_;
    OrderStore orders_;
    OrderBook book_;
    std::unordered_map<OrderId, SubmitResult> submissions_;
    SequenceId last_sequence_;
};
}
