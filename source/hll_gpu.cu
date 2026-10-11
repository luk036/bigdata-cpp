/**
 * @file hll_gpu.cu
 * @brief CUDA kernel and host-side launch for HyperLogLog cardinality estimation.
 *
 * Compiled only when HAS_CUDA is defined (by nvcc). Provides:
 *   - bigdata_hll_kernel: CUDA kernel that hashes each key and folds it into the
 *     registers (fused hashing + sketch, no separate CPU hashing pass)
 *   - bigdata_hll_count_gpu(): host-side function callable from C++ code
 */

#include <cuda_runtime.h>

#include <cmath>     // for ldexp, log
#include <cstddef>   // for size_t
#include <cstdint>   // for uint64_t, uint32_t
#include <cstdlib>   // for exit
#include <iostream>  // for cerr
#include <vector>    // for vector

namespace {

    /// MurmurHash3 64-bit finalizer (device copy of bigdata::fmix64).
    __device__ inline auto fmix64_device(std::uint64_t x) -> std::uint64_t {
        x ^= x >> 33U;
        x *= 0xFF51AFD7ED558CCDULL;
        x ^= x >> 33U;
        x *= 0xC4CEB9FE1A85EC53ULL;
        x ^= x >> 33U;
        return x;
    }

    /// Rank = leading zeros in the (64 - p)-bit field, plus one.
    __device__ inline auto rank_of_device(std::uint64_t remaining, int bits) -> unsigned int {
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

}  // namespace

// ============================================================================
// CUDA kernel -- each thread hashes one key and updates one register
// ============================================================================

extern "C" __global__ void bigdata_hll_kernel(const std::uint64_t* keys, std::size_t num_keys,
                                              int p, unsigned int* registers) {
    const auto idx = (static_cast<std::size_t>(blockIdx.x) * blockDim.x) + threadIdx.x;
    if (idx >= num_keys) {
        return;
    }

    const auto hash = fmix64_device(keys[idx]);
    const int bits = 64 - p;
    const auto reg_index = static_cast<std::size_t>(hash >> bits);
    const auto remaining = hash & ((std::uint64_t{1} << bits) - 1U);
    const auto rank = rank_of_device(remaining, bits);

    atomicMax(&registers[reg_index], rank);
}

// ============================================================================
// Host-side CUDA launch -- compiled by nvcc, callable from C++ code
// ============================================================================

#define BIGDATA_CUDA_CHECK(call)                                                                 \
    do {                                                                                         \
        cudaError_t err_ = call;                                                                 \
        if (err_ != cudaSuccess) {                                                               \
            std::cerr << "CUDA error: " << cudaGetErrorString(err_) << " at " << __FILE__ << ":" \
                      << __LINE__ << "\n";                                                       \
            std::exit(1);                                                                        \
        }                                                                                        \
    } while (0)

/// Estimate the number of distinct keys with a GPU HyperLogLog.
extern "C" auto bigdata_hll_count_gpu(const std::uint64_t* keys, std::size_t num_keys,
                                      int p) -> double {
    static constexpr int THREADS_PER_BLOCK = 256;
    static constexpr double TWO_POW_64 = 18446744073709551616.0;

    const auto num_registers = std::size_t{1} << p;

    unsigned int* d_registers = nullptr;
    BIGDATA_CUDA_CHECK(cudaMalloc(&d_registers, num_registers * sizeof(unsigned int)));
    BIGDATA_CUDA_CHECK(cudaMemset(d_registers, 0, num_registers * sizeof(unsigned int)));

    if (num_keys > 0) {
        std::uint64_t* d_keys = nullptr;
        BIGDATA_CUDA_CHECK(cudaMalloc(&d_keys, num_keys * sizeof(std::uint64_t)));
        BIGDATA_CUDA_CHECK(
            cudaMemcpy(d_keys, keys, num_keys * sizeof(std::uint64_t), cudaMemcpyHostToDevice));

        const auto blocks = (num_keys + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
        bigdata_hll_kernel<<<static_cast<unsigned int>(blocks), THREADS_PER_BLOCK>>>(
            d_keys, num_keys, p, d_registers);
        BIGDATA_CUDA_CHECK(cudaGetLastError());
        BIGDATA_CUDA_CHECK(cudaDeviceSynchronize());
        cudaFree(d_keys);
    }

    std::vector<unsigned int> registers(num_registers, 0U);
    BIGDATA_CUDA_CHECK(cudaMemcpy(registers.data(), d_registers,
                                  num_registers * sizeof(unsigned int), cudaMemcpyDeviceToHost));
    cudaFree(d_registers);

    double alpha = 0.0;
    if (num_registers == 16) {
        alpha = 0.673;
    } else if (num_registers == 32) {
        alpha = 0.697;
    } else if (num_registers == 64) {
        alpha = 0.709;
    } else {
        alpha = 0.7213 / (1.0 + 1.079 / static_cast<double>(num_registers));
    }

    double z = 0.0;
    std::size_t zeros = 0;
    for (const auto reg : registers) {
        z += std::ldexp(1.0, -static_cast<int>(reg));
        if (reg == 0) {
            ++zeros;
        }
    }
    double estimate
        = alpha * static_cast<double>(num_registers) * static_cast<double>(num_registers) / z;
    if (estimate <= 2.5 * static_cast<double>(num_registers)) {
        if (zeros != 0) {
            estimate = static_cast<double>(num_registers)
                       * std::log(static_cast<double>(num_registers) / static_cast<double>(zeros));
        }
    } else if (estimate > (1.0 / 30.0) * TWO_POW_64) {
        estimate = -TWO_POW_64 * std::log(1.0 - estimate / TWO_POW_64);
    }
    return estimate;
}

#undef BIGDATA_CUDA_CHECK
