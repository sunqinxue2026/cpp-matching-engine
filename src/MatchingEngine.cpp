#include "MatchingEngine.h"
#include <utility>
namespace matching {
MatchingEngine::MatchingEngine(std::string symbol, SequenceId last_sequence)
    : orders_(std::move(symbol), last_sequence) {}
SubmitResult MatchingEngine::submitOrder(const OrderRequest& request) {
    return orders_.submit_impl(request, &trades_);
}
}
