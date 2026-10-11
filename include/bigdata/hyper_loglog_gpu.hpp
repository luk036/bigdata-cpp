#pragma once

#include <bigdata/hyper_loglog.hpp>  // for HyperLogLog
#include <cstddef>                   // for size_t
#include <cstdint>                   // for uint64_t
#include <vector>                    // for vector

namespace bigdata {

    /**
     * @file hyper_loglog_gpu.hpp
     * @brief GPU-accelerated HyperLogLog cardinality estimation.
     *
     * Hashes and folds the keys in a single CUDA kernel pass (fused hashing, so no
     * CPU hashing bottleneck). Falls back to the CPU HyperLogLog when CUDA is
     * unavailable.
     *
     * @note Compile with `-DHAS_CUDA` to enable GPU acceleration; requires the CUDA
     *       toolkit and nvcc.
     */

#ifdef HAS_CUDA

    /// Host-side CUDA entry point (defined in hll_gpu.cu). Uses a flat pointer
    /// boundary for MSVC/nvcc cross-compiler linking.
    extern "C" auto bigdata_hll_count_gpu(const std::uint64_t* keys, std::size_t num_keys,
                                          int p) -> double;

#endif  // HAS_CUDA

    /**
     * @brief Estimate the number of distinct keys.
     *
     * Both the GPU and CPU paths hash each key with the shared 64-bit hash, so the
     * estimates agree; the GPU path performs hashing and register folding in one
     * kernel.
     *
     * @param[in] keys Pointer to the keys (may be null when @p num_keys is zero).
     * @param[in] num_keys Number of keys.
     * @param[in] p HyperLogLog precision (4..16).
     * @return The cardinality estimate.
     */
    [[nodiscard]] inline auto hll_distinct_count(const std::uint64_t* keys, std::size_t num_keys,
                                                 int p = 14) -> double {
#ifdef HAS_CUDA
        return bigdata_hll_count_gpu(keys, num_keys, p);
#else
        HyperLogLog hll{p};
        for (std::size_t i = 0; i < num_keys; ++i) {
            hll.add(keys[i]);
        }
        return hll.count();
#endif
    }

    /**
     * @brief Estimate the number of distinct keys in a vector.
     * @param[in] keys The keys to count.
     * @param[in] p HyperLogLog precision (4..16).
     * @return The cardinality estimate.
     */
    [[nodiscard]] inline auto hll_distinct_count(const std::vector<std::uint64_t>& keys,
                                                 int p = 14) -> double {
        return hll_distinct_count(keys.data(), keys.size(), p);
    }

}  // namespace bigdata
