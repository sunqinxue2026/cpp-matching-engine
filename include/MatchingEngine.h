#pragma once
#include "OrderManager.h"
#include "MarketDepth.h"
namespace matching {
// Single-threaded price/time matching facade. Manager owns order state.
class MatchingEngine {
public:
    explicit MatchingEngine(std::string symbol, SequenceId last_sequence = 0,
                            SequenceId last_trade_sequence = 0);
    SubmitResult submitOrder(const OrderRequest& request);
    CancelResult cancelOrder(OrderId id);
    DepthSnapshot marketDepth() const;
    const OrderManager& orders() const noexcept { return orders_; }
    const std::vector<Trade>& trades() const noexcept { return trades_.records(); }
    const TradeManager& trade_manager() const noexcept { return trades_; }
private:
    TradeManager trades_;
    OrderManager orders_;
};
}
