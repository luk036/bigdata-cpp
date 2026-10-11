[![Actions Status](https://github.com/luk036/bigdata-cpp/workflows/MacOS/badge.svg)](https://github.com/luk036/bigdata-cpp/actions)
[![Actions Status](https://github.com/luk036/bigdata-cpp/workflows/Windows/badge.svg)](https://github.com/luk036/bigdata-cpp/actions)
[![Actions Status](https://github.com/luk036/bigdata-cpp/workflows/Ubuntu/badge.svg)](https://github.com/luk036/bigdata-cpp/actions)
[![Actions Status](https://github.com/luk036/bigdata-cpp/workflows/Install/badge.svg)](https://github.com/luk036/bigdata-cpp/actions)
[![codecov](https://codecov.io/gh/luk036/bigdata-cpp/branch/master/graph/badge.svg)](https://codecov.io/gh/luk036/bigdata-cpp)

# 📊 bigdata-cpp

A header-only C++17 library of **streaming probabilistic data structures** for
electronic design automation: a **Count-Min Sketch** for event-frequency
estimation and **HyperLogLog** for cardinality estimation, sharing a single
64-bit hash so their estimates are comparable.

This is the C++ counterpart of the Python [`big_data`](https://github.com/luk036/big_data)
experiments.

## Features

- **Count-Min Sketch** with total-weight tracking, the classic `min` estimator,
  the bias-corrected **Count-Mean-Min** estimator, an opt-in
  **conservative-update** mode, an additive `e·N/width` error bound, and exact
  element-wise merging.
- **HyperLogLog** with the standard small/large-range corrections and
  associative/commutative merging.
- **Optional GPU acceleration**: a fused CUDA kernel
  (`bigdata/hyper_loglog_gpu.hpp` + `source/hll_gpu.cu`) hashes and folds all
  keys in a single device pass. Compile with `-DHAS_CUDA`; falls back to the CPU
  estimator otherwise.
- **One shared hash family** (`bigdata/hll_hash.hpp`) used by every sketch, so
  equal keys hash identically everywhere and sketches built in different
  translation units merge correctly.
- Header-only, dependency-free (standard library only), C++17.
- doctest test suite, Doxygen documentation, packaging via PackageProject.

## Usage

```cpp
#include <bigdata/count_min_sketch.hpp>
#include <bigdata/hyper_loglog.hpp>

bigdata::CountMinSketch sketch(1000, 5);
sketch.update(42);
sketch.update(42);
double count = sketch.query(42);          // ~2.0 (never underestimates)
double unbiased = sketch.query_mean_min(42);

bigdata::HyperLogLog hll(14);
for (std::uint64_t node = 0; node < 1'000'000; ++node) hll.add(node);
double distinct = hll.count();            // ~1e6
```

With CUDA available the same cardinality can be estimated on the GPU (hashing
included on the device):

```cpp
#include <bigdata/hyper_loglog_gpu.hpp>

std::vector<std::uint64_t> keys = /* ... */;
double distinct = bigdata::hll_distinct_count(keys, /*p=*/14);
```

#### Building with CUDA

When `CUDAToolkit` is found the fused kernel is compiled into the test binary
(`HAS_CUDA` defined); otherwise it falls back to the CPU estimator. Two host
toolchains are supported:

- **nvcc** (used when the C++ compiler is MSVC); and
- **clang**, which compiles CUDA directly and therefore avoids the nvcc /
  `cudafe++` coupling that breaks on very new MSVC releases.

clang emits SASS rather than portable PTX, so set the target architecture to your
GPU: `-DBIGDATA_CUDA_ARCH=sm_89` (RTX 40xx), `sm_86` (RTX 30xx), `sm_75`
(Turing). The default is `sm_75`.

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCUDAToolkit_ROOT=<cuda> -DBIGDATA_CUDA_ARCH=sm_89
cmake --build build
ctest --test-dir build --output-on-failure
```

### Build and run the standalone target

```bash
cmake -B build
cmake --build build
./build/BigData --events 1000000 --nodes 10000
```

### Build and run the test suite

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Consume the library with `find_package`

```cmake
find_package(BigData REQUIRED)
target_link_libraries(my_app PRIVATE BigData::BigData)
```

## Project layout

```
include/bigdata/     # Public headers: hll_hash.hpp, count_min_sketch.hpp, hyper_loglog.hpp, hyper_loglog_gpu.hpp
source/              # hll_gpu.cu (compiled by nvcc into the test binary when CUDA is available)
test/source/         # doctest tests
standalone/          # Example executable
documentation/       # Doxygen configuration
```

## License

MIT — see [LICENSE.txt](LICENSE.txt).
