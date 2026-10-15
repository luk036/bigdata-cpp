#pragma once

#include <cstdint>       // for uint64_t, uint8_t
#include <string_view>   // for string_view
#include <type_traits>   // for enable_if_t, is_integral_v
#include <vector>        // for vector

#include <bigdata/hll_core.hpp>  // for fmix64

namespace bigdata {

/**
 * @brief A single 64-bit hash family shared by every sketch in the library.
 *
 * Arbitrary keys are reduced to a 64-bit integer and passed through the
 * MurmurHash3 finalizer, so Count-Min Sketch and HyperLogLog agree on the hash
 * of a key. Folding a string hashes *every* byte (FNV-1a) before finalising, so
 * keys that share a prefix still hash differently.
 */

/**
 * @brief FNV-1a fold over every byte, finalised with fmix64.
 * @param[in] data The bytes to hash.
 * @return A 64-bit hash of the byte sequence.
 */
[[nodiscard]] constexpr auto hash_bytes(std::string_view data) noexcept -> std::uint64_t {
    std::uint64_t h = 0;
    for (const char ch : data) {
        h = (h ^ static_cast<std::uint8_t>(ch)) * 0x00000100000001B3ULL;
    }
    return fmix64(h ^ (static_cast<std::uint64_t>(data.size()) << 32U));
}

/**
 * @brief Hash a string deterministically.
 * @param[in] value The string to hash.
 * @return A 64-bit hash.
 */
[[nodiscard]] constexpr auto hash_value(std::string_view value) noexcept -> std::uint64_t {
    return hash_bytes(value);
}

/**
 * @brief Hash an integral key (reduced to 64 bits and mixed).
 * @tparam T An integral type.
 * @param[in] value The key to hash.
 * @return A 64-bit hash.
 */
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
[[nodiscard]] constexpr auto hash_value(T value) noexcept -> std::uint64_t {
    return fmix64(static_cast<std::uint64_t>(value));
}

/**
 * @brief Hash a batch of strings.
 * @param[in] items The strings to hash.
 * @return A vector of 64-bit hashes, aligned with @p items.
 */
[[nodiscard]] inline auto hash_strings(const std::vector<std::string_view>& items)
    -> std::vector<std::uint64_t> {
    std::vector<std::uint64_t> out;
    out.reserve(items.size());
    for (const auto item : items) {
        out.push_back(hash_bytes(item));
    }
    return out;
}

}  // namespace bigdata
