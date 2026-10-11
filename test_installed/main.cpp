#include <bigdata/count_min_sketch.hpp>
#include <bigdata/hyper_loglog.hpp>
#include <bigdata/version.h>

#include <iostream>

auto main() -> int {
    bigdata::CountMinSketch sketch{64, 3};
    sketch.update(7);
    bigdata::HyperLogLog hll{12};
    hll.add(1);

    std::cout << "bigdata installed test: version " << BIGDATA_VERSION << "\n";
    const bool ok = (BIGDATA_VERSION_MAJOR >= 0) && (sketch.query(7) >= 1.0) && (hll.count() > 0.0);
    return ok ? 0 : 1;
}
