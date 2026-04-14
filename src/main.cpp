// GA-3DBPP: Multi-Heuristic 3-D Bin Packing
// Ananno & Ribeiro (2024), IEEE Access Vol. 12
//
// Usage:
//   GA_3DBPP <input.csv> [output.json]
//
// Processes the first order found in the CSV.  Phase 1 builds homogeneous
// layers and stacks them into blocks; Phase 2 runs NSGA-II to place any
// remaining residual items via Extreme Points.  The best Pareto-front solution
// is written to output.json for Three.js visualisation.

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>

#include "BlockBuilder.h"
#include "CSVReader.h"
#include "JSONWriter.h"
#include "LayerGenerator.h"
#include "Logger.h"
#include "NSGA2.h"
#include "Packer.h"
#include "Types.h"

// ─── Task 7.3: Solution selection from Pareto front ──────────────────────────
//
// Paper: select the solution with the fewest containers; break ties by choosing
// the one with the least total wasted volume (highest compactness).
// objectives[0] = container count (minimize)
// objectives[2] = total wasted volume in mm^3 (minimize)
static const Individual& selectBest(const std::vector<Individual>& front)
{
    return *std::min_element(front.begin(), front.end(),
        [](const Individual& a, const Individual& b) {
            if (a.objectives[0] != b.objectives[0])
                return a.objectives[0] < b.objectives[0];
            return a.objectives[2] < b.objectives[2];
        });
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // ── Task 7.4: Logger initialisation ─────────────────────────────────────
    initLogger(spdlog::level::info);
    auto log = getLogger();

    if (argc < 2) {
        log->error("Usage: GA_3DBPP <input.csv> [output.json]");
        return 1;
    }
    const std::string input_path  = argv[1];
    const std::string output_path = (argc >= 3) ? argv[2] : "output/solution.json";

    // Create output directory if it does not exist.
    const auto out_dir = std::filesystem::path(output_path).parent_path();
    if (!out_dir.empty()) std::filesystem::create_directories(out_dir);

    // ── Read CSV ─────────────────────────────────────────────────────────────
    log->info("Reading '{}'", input_path);
    const auto orders = CSVReader(input_path).read();
    if (orders.empty()) {
        log->error("No valid orders in '{}'", input_path);
        return 1;
    }

    // Phase 7 processes the first order.  Multi-order batching is Phase 8.
    const auto& [order_id, itemTypes] = *orders.begin();
    log->info("Order {} — {} item type(s), {} total items",
              order_id,
              itemTypes.size(),
              [&]{ int n = 0; for (const auto& it : itemTypes) n += it.q; return n; }());

    const auto t0 = std::chrono::steady_clock::now();

    // ── Phase 1: Layer generation → filter → merge → block building ──────────
    // Generate candidate layers for every item type across all three footprint
    // sizes (full, half, quarter).  FilterByFillRate removes any layer that
    // does not meet the 90 %/90 %/85 % coverage thresholds from the paper.

    std::vector<Layer> all_layers;
    all_layers.reserve(itemTypes.size() * 7);  // 1 full + 4 halves + 2 quarters each
    for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
        all_layers.push_back(LayerGenerator::generateFull(itemTypes[i], i));
        for (Layer& lyr : LayerGenerator::generateHalves(itemTypes[i], i))
            all_layers.push_back(std::move(lyr));
        for (Layer& lyr : LayerGenerator::generateQuarters(itemTypes[i], i))
            all_layers.push_back(std::move(lyr));
    }
    BlockBuilder::filterByFillRate(all_layers);
    log->info("Phase 1: {} layer candidate(s) pass fill-rate filter",
              all_layers.size());

    std::vector<Container> containers;
    std::vector<int>       residualTypes;
    std::vector<int>       residualCounts(static_cast<int>(itemTypes.size()), 0);

    if (all_layers.empty()) {
        // Fully heterogeneous order — no item type can form a useful layer.
        // Every item is a residual; spawn one empty pallet as the GA's starting
        // point (the flowchart's ALL_RESIDUAL → VOL_CHECK → spawn branch).
        log->info("Phase 1 skipped — fully heterogeneous order, all items residual");
        for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
            residualTypes.push_back(i);
            residualCounts[i] = itemTypes[i].q;
        }
        containers.push_back(Container{});
    } else {
        auto merged  = BlockBuilder::mergeLayers(std::move(all_layers));
        containers   = BlockBuilder::buildBlocks(merged, itemTypes);
        log->info("Phase 1: {} container(s) used for blocks", containers.size());

        const auto resInfo = BlockBuilder::computeResiduals(itemTypes, containers);
        if (resInfo.spawn_new_pallet) {
            containers.push_back(Container{});
            log->info("Spawned one additional empty pallet for residuals");
        }
        for (const auto& [type_idx, count] : resInfo.residuals) {
            residualTypes.push_back(type_idx);
            residualCounts[type_idx] = count;
        }
        log->info("Phase 1: {} residual item type(s)", residualTypes.size());
    }

    // ── Phase 2: NSGA-II genetic algorithm ───────────────────────────────────
    PackingSolution best_solution;

    if (residualTypes.empty()) {
        // All items packed in Phase 1 — GA adds no value here.
        log->info("All items packed in Phase 1, skipping GA");
        best_solution.containers = containers;
    } else {
        log->info("Phase 2: GA on {} residual type(s) across {} seed container(s)",
                  residualTypes.size(), containers.size());

        std::mt19937 rng{std::random_device{}()};
        const auto pop   = NSGA2::run(residualTypes, residualCounts, itemTypes, containers, rng);
        const auto front = NSGA2::extractParetoFront(pop);

        if (front.empty()) {
            log->error("GA produced an empty Pareto front — cannot select solution");
            return 1;
        }
        log->info("GA complete — Pareto front: {} solution(s)", front.size());

        // ── Task 7.3: select best from front ────────────────────────────────
        const Individual& best = selectBest(front);
        log->info("Selected: {:.0f} container(s), utilization {:.1f}%, "
                  "wasted {:.0f} mm^3",
                  best.objectives[0],
                  -best.objectives[1] * 100.0,
                  best.objectives[2]);

        int unplaced = 0;
        best_solution = Packer::decode(
            best.chromosome, residualCounts, itemTypes, containers, unplaced);

        if (unplaced > 0)
            log->warn("{} item(s) could not be placed — item may exceed pallet size",
                      unplaced);
    }

    // ── Output ────────────────────────────────────────────────────────────────
    const auto   t1    = std::chrono::steady_clock::now();
    const double secs  = std::chrono::duration<double>(t1 - t0).count();

    if (!writeJSON(best_solution, itemTypes, output_path)) {
        log->error("Failed to write JSON to '{}'", output_path);
        return 1;
    }
    log->info("Solution written to '{}' ({:.2f}s)", output_path, secs);

    // ── Task 7.4: Human-readable summary ─────────────────────────────────────
    std::cout << "\n=== GA-3DBPP Result ===\n";
    std::cout << "Order           : " << order_id << "\n";
    std::cout << "Containers used : " << best_solution.containers.size() << "\n";
    std::cout << std::fixed << std::setprecision(1)
              << "Avg utilization : " << best_solution.avgUtilization() * 100.0 << "%\n";
    std::cout << std::setprecision(2)
              << "Elapsed         : " << secs << "s\n";
    std::cout << "Output          : " << output_path << "\n\n";

    return 0;
}
