#pragma once
#include "Order.h"
#include <string>
namespace matching {
struct Trade {
    std::uint64_t trade_id;
    std::string symbol;
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price trade_price;
    Quantity trade_quantity;
    SequenceId trade_sequence;
};
}
