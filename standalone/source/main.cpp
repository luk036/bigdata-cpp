#include <bigdata/count_min_sketch.hpp>
#include <bigdata/hyper_loglog.hpp>
#include <bigdata/version.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

#include <cxxopts.hpp>

auto main(int argc, char** argv) -> int {
    cxxopts::Options options("BigData", "Streaming sketches (Count-Min Sketch, HyperLogLog) for EDA");
    options.add_options()("h,help", "Print usage")("v,version", "Print version")(
        "n,nodes", "Number of distinct nodes",
        cxxopts::value<std::uint64_t>()->default_value("10000")
    )("e,events", "Number of toggle events",
      cxxopts::value<std::uint64_t>()->default_value("1000000"))(
        "p,precision", "HyperLogLog precision (4..16)", cxxopts::value<int>()->default_value("14")
    )("width", "Count-Min Sketch width",
      cxxopts::value<std::uint64_t>()->default_value("1000"))(
        "depth", "Count-Min Sketch depth", cxxopts::value<std::uint64_t>()->default_value("5")
    );

    const auto result = options.parse(argc, argv);
    if (result.count("help") > 0) {
        std::cout << options.help() << '\n';
        return 0;
    }
    if (result.count("version") > 0) {
        std::cout << "BigData, version " << BIGDATA_VERSION << '\n';
        return 0;
    }

    const auto num_nodes = result["nodes"].as<std::uint64_t>();
    const auto num_events = result["events"].as<std::uint64_t>();
    const auto precision = result["precision"].as<int>();
    const auto width = result["width"].as<std::uint64_t>();
    const auto depth = result["depth"].as<std::uint64_t>();

    if (num_nodes == 0) {
        std::cerr << "error: --nodes must be positive\n";
        return 1;
    }

    std::mt19937_64 rng{42};
    std::uniform_int_distribution<std::uint64_t> node_dist{0, num_nodes - 1};

    bigdata::CountMinSketch sketch{static_cast<std::size_t>(width), static_cast<std::size_t>(depth)};
    bigdata::HyperLogLog hll{precision};

    for (std::uint64_t i = 0; i < num_events; ++i) {
        const auto node = node_dist(rng);
        sketch.update(node);
        hll.add(node);
    }

    const auto memory_kib =
        static_cast<double>(width * depth * sizeof(double)) / 1024.0;
    const auto hll_kib = static_cast<double>((std::size_t{1} << precision)) / 1024.0;

    std::cout << "BigData demo (version " << BIGDATA_VERSION << ")\n";
    std::cout << "  events:            " << num_events << '\n';
    std::cout << "  distinct nodes:    " << num_nodes << '\n';
    std::cout << "  Count-Min Sketch:  width=" << width << " depth=" << depth << " memory="
              << memory_kib << " KiB  total=" << sketch.total() << '\n';
    std::cout << "  HyperLogLog:       p=" << precision << " memory=" << hll_kib
              << " KiB  estimate=" << hll.count() << '\n';
    return 0;
}
