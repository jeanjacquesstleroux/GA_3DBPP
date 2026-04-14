#pragma once

#include <string>
#include <vector>

// Per-instance result produced by BenchmarkRunner::run.
struct BenchmarkRecord {
    int    instance     = 0;    // 1-based index within the BR file
    double util_pct     = 0.0;  // average volume utilisation across containers (%)
    double time_ms      = 0.0;  // wall-clock time for this instance (ms)
    int    containers   = 0;    // number of containers used
    int    unplaced     = 0;    // items that could not be placed anywhere
    int    aabb_viol    = 0;    // pairs of items that share interior volume
    int    bounds_viol  = 0;    // items that extend outside their container walls
    int    support_viol = 0;    // items above the floor that fail all support tiers
};

namespace BenchmarkRunner {

// Run at most `max_instances` problems from a BR thpack file.
// Pass max_instances = -1 (default) to process every problem in the file.
// `seed` is forwarded to the NSGA-II RNG for reproducibility.
// Returns one BenchmarkRecord per problem processed.
[[nodiscard]] std::vector<BenchmarkRecord> run(
    const std::string& br_path,
    int      max_instances = -1,
    unsigned seed          = 42);

// Write `records` to a CSV file at `out_path`.
// The CSV has a header row followed by one data row per record.
// Returns true on success, false if the file could not be opened.
[[nodiscard]] bool writeCSV(const std::vector<BenchmarkRecord>& records,
                             const std::string& out_path);

} // namespace BenchmarkRunner
