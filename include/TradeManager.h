#pragma once
#include "Trade.h"
#include <stdexcept>
#include <vector>
namespace matching {
class TradeSequenceExhausted : public std::overflow_error {
public:
    TradeSequenceExhausted() : std::overflow_error("trade sequence exhausted") {}
};
// Owns append-only history. Mutation is private to the order transaction.
class TradeManager {
public:
    explicit TradeManager(SequenceId last_sequence = 0) noexcept : last_sequence_(last_sequence) {}
    const std::vector<Trade>& records() const noexcept { return records_; }
    std::size_t size() const noexcept { return records_.size(); }
    SequenceId last_sequence() const noexcept { return last_sequence_; }
private:
    friend class OrderManager;
    void prepare(std::vector<Trade>& pending);
    void commit(std::vector<Trade>& pending) noexcept;
    std::vector<Trade> records_;
    SequenceId last_sequence_;
};
}
