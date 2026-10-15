#pragma once

#include <cmath>    // for ldexp, log
#include <cstddef>  // for size_t
#include <cstdint>  // for uint64_t

#if defined(__CUDACC__) || defined(__CUDA_ARCH__)
#define BIGDATA_HOST_DEVICE __host__ __device__
#else
#define BIGDATA_HOST_DEVICE
#endif

namespace bigdata {

/**
 * @brief MurmurHash3 64-bit finalizer (usable on host and device).
 * @param[in] x The value to mix.
 * @return A well-distributed 64-bit hash.
 */
[[nodiscard]] BIGDATA_HOST_DEVICE constexpr auto fmix64(std::uint64_t x) noexcept -> std::uint64_t {
    x ^= x >> 33U;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33U;
    x *= 0xC4CEB9FE1A85EC53ULL;
    x ^= x >> 33U;
    return x;
}

namespace detail {

/**
 * @brief Rank = leading zeros in the low @p bits bits, plus one.
 * @param[in] remaining The low @c bits bits of a hash.
 * @param[in] bits Number of bits to consider.
 * @return A rank in @f$[1, bits + 1]@f$.
 */
BIGDATA_HOST_DEVICE constexpr auto rank_of(std::uint64_t remaining, int bits) noexcept
    -> unsigned int {
    if (remaining == 0) {
        return static_cast<unsigned int>(bits + 1);
    }
    int zeros = 0;
    std::uint64_t mask = std::uint64_t{1} << (bits - 1);
    while ((remaining & mask) == 0) {
        ++zeros;
        mask >>= 1U;
    }
    return static_cast<unsigned int>(zeros + 1);
}

/**
 * @brief Bias-correction constant for @f$m = 2^p@f$ registers.
 * @param[in] m The number of registers.
 * @return The alpha constant.
 */
constexpr auto alpha_for(std::size_t m) noexcept -> double {
    if (m == 16) {
        return 0.673;
    }
    if (m == 32) {
        return 0.697;
    }
    if (m == 64) {
        return 0.709;
    }
    return 0.7213 / (1.0 + 1.079 / static_cast<double>(m));
}

/// @f$2^{64}@f$ as a double (the large-range correction threshold).
inline constexpr double TWO_POW_64 = 18446744073709551616.0;

/**
 * @brief Harmonic-mean cardinality estimate with the standard corrections.
 * @tparam Iterator A forward iterator over the registers.
 * @param[in] first Begin iterator.
 * @param[in] last End iterator.
 * @param[in] p Precision in bits.
 * @return The cardinality estimate.
 */
template <typename Iterator>
auto estimate_from_registers(Iterator first, Iterator last, int p) -> double {
    const auto m = std::size_t{1} << p;
    double z = 0.0;
    std::size_t zeros = 0;
    for (auto it = first; it != last; ++it) {
        z += std::ldexp(1.0, -static_cast<int>(*it));
        if (*it == 0) {
            ++zeros;
        }
    }
    double estimate = alpha_for(m) * static_cast<double>(m) * static_cast<double>(m) / z;
    if (estimate <= 2.5 * static_cast<double>(m)) {
        if (zeros != 0) {
            estimate = static_cast<double>(m) *
                       std::log(static_cast<double>(m) / static_cast<double>(zeros));
        }
    } else if (estimate > (1.0 / 30.0) * TWO_POW_64) {
        estimate = -TWO_POW_64 * std::log(1.0 - estimate / TWO_POW_64);
    }
    return estimate;
}

}  // namespace detail

}  // namespace bigdata

#undef BIGDATA_HOST_DEVICE
