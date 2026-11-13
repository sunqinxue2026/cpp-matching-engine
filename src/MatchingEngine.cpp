#include "MatchingEngine.h"
#include <utility>
namespace matching {
MatchingEngine::MatchingEngine(std::string symbol, SequenceId last_sequence, SequenceId last_trade_sequence)
    : trades_(last_trade_sequence), orders_(std::move(symbol), last_sequence) {}
CancelResult MatchingEngine::cancelOrder(OrderId id) { return orders_.cancelOrder(id); }
DepthSnapshot MatchingEngine::marketDepth() const { return MarketDepth::snapshot(orders_.book()); }
SubmitResult MatchingEngine::submitOrder(const OrderRequest& request) {
    return orders_.submit_impl(request, &trades_);
}
}
