#pragma once

#include <cmath>        // for ceil, log2, pow
#include <cstddef>      // for size_t
#include <cstdint>      // for uint64_t, uint8_t
#include <stdexcept>    // for invalid_argument
#include <string_view>  // for string_view
#include <vector>       // for vector

#include <bigdata/hll_core.hpp>  // for rank_of, alpha_for, estimate_from_registers
#include <bigdata/hll_hash.hpp>  // for hash_value

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
    explicit HyperLogLog(int p = 14) : p_{p}, m_{std::size_t{1} << p_}, registers_(m_, 0) {
        if (p < 4 || p > 16) {
            throw std::invalid_argument("p must be between 4 and 16");
        }
    }

    /**
     * @brief Construct a sketch sized for a target relative error.
     * @param[in] rel_error Target standard error, e.g. 0.02 for 2%.
     * @return A sketch whose precision gives a standard error of about @p rel_error.
     * @throws std::invalid_argument if @p rel_error is not in @f$(0, 1)@f$.
     */
    [[nodiscard]] static auto from_error(double rel_error = 0.02) -> HyperLogLog {
        if (!(rel_error > 0.0 && rel_error < 1.0)) {
            throw std::invalid_argument("relative error must be in (0, 1)");
        }
        const auto m = static_cast<std::size_t>(std::ceil(std::pow(1.04 / rel_error, 2.0)));
        int p = static_cast<int>(std::ceil(std::log2(static_cast<double>(m))));
        if (p < 4) {
            p = 4;
        }
        if (p > 16) {
            p = 16;
        }
        return HyperLogLog{p};
    }

    /**
     * @brief Add a pre-computed 64-bit hash.
     * @param[in] hash The hash produced by bigdata::hash_value.
     */
    void add_hash(std::uint64_t hash) {
        const auto index = static_cast<std::size_t>(hash >> (64 - p_));
        const auto remaining = hash & ((std::uint64_t{1} << (64 - p_)) - 1U);
        const auto rank = detail::rank_of(remaining, 64 - p_);
        if (rank > registers_[index]) {
            registers_[index] = static_cast<std::uint8_t>(rank);
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
        return detail::estimate_from_registers(registers_.begin(), registers_.end(), p_);
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
    int p_;
    std::size_t m_;
    std::vector<std::uint8_t> registers_;
};

}  // namespace bigdata
