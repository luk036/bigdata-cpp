-- xmake build for bigdata-cpp: a header-only C++17 library of streaming sketches.
-- CPU-only by default. Enable the optional CUDA HLL kernel with: xmake f --cuda=y

set_project("BigData")
set_version("0.1.0")
set_languages("cxx17")
set_license("MIT")

add_rules("mode.debug", "mode.release")
set_defaultmode("release")

option("cuda")
    set_default(false)
    set_showmenu(true)
    set_description("Enable the optional CUDA HyperLogLog kernel (-DHAS_CUDA)")
option_end()

add_requires("doctest")
add_requires("cxxopts")

target("bigdata")
    set_kind("headeronly")
    add_headerfiles("include/(bigdata/*.hpp)")
    add_includedirs("include", {public = true})
target_end()

target("BigDataTests")
    set_kind("binary")
    add_deps("bigdata")
    add_packages("doctest")
    add_files("test/source/*.cpp")
    set_warnings("all", "extra")
    add_cxflags("/permissive-", "/utf-8", "/wd4819", {tools = {"cl", "clang_cl"}})
    if has_config("cuda") then
        add_rules("cuda")
        add_files("source/hll_gpu.cu")
        add_defines("HAS_CUDA")
    end
target_end()

target("BigData")
    set_kind("binary")
    add_deps("bigdata")
    add_packages("cxxopts")
    add_files("standalone/source/*.cpp")
    set_warnings("all", "extra")
    add_cxflags("/permissive-", "/utf-8", "/wd4819", {tools = {"cl", "clang_cl"}})
    on_load(function (target)
        local inc = path.join(target:autogendir(), "include")
        os.mkdir(path.join(inc, "bigdata"))
        io.writefile(
            path.join(inc, "bigdata", "version.h"),
            '#pragma once\n\n#define BIGDATA_VERSION "0.1.0"\n'
        )
        target:add("includedirs", inc)
    end)
target_end()
