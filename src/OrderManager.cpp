#include "OrderManager.h"
#include <limits>
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
        book_.insert(request.order_id);
        // No allocations or throwing operations after successful book insertion.
        last_sequence_ = sequence;
        recorded->second = {request.order_id, OrderStatus::NEW, SubmitError::NONE, sequence};
        return recorded->second;
    } catch (const std::invalid_argument&) {
        if (created) orders_.erase(request.order_id);
        recorded->second = reject(SubmitError::INVALID_ORDER);
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
