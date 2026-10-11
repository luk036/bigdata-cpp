cmake_minimum_required(VERSION 3.14)
project(BigDataInstalledTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BigData REQUIRED)

add_executable(bigdata_installed_test main.cpp)
target_link_libraries(bigdata_installed_test PRIVATE BigData::BigData)

enable_testing()
add_test(NAME bigdata_installed_test COMMAND bigdata_installed_test)
