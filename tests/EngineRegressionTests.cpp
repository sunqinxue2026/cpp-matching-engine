#include "MatchingEngine.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <random>
#include <vector>
using namespace matching;

TEST(Submission, InvalidRequestsReserveOnlyNonzeroIds) {
    MatchingEngine e("TEST");
    EXPECT_EQ(e.submitOrder({0,"TEST",Side::BUY,100,1}).error,SubmitError::INVALID_ID);
    struct Input { Price p; Quantity q; Side side; };
    std::vector<Input> inputs{{0,1,Side::BUY},{-1,1,Side::SELL},{100,0,Side::BUY},
        {100,-1,Side::SELL},{100,1,static_cast<Side>(42)}};
    OrderId id=1;
    for(const auto& x:inputs) {
        EXPECT_EQ(e.submitOrder({id,"TEST",x.side,x.p,x.q}).error,SubmitError::INVALID_ORDER);
        EXPECT_EQ(e.cancelOrder(id).error,CancelError::REJECTED_ORDER);
        EXPECT_EQ(e.submitOrder({id,"TEST",Side::BUY,100,1}).error,SubmitError::DUPLICATE_ID);
        ++id;
    }
    EXPECT_EQ(e.submitOrder({id,"OTHER",Side::BUY,100,1}).error,SubmitError::SYMBOL_MISMATCH);
    EXPECT_EQ(e.orders().last_sequence(),0u);
    EXPECT_EQ(e.orders().history_count(),0u);
    EXPECT_TRUE(e.trades().empty());
    EXPECT_TRUE(e.orders().book().check_invariants());
}

TEST(Matching, PassivePriceAndFifoBothDirections) {
    for(auto side:{Side::BUY,Side::SELL}) {
        MatchingEngine e("TEST");
        auto passive=side==Side::BUY?Side::SELL:Side::BUY;
        ASSERT_TRUE(e.submitOrder({1,"TEST",passive,100,10}).accepted());
        ASSERT_TRUE(e.submitOrder({2,"TEST",passive,100,10}).accepted());
        ASSERT_TRUE(e.submitOrder({3,"TEST",side,side==Side::BUY?101:99,15}).accepted());
        ASSERT_EQ(e.trades().size(),2u);
        EXPECT_EQ(e.trades()[0].trade_price,100);
        EXPECT_EQ(e.trades()[1].trade_price,100);
        EXPECT_EQ(e.orders().find(1)->status(),OrderStatus::FILLED);
        EXPECT_EQ(e.orders().find(2)->remaining_quantity(),5);
        EXPECT_EQ(e.orders().find(2)->status(),OrderStatus::PARTIALLY_FILLED);
        EXPECT_EQ(e.orders().book().best_order(passive),2u);
        EXPECT_TRUE(e.orders().book().check_invariants());
    }
}

TEST(Matching, PricePriorityCrossLevelsAndResidual) {
    MatchingEngine e("TEST");
    e.submitOrder({1,"TEST",Side::SELL,102,10});
    e.submitOrder({2,"TEST",Side::SELL,100,20});
    e.submitOrder({3,"TEST",Side::BUY,103,40});
    ASSERT_EQ(e.trades().size(),2u);
    EXPECT_EQ(e.trades()[0].sell_order_id,2u);
    EXPECT_EQ(e.trades()[1].sell_order_id,1u);
    EXPECT_EQ(e.orders().find(3)->remaining_quantity(),10);
    auto d=e.marketDepth();
    EXPECT_EQ(d.best_bid,103);
    EXPECT_FALSE(d.best_ask);
    ASSERT_EQ(d.bids.size(),1u);
    EXPECT_EQ(d.bids[0].quantity,10);
}

