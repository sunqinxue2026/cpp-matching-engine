#include "MatchingEngine.h"
#include <iostream>
int main() {
    using namespace matching;
    MatchingEngine engine("TEST");
    engine.submitOrder({1,"TEST",Side::SELL,100,100});
    const auto result=engine.submitOrder({2,"TEST",Side::BUY,101,40});
    for (const auto& t:engine.trades())
        std::cout << "trade=" << t.trade_id << " price_ticks=" << t.trade_price << " quantity=" << t.trade_quantity << '\n';
    std::cout << "buy=" << to_string(result.status)
              << " sell=" << to_string(engine.orders().find(1)->status())
              << " sell_remaining=" << engine.orders().find(1)->remaining_quantity()
              << " invariants=" << engine.orders().book().check_invariants() << '\n';
    const auto before = engine.marketDepth();
    std::cout << "before_cancel ask=" << *before.best_ask << " quantity=" << before.asks.front().quantity << '\n';
    const auto canceled = engine.cancelOrder(1);
    const auto after = engine.marketDepth();
    std::cout << "cancel=" << to_string(canceled.error)
              << " status=" << to_string(*canceled.status)
              << " retained_remaining=" << engine.orders().find(1)->remaining_quantity()
              << " active=" << after.active_order_count << '\n';
}
