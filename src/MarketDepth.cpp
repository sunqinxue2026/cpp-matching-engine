#include "MarketDepth.h"
namespace matching {
DepthSnapshot MarketDepth::snapshot(const OrderBook& book) {
    return {book.best_price(Side::BUY), book.best_price(Side::SELL),
            book.levels(Side::BUY), book.levels(Side::SELL), book.active_count()};
}
}