TEST(Cancellation, TerminalOrdersNeverMatchAgainAndSnapshotsAreIndependent) {
    MatchingEngine e("TEST");
    e.submitOrder({1,"TEST",Side::SELL,100,20});
    auto before=e.marketDepth();
    e.submitOrder({2,"TEST",Side::BUY,100,5});
    EXPECT_TRUE(e.cancelOrder(1).canceled());
    EXPECT_EQ(e.cancelOrder(1).error,CancelError::ALREADY_CANCELED);
    EXPECT_EQ(e.cancelOrder(2).error,CancelError::ALREADY_FILLED);
    EXPECT_EQ(e.cancelOrder(999).error,CancelError::NOT_FOUND);
    e.submitOrder({3,"TEST",Side::BUY,101,30});
    EXPECT_EQ(e.trades().size(),1u);
    EXPECT_EQ(e.orders().find(1)->remaining_quantity(),15);
    EXPECT_EQ(before.asks[0].quantity,20);
    EXPECT_EQ(e.marketDepth().active_order_count,1u);
    EXPECT_EQ(e.orders().submission(1)->status,OrderStatus::NEW);
}

TEST(Boundaries, TradeSequenceBatchRejectionIsAtomic) {
    const auto maximum=std::numeric_limits<SequenceId>::max();
    MatchingEngine e("TEST",0,maximum-1);
    e.submitOrder({1,"TEST",Side::SELL,100,1});
    e.submitOrder({2,"TEST",Side::SELL,100,1});
    EXPECT_EQ(e.submitOrder({3,"TEST",Side::BUY,100,2}).error,SubmitError::TRADE_SEQUENCE_EXHAUSTED);
    EXPECT_EQ(e.orders().find(1)->remaining_quantity(),1);
    EXPECT_EQ(e.orders().find(2)->remaining_quantity(),1);
    EXPECT_EQ(e.orders().last_sequence(),2u);
    EXPECT_TRUE(e.trades().empty());
    ASSERT_TRUE(e.submitOrder({4,"TEST",Side::BUY,100,1}).accepted());
    EXPECT_EQ(e.trades().back().trade_sequence,maximum);
    EXPECT_EQ(e.submitOrder({5,"TEST",Side::BUY,100,1}).error,SubmitError::TRADE_SEQUENCE_EXHAUSTED);
    EXPECT_TRUE(e.orders().book().check_invariants());
}

TEST(Boundaries, LevelOverflowDoesNotMutateExistingOrders) {
    MatchingEngine e("TEST");
    auto maximum=std::numeric_limits<Quantity>::max();
    ASSERT_TRUE(e.submitOrder({1,"TEST",Side::BUY,100,maximum}).accepted());
    EXPECT_EQ(e.submitOrder({2,"TEST",Side::BUY,100,1}).error,SubmitError::LEVEL_OVERFLOW);
    EXPECT_EQ(e.marketDepth().bids[0].quantity,maximum);
    EXPECT_EQ(e.orders().history_count(),1u);
    EXPECT_TRUE(e.cancelOrder(1).canceled());
    EXPECT_TRUE(e.submitOrder({3,"TEST",Side::BUY,100,1}).accepted());
}

