#pragma once

#include <algorithm>   // for min, sort
#include <cmath>       // for exp
#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t
#include <stdexcept>   // for invalid_argument
#include <utility>     // for pair
#include <vector>      // for vector

namespace bigdata {

/**
 * @brief Update strategy for the Count-Min Sketch.
 */
enum class UpdateMode {
    Min,           ///< Classic CMS: increment every row (strictly over-counts).
    Conservative,  ///< Increment only the row counters currently holding the minimum.
};

namespace detail {

/// SplitMix64: a small deterministic PRNG used to derive the hash parameters.
constexpr auto splitmix64(std::uint64_t& state) noexcept -> std::uint64_t {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31U);
}

}  // namespace detail

/**
 * @brief Count-Min Sketch for event-frequency estimation.
 *
 * Beyond the classic estimator, the sketch tracks the total stream weight so the
 * additive @f$\varepsilon N@f$ error bound is available, offers a
 * bias-corrected Count-Mean-Min estimator, and can run in a conservative-update
 * mode that sharply reduces over-counting. Hash parameters are a deterministic
 * function of @c width and @c depth, so equal-sized sketches merge exactly.
 */
class CountMinSketch {
  public:
    /**
     * @brief Construct a Count-Min Sketch.
     * @param[in] width Number of counters per row.
     * @param[in] depth Number of rows (hash functions).
     * @param[in] mode Update strategy.
     * @throws std::invalid_argument if @p width or @p depth is zero.
     */
    explicit CountMinSketch(std::size_t width = 1000, std::size_t depth = 5,
                            UpdateMode mode = UpdateMode::Min)
        : width_{width},
          depth_{depth},
          mode_{mode},
          counters_(width * depth, 0.0),
          hash_a_(depth),
          hash_b_(depth) {
        if (width == 0 || depth == 0) {
            throw std::invalid_argument("width and depth must be positive");
        }
        std::uint64_t state = (static_cast<std::uint64_t>(width) << 20U) ^
                              static_cast<std::uint64_t>(depth);
        for (std::size_t row = 0; row < depth_; ++row) {
            hash_a_[row] = detail::splitmix64(state) % (MODULUS - 1U) + 1U;
            hash_b_[row] = detail::splitmix64(state) % MODULUS;
        }
    }

    /**
     * @brief Add @p value for @p key.
     * @param[in] key The key (e.g. a node id).
     * @param[in] value The weight to add.
     */
    void update(std::uint64_t key, double value = 1.0) {
        total_ += value;
        if (mode_ == UpdateMode::Conservative) {
            double current_min = counters_[index(key, 0)];
            for (std::size_t row = 1; row < depth_; ++row) {
                current_min = std::min(current_min, counters_[index(key, row)]);
            }
            for (std::size_t row = 0; row < depth_; ++row) {
                const std::size_t idx = index(key, row);
                if (counters_[idx] == current_min) {
                    counters_[idx] += value;
                }
            }
            return;
        }
        for (std::size_t row = 0; row < depth_; ++row) {
            counters_[index(key, row)] += value;
        }
    }

    /**
     * @brief Query the classic minimum estimate (never under-counts).
     * @param[in] key The key to query.
     * @return The minimum counter across rows.
     */
    [[nodiscard]] auto query(std::uint64_t key) const -> double {
        double result = counters_[index(key, 0)];
        for (std::size_t row = 1; row < depth_; ++row) {
            result = std::min(result, counters_[index(key, row)]);
        }
        return result;
    }

    /**
     * @brief Query the Count-Mean-Min (bias-corrected) estimate.
     *
     * Each row counter is reduced by the expected collision noise
     * @f$(N - c) / (\text{width} - 1)@f$ before the median across rows is taken.
     * @param[in] key The key to query.
     * @return A nearly unbiased estimate (never negative).
     */
    [[nodiscard]] auto query_mean_min(std::uint64_t key) const -> double {
        std::vector<double> estimates;
        estimates.reserve(depth_);
        for (std::size_t row = 0; row < depth_; ++row) {
            double counter = counters_[index(key, row)];
            if (width_ > 1) {
                counter -= (total_ - counter) / static_cast<double>(width_ - 1);
            }
            estimates.push_back(std::max(0.0, counter));
        }
        const std::size_t mid = estimates.size() / 2;
        std::sort(estimates.begin(), estimates.end());
        if (estimates.size() % 2 == 1) {
            return estimates[mid];
        }
        return 0.5 * (estimates[mid - 1] + estimates[mid]);
    }

    /**
     * @brief Query an estimate together with its additive error bound.
     * @param[in] key The key to query.
     * @return A pair of (minimum estimate, @f$\varepsilon N@f$ bound).
     */
    [[nodiscard]] auto estimate_with_error(std::uint64_t key) const
        -> std::pair<double, double> {
        return {query(key), additive_error_bound()};
    }

    /**
     * @brief The additive error bound @f$e \cdot N / \text{width}@f$.
     * @return The bound over the whole stream so far.
     */
    [[nodiscard]] auto additive_error_bound() const -> double {
        return std::exp(1.0) * total_ / static_cast<double>(width_);
    }

    /**
     * @brief Merge another sketch into a new one (element-wise addition).
     * @param[in] other A sketch with identical dimensions.
     * @return The merged sketch.
     * @throws std::invalid_argument on dimension mismatch.
     */
    [[nodiscard]] auto merge(const CountMinSketch& other) const -> CountMinSketch {
        if (width_ != other.width_ || depth_ != other.depth_) {
            throw std::invalid_argument("sketches must have same dimensions to merge");
        }
        CountMinSketch merged{width_, depth_, mode_};
        for (std::size_t i = 0; i < counters_.size(); ++i) {
            merged.counters_[i] = counters_[i] + other.counters_[i];
        }
        merged.total_ = total_ + other.total_;
        return merged;
    }

    /// Reset all counters and the total weight to zero.
    void reset() {
        std::fill(counters_.begin(), counters_.end(), 0.0);
        total_ = 0.0;
    }

    /// @return The width (counters per row).
    [[nodiscard]] auto width() const -> std::size_t { return width_; }
    /// @return The depth (number of rows).
    [[nodiscard]] auto depth() const -> std::size_t { return depth_; }
    /// @return The total weight added so far.
    [[nodiscard]] auto total() const -> double { return total_; }
    /// @return The update mode.
    [[nodiscard]] auto mode() const -> UpdateMode { return mode_; }
    /// @return The raw counter at (row, column).
    [[nodiscard]] auto counter(std::size_t row, std::size_t column) const -> double {
        return counters_[row * width_ + column];
    }

  private:
    static constexpr std::uint64_t MODULUS = 2147483647ULL;  ///< Mersenne prime 2^31 - 1.

    [[nodiscard]] auto index(std::uint64_t key, std::size_t row) const -> std::size_t {
        const std::uint64_t reduced = key % MODULUS;
        const std::uint64_t hashed =
            (hash_a_[row] * reduced + hash_b_[row]) % MODULUS;
        return (static_cast<std::size_t>(hashed) % width_) + row * width_;
    }

    std::size_t width_;
    std::size_t depth_;
    UpdateMode mode_;
    std::vector<double> counters_;
    std::vector<std::uint64_t> hash_a_;
    std::vector<std::uint64_t> hash_b_;
    double total_{0.0};
};

}  // namespace bigdata
