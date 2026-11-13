#include "MatchingEngine.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using matching::MatchingEngine;
using matching::OrderId;
using matching::Price;
using matching::Quantity;
using matching::Side;
using Clock = std::chrono::steady_clock;

struct Measurement {
    std::string workload;
    std::size_t operations;
    double milliseconds;
    double nanoseconds_per_operation;
    double operations_per_second;
    std::uint64_t checksum;
};

std::vector<std::size_t> sizes() {
    return {1000, 10000, 100000};
}

Measurement benchmark_submit(std::size_t n) {
    MatchingEngine engine("TEST");
    const auto start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        const auto result = engine.submitOrder({static_cast<OrderId>(i + 1), "TEST",
                                                Side::BUY,
                                                static_cast<Price>(80 + i), 1});
        if (!result.accepted()) throw std::runtime_error("submit benchmark rejected an order");
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return {"submit_non_crossing", n, elapsed, elapsed * 1'000'000.0 / static_cast<double>(n),
            static_cast<double>(n) / (elapsed / 1000.0),
            static_cast<std::uint64_t>(engine.orders().history_count() + engine.orders().book().active_count())};
}

Measurement benchmark_cancel(std::size_t n) {
    MatchingEngine engine("TEST");
    for (std::size_t i = 0; i < n; ++i) {
        const auto result = engine.submitOrder({static_cast<OrderId>(i + 1), "TEST",
                                                Side::BUY,
                                                static_cast<Price>(80 + i), 1});
        if (!result.accepted()) throw std::runtime_error("cancel setup rejected an order");
    }
    const auto start = Clock::now();
    std::size_t canceled = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (engine.cancelOrder(static_cast<OrderId>(i + 1)).canceled()) ++canceled;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (canceled != n || engine.orders().book().active_count() != 0)
        throw std::runtime_error("cancel benchmark left active orders");
    return {"cancel_active_orders", n, elapsed, elapsed * 1'000'000.0 / static_cast<double>(n),
            static_cast<double>(n) / (elapsed / 1000.0),
            static_cast<std::uint64_t>(canceled + engine.orders().history_count())};
}

Measurement benchmark_match(std::size_t n) {
    MatchingEngine engine("TEST");
    const auto start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        const auto passive = engine.submitOrder({static_cast<OrderId>(2 * i + 1), "TEST",
                                                 Side::SELL, 100, 1});
        if (!passive.accepted()) throw std::runtime_error("match setup rejected a passive order");
        const auto aggressive = engine.submitOrder({static_cast<OrderId>(2 * i + 2), "TEST",
                                                    Side::BUY, 101, 1});
        if (!aggressive.accepted()) throw std::runtime_error("match benchmark rejected an aggressive order");
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (engine.trades().size() != n || engine.orders().book().active_count() != 0)
        throw std::runtime_error("match benchmark produced an unexpected final state");
    return {"match_one_trade_per_order", n, elapsed, elapsed * 1'000'000.0 / static_cast<double>(n),
            static_cast<double>(n) / (elapsed / 1000.0),
            static_cast<std::uint64_t>(engine.trades().size() + engine.orders().history_count())};
}

void write_header(std::ostream& output) {
    output << "workload,operations,milliseconds,nanoseconds_per_operation,operations_per_second,checksum\n";
}

void write_measurement(std::ostream& output, const Measurement& m) {
    output << m.workload << ',' << m.operations << ',' << std::fixed << std::setprecision(3)
           << m.milliseconds << ',' << std::setprecision(1) << m.nanoseconds_per_operation << ','
           << std::setprecision(1) << m.operations_per_second << ',' << m.checksum << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string output_path;
        if (argc == 3 && std::string(argv[1]) == "--output") output_path = argv[2];
        else if (argc != 1) {
            std::cerr << "usage: performance_benchmark [--output path]\n";
            return 2;
        }

        std::ofstream file;
        std::ostream* output = &std::cout;
        if (!output_path.empty()) {
            file.open(output_path, std::ios::trunc);
            if (!file) throw std::runtime_error("cannot open benchmark output");
            output = &file;
        }

        *output << "# compiler=" << __VERSION__ << "\n";
        *output << "# cxx_standard=201703\n";
        *output << "# hardware_concurrency=" << std::thread::hardware_concurrency() << "\n";
        *output << "# clock=steady_clock\n";
        *output << "# timed_region=only_the_operation_loop; setup_and_validation_excluded\n";
        *output << "# workloads=submit_non_crossing,cancel_active_orders,match_one_trade_per_order\n";
        write_header(*output);

        std::uint64_t checksum = 0;
        for (const auto n : sizes()) {
            for (const auto measurement : {benchmark_submit(n), benchmark_cancel(n), benchmark_match(n)}) {
                write_measurement(*output, measurement);
                checksum ^= measurement.checksum + static_cast<std::uint64_t>(n);
            }
        }
        std::cerr << "benchmark_checksum=" << checksum << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "benchmark_error=" << error.what() << '\n';
        return 1;
    }
}
