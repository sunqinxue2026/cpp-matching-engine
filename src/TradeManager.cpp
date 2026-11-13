#include "TradeManager.h"
#include <limits>
#include <type_traits>
#include <utility>
namespace matching {
void TradeManager::prepare(std::vector<Trade>& pending) {
    if (pending.size() > std::numeric_limits<SequenceId>::max() - last_sequence_)
        throw TradeSequenceExhausted();
    if (pending.size() > records_.max_size() - records_.size())
        throw std::length_error("trade storage exhausted");
    auto next = last_sequence_;
    for (auto& trade : pending) {
        trade.trade_id = ++next;
        trade.trade_sequence = next;
    }
    const auto required = records_.size() + pending.size();
    if (required > records_.capacity()) {
        const auto capacity = records_.capacity();
        const auto grown = capacity > records_.max_size() / 2 ? records_.max_size() : capacity * 2;
        records_.reserve(required > grown ? required : grown);
    }
}
void TradeManager::commit(std::vector<Trade>& pending) noexcept {
    static_assert(std::is_nothrow_move_constructible_v<Trade>);
    // prepare reserved enough space; moving Trade cannot throw.
    for (auto& trade : pending) records_.push_back(std::move(trade));
    if (!pending.empty()) last_sequence_ = records_.back().trade_sequence;
}
}
