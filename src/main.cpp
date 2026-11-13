#include "MatchingEngine.h"
#include <iostream>
int main() {
    matching::MatchingEngine engine("TEST");
    const matching::OrderRequest requests[] = {
        {1,"TEST",matching::Side::BUY,10000,40},
        {2,"TEST",matching::Side::SELL,9999,100},
        {1,"TEST",matching::Side::BUY,10001,1},
        {3,"TEST",matching::Side::BUY,0,1}
    };
    for (const auto& request : requests) {
        const auto result = engine.submitOrder(request);
        std::cout << "id=" << result.order_id << " status=" << matching::to_string(result.status)
                  << " reason=" << matching::to_string(result.error)
                  << " sequence=" << result.sequence << '\n';
    }
    std::cout << "active=" << engine.orders().book().active_count()
              << " history=" << engine.orders().history_count()
              << " invariants=" << engine.orders().book().check_invariants() << '\n';
    std::cout << "Stage 3 accepts crossing orders without matching; matching starts in stage 4.\n";
    return engine.orders().book().check_invariants() ? 0 : 1;
}
