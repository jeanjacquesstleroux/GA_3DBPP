// BenchmarkRunner.cpp — Phase 8, Tasks 8.2–8.6
//
// Runs the full GA-3DBPP pipeline on every problem in a BR thpack benchmark
// file.  For each instance the pipeline mirrors main.cpp:
//   Phase 1: LayerGenerator → filterByFillRate → mergeLayers → buildBlocks
//   Phase 2: NSGA2::run → extractParetoFront → selectBest → Packer::decode
//
// All Phase 1 functions receive the BR container's actual L/W/H so that the
// algorithm is not artificially constrained to the default Euro pallet size.

#include "BenchmarkRunner.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <vector>

#include "AABB.h"
#include "BlockBuilder.h"
#include "BRReader.h"
#include "LayerGenerator.h"
#include "NSGA2.h"
#include "Packer.h"
#include "SupportChecker.h"
#include "Types.h"

// ─── Internal helpers ────────────────────────────────────────────────────────

// Select the solution with the fewest containers; break ties on wasted volume;
// break further ties on auxiliary max single-container utilization (higher is
// better, so negate for ascending comparison).  The final tiebreaker matters
// for high-density BR instances where all feasible solutions have identical
// container count, avg-util, and total wasted volume — without it, selectBest
// would return an arbitrary individual from the Pareto front.
// Mirrors selectBest() in main.cpp.
static const Individual& selectBest(const std::vector<Individual>& front)
{
    return *std::min_element(front.begin(), front.end(),
        [](const Individual& a, const Individual& b) {
            if (a.objectives[0] != b.objectives[0])
                return a.objectives[0] < b.objectives[0];
            if (a.objectives[2] != b.objectives[2])
                return a.objectives[2] < b.objectives[2];
            // Final tiebreaker: prefer the individual that packs the most
            // volume into a single container (higher aux_max_util = better).
            return a.aux_max_util > b.aux_max_util;
        });
}

// Count pairs of items in the same container that share interior volume.
static int countAABBViolations(const PackingSolution& sol)
{
    int count = 0;
    for (const Container& c : sol.containers) {
        const auto& v = c.items;
        for (int i = 0; i < static_cast<int>(v.size()); ++i)
            for (int j = i + 1; j < static_cast<int>(v.size()); ++j)
                if (AABB::overlaps(v[i], v[j])) ++count;
    }
    return count;
}

// Count items that extend outside their container's walls.
static int countBoundsViolations(const PackingSolution& sol)
{
    int count = 0;
    for (const Container& c : sol.containers)
        for (const PlacedItem& pi : c.items)
            if (!AABB::fitsInContainer(pi, c)) ++count;
    return count;
}

// Count items above the floor that fail all three support tiers.
// Uses the default inset (10 mm) to match the placement-time check in
// placeItem().  Phase 1 items (≥90% fill layers) pass trivially because
// the 10 mm inset keeps test vertices away from the 2 mm dynamic-shifting
// gaps at item edges.  Phase 2 items were placed with this same inset, so
// the audit and placement are consistent.
static int countSupportViolations(const PackingSolution& sol)
{
    SupportChecker checker;
    int count = 0;
    for (const Container& c : sol.containers) {
        for (int i = 0; i < static_cast<int>(c.items.size()); ++i) {
            const PlacedItem& pi = c.items[i];
            if (pi.z == 0) continue;  // floor-resting: always supported
            std::vector<PlacedItem> others;
            others.reserve(c.items.size() - 1);
            for (int j = 0; j < static_cast<int>(c.items.size()); ++j)
                if (j != i) others.push_back(c.items[j]);
            if (!checker.isSupported(pi, others)) ++count;
        }
    }
    return count;
}

