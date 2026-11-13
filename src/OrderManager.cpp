#include "OrderManager.h"
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace matching {
std::string_view to_string(SubmitError error) noexcept {
    switch (error) {
    case SubmitError::NONE: return "NONE";
    case SubmitError::INVALID_ID: return "INVALID_ID";
    case SubmitError::DUPLICATE_ID: return "DUPLICATE_ID";
    case SubmitError::INVALID_ORDER: return "INVALID_ORDER";
    case SubmitError::SYMBOL_MISMATCH: return "SYMBOL_MISMATCH";
    case SubmitError::LEVEL_OVERFLOW: return "LEVEL_OVERFLOW";
    case SubmitError::TRADE_SEQUENCE_EXHAUSTED: return "TRADE_SEQUENCE_EXHAUSTED";
    case SubmitError::SEQUENCE_EXHAUSTED: return "SEQUENCE_EXHAUSTED";
    }
    return "UNKNOWN";
}
OrderManager::OrderManager(std::string symbol, SequenceId last_sequence)
    : symbol_(std::move(symbol)), book_(symbol_, orders_), last_sequence_(last_sequence) {}
const Order* OrderManager::find(OrderId id) const noexcept {
    const auto it = orders_.find(id);
    return it == orders_.end() ? nullptr : &it->second;
}
std::optional<SubmitResult> OrderManager::submission(OrderId id) const {
    const auto it = submissions_.find(id);
    return it == submissions_.end() ? std::nullopt : std::optional<SubmitResult>(it->second);
}
SubmitResult OrderManager::submit(const OrderRequest& request) {
    return submit_impl(request, nullptr);
}
SubmitResult OrderManager::submit_impl(const OrderRequest& request, TradeManager* trades) {
    const auto reject = [&](SubmitError error) {
        return SubmitResult{request.order_id, OrderStatus::REJECTED, error, 0};
    };
    if (request.order_id == 0) return reject(SubmitError::INVALID_ID);
    if (submissions_.find(request.order_id) != submissions_.end())
        return reject(SubmitError::DUPLICATE_ID);
    // Allocate the ID ledger entry before any book mutation. Do not retain its
    // iterator beyond this call; no ledger rehash occurs during the transaction.
    auto recorded = submissions_.emplace(request.order_id, reject(SubmitError::INVALID_ORDER)).first;
    bool created = false;
    try {
        if (request.symbol != symbol_) {
            recorded->second = reject(SubmitError::SYMBOL_MISMATCH);
            return recorded->second;
        }
        // Validate using a positive placeholder before checking sequence capacity.
        Order validated(request.order_id, request.symbol, request.side,
                        request.price, request.quantity, 1);
        if (last_sequence_ == std::numeric_limits<SequenceId>::max()) {
            recorded->second = reject(SubmitError::SEQUENCE_EXHAUSTED);
            return recorded->second;
        }
        const auto sequence = last_sequence_ + 1;
        auto inserted = orders_.try_emplace(request.order_id, request.order_id,
            request.symbol, request.side, request.price, request.quantity, sequence);
        if (!inserted.second) throw std::logic_error("order history and ID ledger disagree");
        created = true;
        auto& incoming = inserted.first->second;
        struct Fill { OrderId passive; Quantity quantity; };
        std::vector<Fill> fills;
        std::vector<Trade> pending;
        Quantity remainder = request.quantity;
        if (trades) {
            const Side opposite = request.side == Side::BUY ? Side::SELL : Side::BUY;
            // Snapshot planning never mutates the live book or passive orders.
            for (const auto& level : book_.levels(opposite)) {
                const bool crosses = request.side == Side::BUY ? request.price >= level.price : request.price <= level.price;
                if (!crosses || remainder == 0) break;
                for (const auto id : book_.orders_at(opposite, level.price)) {
                    const auto& passive = orders_.at(id);
                    const auto quantity = std::min(remainder, passive.remaining_quantity());
                    fills.push_back({id, quantity});
                    pending.push_back({0, symbol_, request.side == Side::BUY ? request.order_id : id,
                        request.side == Side::SELL ? request.order_id : id, passive.price(), quantity, 0});
                    remainder -= quantity;
                    if (remainder == 0) break;
                }
            }
            trades->prepare(pending);
        }
        // The new entity is private; prepare its residual before structural insertion.
        if (remainder != request.quantity) incoming.apply_fill(request.quantity - remainder);
        if (remainder > 0) book_.insert(request.order_id);
        // Commit: all allocation is finished. IDs and fills were validated above.
        for (const auto& fill : fills) {
            auto& passive = orders_.at(fill.passive);
            passive.apply_fill(fill.quantity);
            if (passive.remaining_quantity() == 0) book_.erase(fill.passive);
        }
        if (trades) trades->commit(pending);
        last_sequence_ = sequence;
        recorded->second = {request.order_id, incoming.status(), SubmitError::NONE, sequence};
        return recorded->second;
    } catch (const std::invalid_argument&) {
        if (created) orders_.erase(request.order_id);
        recorded->second = reject(SubmitError::INVALID_ORDER);
        return recorded->second;
    } catch (const TradeSequenceExhausted&) {
        if (created) orders_.erase(request.order_id);
        recorded->second = reject(SubmitError::TRADE_SEQUENCE_EXHAUSTED);
        return recorded->second;
    } catch (const std::overflow_error&) {
        if (created) orders_.erase(request.order_id);
        recorded->second = reject(SubmitError::LEVEL_OVERFLOW);
        return recorded->second;
    } catch (...) {
        if (created) orders_.erase(request.order_id);
        submissions_.erase(request.order_id);
        throw;
    }
}
}
