#pragma once
#include "OrderBook.h"
namespace matching {
// Value snapshot: independent of later engine operations; no borrowed iterators.
struct DepthSnapshot {
    std::optional<Price> best_bid;
    std::optional<Price> best_ask;
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
    std::size_t active_order_count;
};
class MarketDepth {
public:
    static DepthSnapshot snapshot(const OrderBook& book);
};
}
