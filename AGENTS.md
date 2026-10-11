# AGENTS.md - Agentic Coding Guidelines for bigdata-cpp

This file provides context and guidelines for AI agents operating in this codebase.

## Project Overview

bigdata-cpp is a header-only C++17 library of streaming probabilistic sketches for EDA:

- **CountMinSketch** - event-frequency estimation (with Count-Mean-Min and conservative update)
- **HyperLogLog** - cardinality estimation
- **hll_hash** - the single shared 64-bit hash family used by both sketches
- **hyper_loglog_gpu** - optional fused CUDA kernel (`source/hll_gpu.cu`, compiled with `-DHAS_CUDA`)

There is no compiled library; everything lives in `include/bigdata/*.hpp`. The only `.cu` source
(`source/hll_gpu.cu`) is compiled by nvcc into the test binary when `CUDAToolkit` is found and the
host compiler is MSVC (on Windows).

## Build Commands

### Standard Build (CMake)

```bash
cmake -B build
cmake --build build
```

### Running Tests

```bash
ctest --test-dir build --output-on-failure

# or directly:
./build/BigDataTests
./build/BigDataTests -tc="CountMinSketch merge is exact, associative and commutative"
```

### Run Standalone

```bash
cmake -B build
cmake --build build
./build/BigData --help
```

### Code Formatting

```bash
cmake --build build --target format      # check
cmake --build build --target fix-format  # apply
```

Requires: `clang-format`, `cmake-format`, `pyyaml`.

## Code Style Guidelines

### C++ Standard
- **C++17** required
- MSVC: `/permissive-` (standards conformance)

### Formatting (clang-format)
- **BasedOnStyle**: Google, ColumnLimit: 100, IndentWidth: 4
- **BreakBeforeBraces**: Attach, **NamespaceIndentation**: All

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes/Structs | PascalCase | `CountMinSketch`, `HyperLogLog` |
| Functions | snake_case | `hash_bytes`, `query_mean_min` |
| Variables | snake_case | `num_modules` |
| Member vars | trailing underscore | `width_`, `registers_` |
| Constants | UPPER_SNAKE | `MODULUS`, `TWO_POW_64` |
| Enums | PascalCase | `UpdateMode` |
| Files | snake_case | `count_min_sketch.hpp` |

### Header Conventions
- `#pragma once`
- Doxygen `@brief`/`@param`/`@return` comments on every public declaration
- `[[nodiscard]]` on query functions
- `constexpr`/`noexcept` where possible

### Error Handling
- **Preferred**: `std::invalid_argument` for programmer errors (bad dimensions/precision)
- No error suppression; no `reinterpret_cast` hacks

## Key Dependencies

| Library | Purpose |
|---------|---------|
| doctest | Testing |
| CPM.cmake | Reproducible dependencies |
| cxxopts | standalone CLI parsing |
| PackageProject | Installable CMake package |

## GPU / CUDA notes

When `CUDAToolkit` is found, `source/hll_gpu.cu` is compiled into `BigDataTests` with `HAS_CUDA`.
Two host toolchains work: **nvcc** (with MSVC) and **clang** (compiles CUDA directly, avoiding the
nvcc/`cudafe++` incompatibility with the newest MSVC). With clang, set the target GPU arch via
`-DBIGDATA_CUDA_ARCH=sm_XX` (clang emits SASS, not portable PTX). MinGW (`GNU`) disables CUDA
because nvcc rejects it as a host.

## Common Tasks

- **Add header**: `include/bigdata/*.hpp` (header-only - no `.cpp` needed)
- **Add test**: `test/source/test_*.cpp` using doctest macros
- **Format before commit** - run `fix-format`
- **Test after changes** - `ctest --test-dir build --output-on-failure`
