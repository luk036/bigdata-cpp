#include <bigdata/hll_hash.hpp>
#include <bigdata/hyper_loglog.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

using bigdata::hash_bytes;
using bigdata::hash_strings;
using bigdata::hash_value;
using bigdata::HyperLogLog;

TEST_CASE("Hashing is deterministic and 64-bit") {
    CHECK(hash_value(std::string_view{"item_0"}) == hash_value(std::string_view{"item_0"}));
    CHECK(hash_value(std::uint64_t{42}) == hash_value(std::uint64_t{42}));
    CHECK(hash_value(42) == bigdata::fmix64(42U));
    CHECK(hash_bytes("") == bigdata::fmix64(0U));
}

TEST_CASE("Hashing uses every byte so prefixes differ") {
    CHECK(hash_bytes("item_00000001") != hash_bytes("item_00000002"));
    std::vector<std::uint64_t> seen;
    seen.reserve(5000);
    for (int i = 0; i < 5000; ++i) {
        seen.push_back(hash_bytes("item_" + std::to_string(i)));
    }
    // A naive first-8-bytes reduction would collapse these to 1000 distinct hashes.
    std::sort(seen.begin(), seen.end());
    CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
}

TEST_CASE("Batch string hashing matches the scalar hasher") {
    const std::vector<std::string_view> items{"", "a", "item_0", "item_123", "xyz"};
    const auto batch = hash_strings(items);
    REQUIRE(batch.size() == items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        CHECK(batch[i] == hash_bytes(items[i]));
    }
}

TEST_CASE("HyperLogLog estimates cardinality") {
    HyperLogLog hll{12};
    const std::uint64_t n = 1000;
    for (std::uint64_t i = 0; i < n; ++i) {
        hll.add(i);
    }
    const auto estimate = hll.count();
    CHECK(std::abs(estimate - static_cast<double>(n)) / static_cast<double>(n) < 0.10);
}

TEST_CASE("HyperLogLog stays accurate beyond the small range") {
    // n is large enough that the small-range linear-counting correction does not
    // apply, where the rank computation (rank = leading zeros + 1) matters.
    HyperLogLog hll{14};
    const std::uint64_t n = 200000;
    for (std::uint64_t i = 0; i < n; ++i) {
        hll.add(i);
    }
    const auto estimate = hll.count();
    CHECK(std::abs(estimate - static_cast<double>(n)) / static_cast<double>(n) < 0.05);
}

TEST_CASE("HyperLogLog add matches add_hash") {
    HyperLogLog via_key{12};
    HyperLogLog via_hash{12};
    for (std::uint64_t i = 0; i < 5000; ++i) {
        via_key.add(i);
        via_hash.add_hash(hash_value(i));
    }
    CHECK(via_key.count() == doctest::Approx(via_hash.count()));
}

TEST_CASE("HyperLogLog merge equals a single pass") {
    HyperLogLog a{12};
    HyperLogLog b{12};
    for (std::uint64_t i = 0; i < 5000; ++i) {
        a.add(i);
    }
    for (std::uint64_t i = 5000; i < 10000; ++i) {
        b.add(i);
    }
    a.merge(b);
    CHECK(std::abs(a.count() - 10000.0) / 10000.0 < 0.10);
}

TEST_CASE("HyperLogLog validates its precision") {
    CHECK_THROWS_AS(HyperLogLog(3), std::invalid_argument);
    CHECK_THROWS_AS(HyperLogLog(17), std::invalid_argument);
    HyperLogLog a{12};
    HyperLogLog b{13};
    CHECK_THROWS_AS(a.merge(b), std::invalid_argument);
}
