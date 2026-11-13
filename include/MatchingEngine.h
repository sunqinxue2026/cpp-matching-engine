#pragma once
#include "OrderManager.h"
namespace matching {
// Stage 3: submission facade only. Matching is introduced in stage 4.
class MatchingEngine {
public:
    explicit MatchingEngine(std::string symbol, SequenceId last_sequence = 0);
    SubmitResult submitOrder(const OrderRequest& request);
    const OrderManager& orders() const noexcept { return orders_; }
private:
    OrderManager orders_;
};
}
