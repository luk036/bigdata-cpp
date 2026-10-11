cmake_minimum_required(VERSION 3.14...3.22)

project(
  BigData
  VERSION 0.1.0
  LANGUAGES CXX
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---------------------------------------------------------------------------
# Compiler warnings (as errors)
# ---------------------------------------------------------------------------

function(target_enable_warnings TARGET)
  if(MSVC)
    target_compile_options(${TARGET} PRIVATE /W4 /WX /utf-8 /wd4819)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(BIGDATA_BUILD_TESTS "Build tests" ON)
option(BIGDATA_BUILD_STANDALONE "Build standalone executable" ON)
option(BIGDATA_BUILD_DOCS "Build Doxygen documentation" OFF)
option(BIGDATA_ENABLE_CLANG_TIDY "Enable clang-tidy analysis" OFF)
option(BIGDATA_ENABLE_COVERAGE "Enable coverage reporting" OFF)

# ---------------------------------------------------------------------------
# CUDA (optional)
# ---------------------------------------------------------------------------

find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND AND WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # nvcc rejects MinGW as a host on Windows; clang and MSVC are supported.
  message(STATUS "CUDA found but the host compiler is GNU (MinGW): disabling GPU acceleration")
  set(HAS_CUDA OFF)
elseif(CUDAToolkit_FOUND)
  message(STATUS "CUDA detected: enabling GPU acceleration")
  set(HAS_CUDA ON)
else()
  message(STATUS "CUDA not found: CPU-only mode")
  set(HAS_CUDA OFF)
endif()

# GPU architecture for the clang CUDA build (clang emits SASS, not portable PTX, so this must match
# the target GPU: e.g. sm_75 Turing, sm_86 RTX 30xx, sm_89 RTX 40xx).
set(BIGDATA_CUDA_ARCH
    "sm_75"
    CACHE STRING "CUDA GPU architecture for the clang CUDA build"
)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# Download CPM.cmake for reproducible dependency management see
# https://github.com/cpm-cmake/CPM.cmake for more info
set(CPM_DOWNLOAD_VERSION 0.38.5)
set(CPM_HASH_SUM "192aa0ccdc57dfe75bd9e4b176bf7fb5692fd2b3e3f7b09c74856fc39572b31c")
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD
       https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
       ${CPM_DOWNLOAD_LOCATION} EXPECTED_HASH SHA256=${CPM_HASH_SUM}
  )
endif()
include(${CPM_DOWNLOAD_LOCATION})

# PackageProject.cmake will be used to make our target installable
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.8.0")

# ---------------------------------------------------------------------------
# Library target (header-only)
# ---------------------------------------------------------------------------

# BigData is a header-only library, so it is an INTERFACE target: consumers get the include
# directory (and C++17) transitively and nothing needs to be compiled into the library.
add_library(${PROJECT_NAME} INTERFACE)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME}
                       INTERFACE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->"
)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

target_include_directories(
  ${PROJECT_NAME}
  INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Create an installable target ----
# this allows users to install and find the library via `find_package()`.

# the location where the project's version header will be placed should match the project's regular
# header paths
string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)