// Run the full pipeline on one BRProblem and return the PackingSolution.
static PackingSolution solveProblem(const BRProblem& prob,
                                    unsigned seed,
                                    int& out_unplaced,
                                    bool relaxed)
{
    const int PL = prob.L, PW = prob.W, PH = prob.H;

    // ── Phase 1 ─────────────────────────────────────────────────────────────
    std::vector<Layer> all_layers;
    all_layers.reserve(prob.items.size() * 7);
    for (int i = 0; i < static_cast<int>(prob.items.size()); ++i) {
        all_layers.push_back(LayerGenerator::generateFull(prob.items[i], i, PL, PW));
        for (Layer& l : LayerGenerator::generateHalves(prob.items[i], i, PL, PW))
            all_layers.push_back(std::move(l));
        for (Layer& l : LayerGenerator::generateQuarters(prob.items[i], i, PL, PW))
            all_layers.push_back(std::move(l));
    }
    BlockBuilder::filterByFillRate(all_layers);

    // Helper: create a properly-sized container for this BR problem.
    auto makeContainer = [PL, PW, PH]() {
        Container c;
        c.L = PL; c.W = PW; c.H = PH;
        return c;
    };

    std::vector<Container> containers;
    std::vector<int>       residualTypes;
    std::vector<int>       residualCounts(static_cast<int>(prob.items.size()), 0);

    if (all_layers.empty()) {
        // Fully heterogeneous order — Phase 1 skipped.
        for (int i = 0; i < static_cast<int>(prob.items.size()); ++i) {
            residualTypes.push_back(i);
            residualCounts[i] = prob.items[i].q;
        }
        containers.push_back(makeContainer());
    } else {
        auto merged  = BlockBuilder::mergeLayers(std::move(all_layers), PL, PW);
        containers   = BlockBuilder::buildBlocks(merged, prob.items, PL, PW, PH);
        auto resInfo = BlockBuilder::computeResiduals(prob.items, containers, PL, PW, PH);
        if (resInfo.spawn_new_pallet)
            containers.push_back(makeContainer());
        for (const auto& [type_idx, count] : resInfo.residuals) {
            residualTypes.push_back(type_idx);
            residualCounts[type_idx] = count;
        }
    }

    // ── Phase 2 ─────────────────────────────────────────────────────────────
    if (residualTypes.empty()) {
        PackingSolution sol;
        sol.containers = containers;
        out_unplaced   = 0;
        return sol;
    }

    std::mt19937 rng{seed};
    auto pop   = NSGA2::run(residualTypes, residualCounts, prob.items, containers, rng, relaxed);
    auto front = NSGA2::extractParetoFront(pop);

    const Individual& best = selectBest(front);
    return Packer::decode(best.chromosome, residualCounts, prob.items,
                          containers, out_unplaced, relaxed);
}

// ─── Public API ──────────────────────────────────────────────────────────────

std::vector<BenchmarkRecord> BenchmarkRunner::run(
    const std::string& br_path,
    int      max_instances,
    unsigned seed,
    bool     relaxed)
{
    const auto problems = loadBRFile(br_path);
    std::vector<BenchmarkRecord> records;

    const int n = (max_instances < 0 || max_instances > static_cast<int>(problems.size()))
                  ? static_cast<int>(problems.size())
                  : max_instances;
    records.reserve(n);

    for (int inst = 0; inst < n; ++inst) {
        const BRProblem& prob = problems[inst];
        const auto t0 = std::chrono::steady_clock::now();

        int unplaced = 0;
        const PackingSolution sol = solveProblem(prob, seed + static_cast<unsigned>(inst),
                                                  unplaced, relaxed);

        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Compute per-container utilization to find the most-filled one.
        double max_util = 0.0;
        for (const Container& c : sol.containers) {
            const double cap = static_cast<double>(c.L) * c.W * c.H;
            if (cap <= 0.0) continue;
            double used = 0.0;
            for (const PlacedItem& pi : c.items)
                used += static_cast<double>(pi.dx) * pi.dy * pi.dz;
            const double u = used / cap;
            if (u > max_util) max_util = u;
        }

        BenchmarkRecord rec;
        rec.instance     = inst + 1;  // 1-based
        rec.util_pct     = sol.avgUtilization() * 100.0;
        rec.max_util_pct = max_util * 100.0;
        rec.time_ms      = ms;
        rec.containers   = static_cast<int>(sol.containers.size());
        rec.unplaced     = unplaced;
        rec.aabb_viol    = countAABBViolations(sol);
        rec.bounds_viol  = countBoundsViolations(sol);
        rec.support_viol = countSupportViolations(sol);

        records.push_back(rec);
    }

    return records;
}

bool BenchmarkRunner::writeCSV(const std::vector<BenchmarkRecord>& records,
                                const std::string& out_path)
{
    std::ofstream f(out_path);
    if (!f.is_open()) return false;

    f << "instance,util_pct,max_util_pct,time_ms,containers,unplaced,"
         "aabb_violations,bounds_violations,support_violations\n";

    for (const BenchmarkRecord& r : records) {
        f << r.instance     << ','
          << r.util_pct     << ','
          << r.max_util_pct << ','
          << r.time_ms      << ','
          << r.containers   << ','
          << r.unplaced     << ','
          << r.aabb_viol    << ','
          << r.bounds_viol  << ','
          << r.support_viol << '\n';
    }

    return f.good();
}
