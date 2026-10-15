/**
 * @file hll_gpu.cu
 * @brief CUDA kernel and host-side launch for HyperLogLog cardinality estimation.
 *
 * Compiled only when HAS_CUDA is defined (by nvcc). Provides:
 *   - bigdata_hll_kernel: CUDA kernel that hashes each key and folds it into the
 *     registers (fused hashing + sketch, no separate CPU hashing pass)
 *   - bigdata_hll_count_gpu(): host-side function callable from C++ code
 *
 * The shared hash finalizer, rank computation and estimator live in
 * <bigdata/hll_core.hpp>, so the GPU and CPU paths cannot drift apart.
 */

#include <cuda_runtime.h>

#include <bigdata/hll_core.hpp>

#include <cstddef>   // for size_t
#include <cstdint>   // for uint64_t, uint32_t
#include <cstdlib>   // for exit
#include <iostream>  // for cerr
#include <vector>    // for vector

// ============================================================================
// CUDA kernel -- each thread hashes one key and updates one register
// ============================================================================

extern "C" __global__ void bigdata_hll_kernel(const std::uint64_t* keys, std::size_t num_keys,
                                              int p, unsigned int* registers) {
    const auto idx = (static_cast<std::size_t>(blockIdx.x) * blockDim.x) + threadIdx.x;
    if (idx >= num_keys) {
        return;
    }

    const auto hash = bigdata::fmix64(keys[idx]);
    const int bits = 64 - p;
    const auto reg_index = static_cast<std::size_t>(hash >> bits);
    const auto remaining = hash & ((std::uint64_t{1} << bits) - 1U);
    const auto rank = bigdata::detail::rank_of(remaining, bits);

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

    return bigdata::detail::estimate_from_registers(registers.begin(), registers.end(), p);
}

#undef BIGDATA_CUDA_CHECK