packageProject(
  NAME ${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  NAMESPACE ${PROJECT_NAME}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"
  COMPATIBILITY SameMajorVersion
)

# ---------------------------------------------------------------------------
# Standalone executable
# ---------------------------------------------------------------------------

if(BIGDATA_BUILD_STANDALONE)
  # cxxopts is a header-only CLI parser used by the standalone example. Prefer a consumer- or
  # system-provided cxxopts; only fall back to CPM when no cxxopts target is available.
  find_package(cxxopts CONFIG QUIET)
  if(NOT cxxopts_FOUND AND NOT TARGET cxxopts::cxxopts)
    CPMAddPackage(
      GITHUB_REPOSITORY jarro2783/cxxopts
      VERSION 3.2.1
      OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL NO"
    )
  endif()
  # keep third-party headers out of our -Werror scope
  if(TARGET cxxopts::cxxopts)
    get_target_property(CXXOPTS_INCLUDE_DIRS cxxopts::cxxopts INTERFACE_INCLUDE_DIRECTORIES)
  endif()

  file(GLOB BIGDATA_STANDALONE_SOURCES CONFIGURE_DEPENDS "standalone/source/*.cpp")
  add_executable(BigDataStandalone ${BIGDATA_STANDALONE_SOURCES})
  set_target_properties(BigDataStandalone PROPERTIES OUTPUT_NAME "BigData" CXX_STANDARD 17)
  target_include_directories(BigDataStandalone SYSTEM PRIVATE ${CXXOPTS_INCLUDE_DIRS})
  target_link_libraries(BigDataStandalone PRIVATE BigData::BigData cxxopts::cxxopts)
  target_enable_warnings(BigDataStandalone)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

if(BIGDATA_BUILD_TESTS)
  # Try system-installed doctest first, otherwise fetch it.
  find_package(doctest CONFIG QUIET)

  if(NOT doctest_FOUND)
    CPMAddPackage(
      NAME doctest
      GITHUB_REPOSITORY doctest/doctest
      VERSION 2.5.2
      OPTIONS "DOCTEST_NO_INSTALL ON" # keep the installed prefix minimal
    )
  endif()

  # Code formatting targets (format / fix-format) via Format.cmake
  CPMAddPackage("gh:TheLartians/Format.cmake@1.7.3")

  file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "test/source/*.cpp")
  add_executable(BigDataTests ${TEST_SOURCES})
  target_link_libraries(BigDataTests PRIVATE doctest::doctest BigData::BigData)
  set_target_properties(BigDataTests PROPERTIES CXX_STANDARD 17)
  target_enable_warnings(BigDataTests)

  # ---- CUDA GPU object (compiled by the host compiler or nvcc, linked into the tests) ----
  if(HAS_CUDA)
    set(CUDA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/hll_gpu.cu")

    if(DEFINED CUDAToolkit_TARGET_DIR)
      set(BIGDATA_CUDA_ROOT "${CUDAToolkit_TARGET_DIR}")
    else()
      set(BIGDATA_CUDA_ROOT "${CUDAToolkit_ROOT}")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      # clang compiles CUDA directly (no nvcc / cudafe++), so it avoids the
      # nvcc-vs-MSV C coupling that breaks on very new toolchains.
      set(CUDA_OBJ "${CMAKE_CURRENT_BINARY_DIR}/hll_gpu_clang.obj")
      add_custom_command(
        OUTPUT ${CUDA_OBJ}
        COMMAND
          ${CMAKE_CXX_COMPILER} -x cuda --cuda-path=${BIGDATA_CUDA_ROOT}
          --cuda-gpu-arch=${BIGDATA_CUDA_ARCH} --std=c++17 -fms-runtime-lib=dll
          -I${CMAKE_CURRENT_SOURCE_DIR}/include -DHAS_CUDA -O2 -c ${CUDA_SRC} -o ${CUDA_OBJ}
        DEPENDS ${CUDA_SRC}
        COMMENT "Compiling hll_gpu.cu with clang (CUDA)"
        VERBATIM
      )
    else()
      set(CUDA_OBJ "${CMAKE_CURRENT_BINARY_DIR}/hll_gpu.obj")
      set(NVCC_FLAGS
          --compile
          --gpu-architecture=compute_75
          --std=c++17
          --use-local-env
          -ccbin
          "${CMAKE_CXX_COMPILER}"
          -I"${CMAKE_CURRENT_SOURCE_DIR}/include"
          -I"${CUDAToolkit_INCLUDE_DIRS}"
          -DHAS_CUDA
          -O2
          -o
          "${CUDA_OBJ}"
      )

      if(MSVC)
        list(
          APPEND
          NVCC_FLAGS
          -allow-unsupported-compiler
          -Xcompiler="/EHsc"
          -Xcompiler="/wd4819"
          -Xcompiler="/O2"
          "-Xcompiler=/MD$<$<CONFIG:Debug>:d>"
        )
      endif()

      add_custom_command(
        OUTPUT ${CUDA_OBJ}
        COMMAND ${CUDAToolkit_BIN_DIR}/nvcc ${NVCC_FLAGS} "${CUDA_SRC}"
        DEPENDS ${CUDA_SRC}
        COMMENT "Compiling hll_gpu.cu with nvcc"
        VERBATIM
      )
    endif()

    target_sources(BigDataTests PRIVATE ${CUDA_OBJ})
    target_compile_definitions(BigDataTests PRIVATE HAS_CUDA)
    target_link_libraries(BigDataTests PRIVATE CUDA::cudart)
  endif()

  add_test(NAME BigDataAllTests COMMAND BigDataTests)
endif()

# ---------------------------------------------------------------------------
# clang-tidy
# ---------------------------------------------------------------------------

if(BIGDATA_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY clang-tidy)
  if(CLANG_TIDY)
    add_custom_target(
      clang-tidy
      COMMAND ${CLANG_TIDY} --extra-arg=-std=c++17 --extra-arg=-I${CMAKE_CURRENT_SOURCE_DIR}/include
              ${CMAKE_CURRENT_SOURCE_DIR}/include/bigdata/*.hpp
      COMMENT "Running clang-tidy static analysis"
    )
  else()
    message(WARNING "clang-tidy not found - analysis target unavailable")
  endif()
endif()

# ---------------------------------------------------------------------------
# Doxygen documentation
# ---------------------------------------------------------------------------

if(BIGDATA_BUILD_DOCS)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_PROJECT_NAME ${PROJECT_NAME})
    set(DOXYGEN_PROJECT_VERSION ${PROJECT_VERSION})
    set(DOXYGEN_PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen")

    configure_file(documentation/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    add_custom_target(
      GenerateDocs
      COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      COMMAND echo "Docs written to: ${DOXYGEN_OUTPUT_DIRECTORY}/html"
      COMMENT "Generating Doxygen HTML documentation in ${DOXYGEN_OUTPUT_DIRECTORY}/html"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
  else()
    add_custom_target(
      GenerateDocs COMMAND ${CMAKE_COMMAND} -E echo
                           "Doxygen not found - skipping documentation generation"
    )
  endif()
endif()

# ---------------------------------------------------------------------------
# Coverage report target (GCC/Clang only - requires gcovr)
# ---------------------------------------------------------------------------

if(BIGDATA_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  foreach(COVERAGE_TARGET BigDataTests BigDataStandalone)
    if(TARGET ${COVERAGE_TARGET})
      target_compile_options(${COVERAGE_TARGET} PRIVATE -O0 -g -fprofile-arcs -ftest-coverage)
    endif()
  endforeach()
  foreach(COVERAGE_EXEC BigDataTests BigDataStandalone)
    if(TARGET ${COVERAGE_EXEC})
      target_link_options(${COVERAGE_EXEC} PRIVATE -fprofile-arcs -ftest-coverage)
    endif()
  endforeach()

  find_program(GCOVR_PATH gcovr)
  if(GCOVR_PATH)
    set(extra_gcovr_args "")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
      find_program(LLVM_COV_PATH llvm-cov)
      if(LLVM_COV_PATH)
        list(APPEND extra_gcovr_args --llvm-cov-binary ${LLVM_COV_PATH})
      endif()
    endif()
    add_custom_target(
      coverage
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND
        ${GCOVR_PATH} --root ${PROJECT_SOURCE_DIR} --html-details
        ${CMAKE_BINARY_DIR}/coverage/index.html --exclude test/ --exclude build/ ${extra_gcovr_args}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating coverage report in ${CMAKE_BINARY_DIR}/coverage/index.html"
    )
  else()
    message(
      WARNING "gcovr not found - coverage target unavailable. Install with: pip install gcovr"
    )
  endif()
endif()
