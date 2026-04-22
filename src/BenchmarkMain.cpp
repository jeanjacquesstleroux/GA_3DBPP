// BenchmarkMain.cpp — GA_3DBPPBenchmark entry point (Phase 8, Task 8.6)
//
// Usage:
//   GA_3DBPPBenchmark <thpack_file> [output.csv] [max_instances] [--relaxed]
//
// --relaxed  Skip the support constraint during Phase 2 placement.
//            Reports max_util_pct (most-filled single container) alongside the
//            standard avg_util_pct.  Use for comparison against Table 7 of the
//            paper (Ananno & Ribeiro 2024), which reports relaxed-mode results.
//
// Runs the full pipeline on every problem in a BR thpack file and prints a
// summary to stdout.  Optionally writes per-instance results to a CSV file.

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>

#include "BenchmarkRunner.h"
#include "Logger.h"

int main(int argc, char* argv[])
{
    initLogger(spdlog::level::warn);  // suppress info/debug noise during batch run

    if (argc < 2) {
        std::cerr << "Usage: GA_3DBPPBenchmark <thpack_file> [output.csv] [max_instances] [--relaxed]\n";
        return 1;
    }

    // Parse positional + optional arguments.
    // --relaxed may appear anywhere after the mandatory thpack_file argument.
    std::string br_path;
    std::string csv_path;
    int         max_inst = -1;
    bool        relaxed  = false;

    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--relaxed") {
            relaxed = true;
        } else if (positional == 0) {
            br_path  = arg;  ++positional;
        } else if (positional == 1) {
            csv_path = arg;  ++positional;
        } else if (positional == 2) {
            max_inst = std::atoi(argv[i]);  ++positional;
        }
    }

    if (br_path.empty()) {
        std::cerr << "Usage: GA_3DBPPBenchmark <thpack_file> [output.csv] [max_instances] [--relaxed]\n";
        return 1;
    }

    std::cout << "Benchmarking: " << br_path << '\n';
    if (!csv_path.empty())
        std::cout << "CSV output  : " << csv_path << '\n';
    if (max_inst > 0)
        std::cout << "Max inst    : " << max_inst << '\n';
    std::cout << "Mode        : " << (relaxed ? "RELAXED (no support check)" : "STRICT") << '\n';
    std::cout << std::string(70, '-') << '\n';

    const auto records = BenchmarkRunner::run(br_path, max_inst, /*seed=*/42, relaxed);

    if (records.empty()) {
        std::cerr << "No records produced — check that the BR file exists and is valid.\n";
        return 1;
    }

    // ── Per-instance table ────────────────────────────────────────────────────
    std::cout << std::left
              << std::setw(6)  << "Inst"
              << std::setw(10) << "AvgUtil%"
              << std::setw(10) << "MaxUtil%"
              << std::setw(10) << "Time(ms)"
              << std::setw(8)  << "Ctrs"
              << std::setw(10) << "Unplaced"
              << std::setw(10) << "AABB"
              << std::setw(10) << "Bounds"
              << "Support\n"
              << std::string(74, '-') << '\n';

    for (const BenchmarkRecord& r : records) {
        std::cout << std::setw(6)  << r.instance
                  << std::setw(10) << std::fixed << std::setprecision(1) << r.util_pct
                  << std::setw(10) << std::setprecision(1) << r.max_util_pct
                  << std::setw(10) << std::setprecision(1) << r.time_ms
                  << std::setw(8)  << r.containers
                  << std::setw(10) << r.unplaced
                  << std::setw(10) << r.aabb_viol
                  << std::setw(10) << r.bounds_viol
                  << r.support_viol << '\n';
    }

    // ── Aggregate summary ─────────────────────────────────────────────────────
    const int    N              = static_cast<int>(records.size());
    const double avg_util       = std::accumulate(records.begin(), records.end(), 0.0,
                                    [](double s, const BenchmarkRecord& r) { return s + r.util_pct; }) / N;
    const double avg_max_util   = std::accumulate(records.begin(), records.end(), 0.0,
                                    [](double s, const BenchmarkRecord& r) { return s + r.max_util_pct; }) / N;
    const double avg_time       = std::accumulate(records.begin(), records.end(), 0.0,
                                    [](double s, const BenchmarkRecord& r) { return s + r.time_ms; }) / N;
    const int    total_viol     = std::accumulate(records.begin(), records.end(), 0,
                                    [](int s, const BenchmarkRecord& r) {
                                        return s + r.aabb_viol + r.bounds_viol + r.support_viol;
                                    });
    const int    total_unpl     = std::accumulate(records.begin(), records.end(), 0,
                                    [](int s, const BenchmarkRecord& r) { return s + r.unplaced; });

    std::cout << std::string(74, '=') << '\n'
              << "Instances     : " << N           << '\n'
              << "Avg util      : " << std::fixed << std::setprecision(1) << avg_util << "%\n"
              << "Avg max-util  : " << std::setprecision(1) << avg_max_util << "%\n"
              << "Avg time      : " << std::setprecision(1) << avg_time << " ms\n"
              << "Total unplaced: " << total_unpl  << '\n'
              << "Total viol    : " << total_viol  << '\n';

    // ── Write CSV ─────────────────────────────────────────────────────────────
    if (!csv_path.empty()) {
        const auto dir = std::filesystem::path(csv_path).parent_path();
        if (!dir.empty()) std::filesystem::create_directories(dir);

        if (BenchmarkRunner::writeCSV(records, csv_path))
            std::cout << "CSV written : " << csv_path << '\n';
        else
            std::cerr << "Warning: could not write CSV to '" << csv_path << "'\n";
    }

    return 0;
}
