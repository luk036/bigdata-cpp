#pragma once

#include <cmath>        // for ldexp, log
#include <cstddef>      // for size_t
#include <cstdint>      // for uint64_t, uint8_t
#include <stdexcept>    // for invalid_argument
#include <string_view>  // for string_view
#include <vector>       // for vector

#include <bigdata/hll_hash.hpp>  // for fmix64, hash_bytes

namespace bigdata {

/**
 * @brief HyperLogLog cardinality estimator.
 *
 * Uses @f$m = 2^p@f$ one-byte registers and the shared 64-bit hash. The register
 * index is taken from the top @c p bits and the stored value is the number of
 * leading zeros in the remaining @f$64 - p@f$ bits plus one (so a register never
 * holds a rank of zero). Merging two sketches is an element-wise maximum, which
 * is associative and commutative.
 */
class HyperLogLog {
  public:
    /**
     * @brief Construct a HyperLogLog sketch.
     * @param[in] p Precision in bits; @f$4 \le p \le 16@f$.
     * @throws std::invalid_argument if @p p is out of range.
     */
    explicit HyperLogLog(int p = 14)
        : p_{p}, m_{std::size_t{1} << p_}, registers_(std::size_t{1} << p_, 0) {
        if (p < 4 || p > 16) {
            throw std::invalid_argument("p must be between 4 and 16");
        }
        if (m_ == 16) {
            alpha_ = 0.673;
        } else if (m_ == 32) {
            alpha_ = 0.697;
        } else if (m_ == 64) {
            alpha_ = 0.709;
        } else {
            alpha_ = 0.7213 / (1.0 + 1.079 / static_cast<double>(m_));
        }
    }

    /**
     * @brief Add a pre-computed 64-bit hash.
     * @param[in] hash The hash produced by bigdata::hash_value.
     */
    void add_hash(std::uint64_t hash) {
        const auto index = static_cast<std::size_t>(hash >> (64 - p_));
        const auto remaining = hash & ((std::uint64_t{1} << (64 - p_)) - 1U);
        const auto rank = rank_of(remaining);
        if (rank > registers_[index]) {
            registers_[index] = rank;
        }
    }

    /**
     * @brief Add an integral key (hashed then folded).
     * @tparam T An integral type.
     * @param[in] key The key to add.
     */
    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    void add(T key) {
        add_hash(hash_value(key));
    }

    /**
     * @brief Add a string value (hashed then folded).
     * @param[in] value The string to add.
     */
    void add(std::string_view value) { add_hash(hash_value(value)); }

    /**
     * @brief Estimate the number of distinct keys seen.
     * @return The cardinality estimate.
     */
    [[nodiscard]] auto count() const -> double {
        double z = 0.0;
        std::size_t zeros = 0;
        for (const auto reg : registers_) {
            z += std::ldexp(1.0, -static_cast<int>(reg));
            if (reg == 0) {
                ++zeros;
            }
        }
        double estimate = alpha_ * static_cast<double>(m_) * static_cast<double>(m_) / z;
        if (estimate <= 2.5 * static_cast<double>(m_)) {
            if (zeros != 0) {
                estimate = static_cast<double>(m_) * std::log(static_cast<double>(m_) /
                                                              static_cast<double>(zeros));
            }
        } else if (estimate > (1.0 / 30.0) * TWO_POW_64) {
            estimate = -TWO_POW_64 * std::log(1.0 - estimate / TWO_POW_64);
        }
        return estimate;
    }

    /**
     * @brief Merge another sketch into this one (element-wise maximum).
     * @param[in] other A sketch with the same precision.
     * @throws std::invalid_argument if the precisions differ.
     */
    void merge(const HyperLogLog& other) {
        if (p_ != other.p_) {
            throw std::invalid_argument("cannot merge sketches with different precision");
        }
        for (std::size_t i = 0; i < m_; ++i) {
            if (other.registers_[i] > registers_[i]) {
                registers_[i] = other.registers_[i];
            }
        }
    }

    /// Reset all registers to zero.
    void reset() { std::fill(registers_.begin(), registers_.end(), 0); }

    /// @return The precision @c p.
    [[nodiscard]] auto p() const -> int { return p_; }
    /// @return The number of registers @f$2^p@f$.
    [[nodiscard]] auto m() const -> std::size_t { return m_; }
    /// @return A const reference to the raw registers.
    [[nodiscard]] auto registers() const -> const std::vector<std::uint8_t>& {
        return registers_;
    }

  private:
    static constexpr double TWO_POW_64 = 18446744073709551616.0;  ///< 2^64 as a double.

    /// Rank = leading zeros in the (64 - p)-bit field, plus one.
    [[nodiscard]] auto rank_of(std::uint64_t remaining) const -> std::uint8_t {
        const int bits = 64 - p_;
        if (remaining == 0) {
            return static_cast<std::uint8_t>(bits + 1);
        }
        int zeros = 0;
        std::uint64_t mask = std::uint64_t{1} << (bits - 1);
        while ((remaining & mask) == 0) {
            ++zeros;
            mask >>= 1U;
        }
        return static_cast<std::uint8_t>(zeros + 1);
    }

    int p_;
    std::size_t m_;
    double alpha_{0.0};
    std::vector<std::uint8_t> registers_;
};

}  // namespace bigdata
