#include <bigdata/count_min_sketch.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include <doctest/doctest.h>

using bigdata::CountMinSketch;
using bigdata::UpdateMode;

TEST_CASE("CountMinSketch never underestimates under collisions") {
    CountMinSketch sketch{32, 4};
    std::mt19937_64 rng{0};
    std::vector<std::uint64_t> truth;
    for (std::uint64_t key = 0; key < 500; ++key) {
        const auto count = 1 + (rng() % 20);
        truth.push_back(count);
        for (std::uint64_t i = 0; i < count; ++i) {
            sketch.update(key);
        }
    }
    for (std::uint64_t key = 0; key < truth.size(); ++key) {
        CHECK(sketch.query(key) >= static_cast<double>(truth[key]));
    }
}

TEST_CASE("CountMinSketch rejects invalid dimensions") {
    CHECK_THROWS_AS(CountMinSketch(0, 5), std::invalid_argument);
    CHECK_THROWS_AS(CountMinSketch(1000, 0), std::invalid_argument);
}

TEST_CASE("CountMinSketch tracks total weight") {
    CountMinSketch sketch{64, 3};
    sketch.update(1);
    sketch.update(2, 3.0);
    CHECK(sketch.total() == doctest::Approx(4.0));
}

TEST_CASE("CountMinSketch mean-min reduces over-counting bias") {
    CountMinSketch sketch{64, 5};
    std::mt19937_64 rng{1};
    std::vector<std::uint64_t> truth;
    for (std::uint64_t key = 0; key < 1000; ++key) {
        const auto count = 1 + (rng() % 50);
        truth.push_back(count);
        for (std::uint64_t i = 0; i < count; ++i) {
            sketch.update(key);
        }
    }
    double min_error = 0.0;
    double mean_min_error = 0.0;
    for (std::uint64_t key = 0; key < truth.size(); ++key) {
        const auto true_count = static_cast<double>(truth[key]);
        min_error += sketch.query(key) - true_count;
        mean_min_error += sketch.query_mean_min(key) - true_count;
    }
    min_error /= static_cast<double>(truth.size());
    mean_min_error /= static_cast<double>(truth.size());
    CHECK(min_error > 0.0);
    CHECK(std::abs(mean_min_error) < std::abs(min_error));
}

TEST_CASE("CountMinSketch conservative update counts no more than classic") {
    CountMinSketch classic{32, 4};
    CountMinSketch conservative{32, 4, UpdateMode::Conservative};
    std::mt19937_64 rng{2};
    std::vector<std::uint64_t> truth;
    for (std::uint64_t key = 0; key < 300; ++key) {
        const auto count = 1 + (rng() % 10);
        truth.push_back(count);
        for (std::uint64_t i = 0; i < count; ++i) {
            classic.update(key);
            conservative.update(key);
        }
    }
    for (std::uint64_t key = 0; key < truth.size(); ++key) {
        const auto true_count = static_cast<double>(truth[key]);
        CHECK(conservative.query(key) >= true_count);
        CHECK(conservative.query(key) <= classic.query(key));
    }
}

TEST_CASE("CountMinSketch merge is exact, associative and commutative") {
    CountMinSketch a{50, 3};
    a.update(1);
    a.update(2);
    CountMinSketch b{50, 3};
    b.update(2);
    b.update(3);
    CountMinSketch c{50, 3};
    c.update(4, 2.0);

    CountMinSketch single_pass{50, 3};
    single_pass.update(1, 1.0);
    single_pass.update(2, 1.0);
    single_pass.update(2, 1.0);
    single_pass.update(3, 1.0);
    single_pass.update(4, 2.0);

    const auto merged = a.merge(b).merge(c);
    for (std::uint64_t key = 0; key < 5; ++key) {
        CHECK(merged.query(key) == doctest::Approx(single_pass.query(key)));
    }
    CHECK(merged.total() == doctest::Approx(single_pass.total()));

    const auto ab = a.merge(b);
    const auto ba = b.merge(a);
    for (std::uint64_t key = 0; key < 5; ++key) {
        CHECK(ab.query(key) == doctest::Approx(ba.query(key)));
    }
    const auto left = a.merge(b).merge(c);
    const auto right = a.merge(b.merge(c));
    for (std::uint64_t key = 0; key < 5; ++key) {
        CHECK(left.query(key) == doctest::Approx(right.query(key)));
    }
}

TEST_CASE("CountMinSketch merge rejects mismatched dimensions") {
    CountMinSketch a{10, 2};
    CountMinSketch b{11, 2};
    CHECK_THROWS_AS((void)a.merge(b), std::invalid_argument);
}

TEST_CASE("CountMinSketch reset clears counters and total") {
    CountMinSketch sketch{16, 2};
    sketch.update(1, 5.0);
    sketch.reset();
    CHECK(sketch.total() == doctest::Approx(0.0));
    CHECK(sketch.query(1) == doctest::Approx(0.0));
}

TEST_CASE("CountMinSketch reports the additive error bound") {
    CountMinSketch sketch{100, 3};
    for (int i = 0; i < 50; ++i) {
        sketch.update(7);
    }
    const auto [estimate, bound] = sketch.estimate_with_error(7);
    CHECK(estimate == doctest::Approx(sketch.query(7)));
    CHECK(bound == doctest::Approx(std::exp(1.0) * sketch.total() / 100.0));
}