// Deliberately slow reference: linear best-order search; no production book/index reuse.
struct RefOrder { OrderId id; Side side; Price price; Quantity original,remaining; OrderStatus status; };
struct RefTrade { OrderId buy,sell; Price price; Quantity quantity; };
TEST(Differential, SeededMixedFlowMatchesIndependentReference) {
    for(unsigned seed:{7u,2026u,113u}) {
        SCOPED_TRACE(seed);
        MatchingEngine e("TEST");
        std::vector<RefOrder> orders;
        std::vector<RefTrade> trades;
        std::mt19937 rng(seed);
        for(int step=0;step<800;++step) {
            SCOPED_TRACE(step);
            if(!orders.empty() && rng()%4==0) {
                auto& o=orders[rng()%orders.size()];
                auto actual=e.cancelOrder(o.id);
                bool active=o.status==OrderStatus::NEW || o.status==OrderStatus::PARTIALLY_FILLED;
                EXPECT_EQ(actual.canceled(),active);
                if(active) o.status=OrderStatus::CANCELED;
            } else {
                RefOrder incoming{static_cast<OrderId>(orders.size()+1),rng()%2?Side::BUY:Side::SELL,
                    static_cast<Price>(95+rng()%11),static_cast<Quantity>(1+rng()%25),0,OrderStatus::NEW};
                incoming.remaining=incoming.original;
                while(incoming.remaining>0) {
                    RefOrder* best=nullptr;
                    for(auto& o:orders) {
                        if(o.side==incoming.side || (o.status!=OrderStatus::NEW && o.status!=OrderStatus::PARTIALLY_FILLED)) continue;
                        if(incoming.side==Side::BUY ? o.price>incoming.price : o.price<incoming.price) continue;
                        if(!best || (incoming.side==Side::BUY ? o.price<best->price : o.price>best->price)) best=&o;
                    }
                    if(!best) break;
                    auto q=std::min(incoming.remaining,best->remaining);
                    trades.push_back({incoming.side==Side::BUY?incoming.id:best->id,
                        incoming.side==Side::SELL?incoming.id:best->id,best->price,q});
                    best->remaining-=q;
                    best->status=best->remaining?OrderStatus::PARTIALLY_FILLED:OrderStatus::FILLED;
                    incoming.remaining-=q;
                }
                incoming.status=incoming.remaining==0?OrderStatus::FILLED:
                    incoming.remaining<incoming.original?OrderStatus::PARTIALLY_FILLED:OrderStatus::NEW;
                ASSERT_TRUE(e.submitOrder({incoming.id,"TEST",incoming.side,incoming.price,incoming.original}).accepted());
                orders.push_back(incoming);
            }
            ASSERT_TRUE(e.orders().book().check_invariants());
            ASSERT_EQ(e.trades().size(),trades.size());
            std::vector<Quantity> executed(orders.size()+1,0);
            for(std::size_t i=0;i<trades.size();++i) {
                const auto& actual=e.trades()[i]; const auto& expected=trades[i];
                ASSERT_EQ(actual.buy_order_id,expected.buy); ASSERT_EQ(actual.sell_order_id,expected.sell);
                ASSERT_EQ(actual.trade_price,expected.price); ASSERT_EQ(actual.trade_quantity,expected.quantity);
                ASSERT_EQ(actual.trade_id,i+1); ASSERT_EQ(actual.trade_sequence,i+1);
                executed[actual.buy_order_id]+=actual.trade_quantity; executed[actual.sell_order_id]+=actual.trade_quantity;
            }
            std::size_t active=0;
            for(const auto& o:orders) {
                const auto* actual=e.orders().find(o.id); ASSERT_NE(actual,nullptr);
                ASSERT_EQ(actual->remaining_quantity(),o.remaining); ASSERT_EQ(actual->status(),o.status);
                ASSERT_EQ(actual->quantity(),executed[o.id]+actual->remaining_quantity());
                if(o.status==OrderStatus::NEW || o.status==OrderStatus::PARTIALLY_FILLED) ++active;
            }
            auto depth=e.marketDepth(); ASSERT_EQ(depth.active_order_count,active);
            for(auto side:{Side::BUY,Side::SELL}) {
                std::vector<Price> prices;
                for(const auto& o:orders) if(o.side==side && (o.status==OrderStatus::NEW || o.status==OrderStatus::PARTIALLY_FILLED)) prices.push_back(o.price);
                std::sort(prices.begin(),prices.end()); prices.erase(std::unique(prices.begin(),prices.end()),prices.end());
                if(side==Side::BUY) std::reverse(prices.begin(),prices.end());
                const auto& levels=side==Side::BUY?depth.bids:depth.asks;
                ASSERT_EQ(levels.size(),prices.size());
                for(std::size_t i=0;i<prices.size();++i) {
                    Quantity total=0; std::size_t count=0;
                    for(const auto& o:orders) if(o.side==side && o.price==prices[i] && (o.status==OrderStatus::NEW || o.status==OrderStatus::PARTIALLY_FILLED)) { total+=o.remaining; ++count; }
                    ASSERT_EQ(levels[i].price,prices[i]); ASSERT_EQ(levels[i].quantity,total); ASSERT_EQ(levels[i].order_count,count);
                }
                auto best=side==Side::BUY?depth.best_bid:depth.best_ask;
                ASSERT_EQ(best,prices.empty()?std::optional<Price>{}:std::optional<Price>{prices.front()});
            }
            if(depth.best_bid && depth.best_ask) ASSERT_LT(*depth.best_bid,*depth.best_ask);
        }
    }
}
