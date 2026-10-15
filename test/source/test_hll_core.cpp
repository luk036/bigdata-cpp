#include <bigdata/hll_core.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <doctest/doctest.h>

using bigdata::detail::alpha_for;
using bigdata::detail::estimate_from_registers;
using bigdata::detail::rank_of;

TEST_CASE("rank_of matches the leading-zero definition") {
    const int p = 12;
    const int bits = 64 - p;
    CHECK(rank_of(std::uint64_t{0}, bits) == static_cast<unsigned int>(bits + 1));
    CHECK(rank_of(std::uint64_t{1}, bits) == static_cast<unsigned int>(bits));
    CHECK(rank_of(std::uint64_t{1} << (bits - 1), bits) == 1U);
    CHECK(rank_of(std::uint64_t{1} << (bits - 2), bits) == 2U);
}

TEST_CASE("alpha_for uses the tabulated small-range constants") {
    CHECK(alpha_for(16) == doctest::Approx(0.673));
    CHECK(alpha_for(32) == doctest::Approx(0.697));
    CHECK(alpha_for(64) == doctest::Approx(0.709));
    CHECK(alpha_for(16384) == doctest::Approx(0.7213 / (1.0 + 1.079 / 16384.0)));
}

TEST_CASE("estimate_from_registers is independent of the register dtype") {
    const int p = 12;
    const std::size_t m = std::size_t{1} << p;
    std::vector<std::uint8_t> bytes(m, 0);
    for (std::size_t i = 0; i < m; i += 3) {
        bytes[i] = 1;
    }
    for (std::size_t i = 0; i < m; i += 7) {
        bytes[i] = 2;
    }
    const std::vector<unsigned int> words(bytes.begin(), bytes.end());

    const double via_bytes = estimate_from_registers(bytes.begin(), bytes.end(), p);
    const double via_words = estimate_from_registers(words.begin(), words.end(), p);
    CHECK(via_bytes == doctest::Approx(via_words));
    CHECK(via_bytes > 0.0);
}
