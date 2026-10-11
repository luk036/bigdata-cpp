/**
 * @file test_hyper_loglog_gpu.cpp
 * @brief Tests for GPU-accelerated HyperLogLog.
 *
 * When HAS_CUDA is defined the CUDA kernel is used; otherwise the CPU fallback
 * path is exercised. Both must produce the same estimates.
 */

#include <doctest/doctest.h>

#include <bigdata/hyper_loglog.hpp>
#include <bigdata/hyper_loglog_gpu.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

TEST_CASE("hll_distinct_count estimates cardinality") {
    std::vector<std::uint64_t> keys;
    const std::uint64_t n = 100000;
    keys.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        keys.push_back(i * 2654435761ULL);
    }
    const auto estimate = bigdata::hll_distinct_count(keys, 14);
    CHECK(std::abs(estimate - static_cast<double>(n)) / static_cast<double>(n) < 0.05);
}

TEST_CASE("hll_distinct_count matches the CPU HyperLogLog") {
    std::vector<std::uint64_t> keys;
    const std::uint64_t n = 200000;
    keys.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        keys.push_back(i * 40503ULL);
    }

    bigdata::HyperLogLog cpu{12};
    for (const auto key : keys) {
        cpu.add(key);
    }

    CHECK(bigdata::hll_distinct_count(keys, 12) == doctest::Approx(cpu.count()));
}

TEST_CASE("hll_distinct_count handles empty input") {
    CHECK(bigdata::hll_distinct_count(nullptr, 0, 12) == doctest::Approx(0.0));
    CHECK(bigdata::hll_distinct_count(std::vector<std::uint64_t>{}, 14) == doctest::Approx(0.0));
}
