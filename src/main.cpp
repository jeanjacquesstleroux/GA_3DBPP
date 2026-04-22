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
#include <map>
#include <random>

#include "AnimatedSolution.h"
#include "BlockBuilder.h"
#include "BRReader.h"
#include "CSVReader.h"
#include "JSONWriter.h"
#include "LayerGenerator.h"
#include "Logger.h"
#include "NSGA2.h"
#include "Packer.h"
#include "Types.h"

// ─── Task 2 (animated): Build AnimatedContainer from a Phase 1 container ─────
//
// Converts every PlacedItem in `src` to an AnimatedPlacedItem tagged with
// phase=1, a sequential placement_order, and a layer_index computed by the
// z-band grouping algorithm from the plan (§1.3).
// Also builds the layer_manifest from the resulting band assignments.
// Only called when --animated-output is active; has no effect on normal runs.
static AnimatedContainer buildAnimatedPhase1Container(const Container& src)
{
    AnimatedContainer ac;
    ac.L = src.L;
    ac.W = src.W;
    ac.H = src.H;

    // Copy PlacedItem fields and assign placement_order; layer_index filled below.
    int order = 0;
    for (const PlacedItem& p : src.items) {
        AnimatedPlacedItem ap;
        ap.item_type_index = p.item_type_index;
        ap.orientation     = p.orientation;
        ap.x  = p.x;  ap.y  = p.y;  ap.z  = p.z;
        ap.dx = p.dx; ap.dy = p.dy; ap.dz = p.dz;
        ap.phase           = 1;
        ap.placement_order = order++;
        // layer_index stays -1 until the band pass below overwrites it.
        ac.items.push_back(ap);
    }

    // ── Layer assignment (plan §1.3) ──────────────────────────────────────────
    // Each "band" tracks the z-range [z_min, z_max] of one discovered layer.
    // An item joins the first band whose z_max (+ 1 mm tolerance) covers the
    // item's top; otherwise it starts a new band.
    struct Band { int z_min; int z_max; };
    constexpr int TOL = 1;   // mm — handles minor alignment gaps
    std::vector<Band> bands;

    for (AnimatedPlacedItem& ap : ac.items) {
        const int z_top = ap.z + ap.dz;
        bool matched = false;
        for (int i = 0; i < static_cast<int>(bands.size()); ++i) {
            if (ap.z >= bands[i].z_min && z_top <= bands[i].z_max + TOL) {
                ap.layer_index = i;
                bands[i].z_max = std::max(bands[i].z_max, z_top);
                matched = true;
                break;
            }
        }
        if (!matched) {
            ap.layer_index = static_cast<int>(bands.size());
            bands.push_back({ap.z, z_top});
        }
    }

    // ── Build layer_manifest from band assignments ────────────────────────────
    const int n_layers = static_cast<int>(bands.size());
    ac.layer_manifest.resize(n_layers);
    for (int i = 0; i < n_layers; ++i) {
        ac.layer_manifest[i].layer_index = i;
        ac.layer_manifest[i].z_min       = bands[i].z_min;
        ac.layer_manifest[i].z_max       = bands[i].z_max;
    }

    // Accumulate per-layer item counts and type breakdowns.
    std::vector<std::map<int, int>> type_counts(n_layers);
    for (const AnimatedPlacedItem& ap : ac.items) {
        ++ac.layer_manifest[ap.layer_index].item_count;
        ++type_counts[ap.layer_index][ap.item_type_index];
    }
    for (int i = 0; i < n_layers; ++i) {
        for (const auto& [type_idx, cnt] : type_counts[i]) {
            ac.layer_manifest[i].item_type_summary.push_back({type_idx, cnt});
        }
    }

    return ac;
}

// ─── Task 7.3: Solution selection from Pareto front ──────────────────────────
//
// Paper: select the solution with the fewest containers; break ties by choosing
// the one with the least total wasted volume (highest compactness).
// Final tiebreaker: aux_max_util (highest single-container fill) handles the
// degenerate case where all objectives are equal — common for high-density BR
// instances where every chromosome produces the same container count and all
// items are placed.
// objectives[0] = container count (minimize)
// objectives[2] = total wasted volume in mm^3 (minimize)
// aux_max_util  = max single-container utilization (maximize as tiebreaker)
static const Individual& selectBest(const std::vector<Individual>& front)
{
    return *std::min_element(front.begin(), front.end(),
        [](const Individual& a, const Individual& b) {
            if (a.objectives[0] != b.objectives[0])
                return a.objectives[0] < b.objectives[0];
            if (a.objectives[2] != b.objectives[2])
                return a.objectives[2] < b.objectives[2];
            return a.aux_max_util > b.aux_max_util;
        });
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // ── Task 7.4: Logger initialisation ─────────────────────────────────────
    initLogger(spdlog::level::info);
    auto log = getLogger();

    if (argc < 2) {
        log->error("Usage: GA_3DBPP <input> [output.json] [--animated-output]");
        return 1;
    }
    const std::string input_path  = argv[1];
    const std::string output_path = (argc >= 3 && std::string(argv[2]) != "--animated-output")
                                        ? argv[2]
                                        : "output/solution.json";

    // Scan remaining argv for --animated-output (no-op in Task 1; wired up for
    // later tasks that will populate and write AnimatedSolution).
    bool animated_output = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--animated-output") {
            animated_output = true;
        }
    }
    if (animated_output) log->info("Animated output mode enabled");

    // Create output directory if it does not exist.
    const auto out_dir = std::filesystem::path(output_path).parent_path();
    if (!out_dir.empty()) std::filesystem::create_directories(out_dir);

    // ── Read input — CSV or BR benchmark format ──────────────────────────────
    // BR benchmark files use a .txt extension; everything else is treated as CSV.
    log->info("Reading '{}'", input_path);

    std::string          order_id  = "instance_1";
    std::vector<ItemType> itemTypes;
    Container             br_container_proto;   // used only for BR inputs

    const bool is_br = input_path.ends_with(".txt");
    if (is_br) {
        const auto problems = loadBRFile(input_path);
        if (problems.empty()) {
            log->error("No valid problems in '{}'", input_path);
            return 1;
        }
        const BRProblem& prob = problems[0];   // animated demo uses instance 1
        itemTypes             = prob.items;
        br_container_proto.L  = prob.L;
        br_container_proto.W  = prob.W;
        br_container_proto.H  = prob.H;
        log->info("BR format — instance 1: container {}×{}×{}, {} item type(s)",
                  prob.L, prob.W, prob.H, prob.items.size());
    } else {
        const auto orders = CSVReader(input_path).read();
        if (orders.empty()) {
            log->error("No valid orders in '{}'", input_path);
            return 1;
        }
        const auto& entry = *orders.begin();
        order_id  = entry.first;
        itemTypes = entry.second;
    }
    log->info("Order {} — {} item type(s), {} total items",
              order_id,
              itemTypes.size(),
              [&]{ int n = 0; for (const auto& it : itemTypes) n += it.q; return n; }());

    const auto t0 = std::chrono::steady_clock::now();

    AnimatedSolution anim_sol;   // populated incrementally under --animated-output

    // ── Phase 1: Layer generation → filter → merge → block building ──────────
    // Generate candidate layers for every item type across all three footprint
    // sizes (full, half, quarter).  FilterByFillRate removes any layer that
    // does not meet the 90 %/90 %/85 % coverage thresholds from the paper.

    // For BR inputs, spawn new pallets with the problem's container dimensions.
    // For CSV inputs the default Config Euro-pallet dimensions apply.
    // Defined here (before layer generation) so generators use the correct dims.
    const Container container_proto = is_br ? br_container_proto : Container{};

    std::vector<Layer> all_layers;
    all_layers.reserve(itemTypes.size() * 7);  // 1 full + 4 halves + 2 quarters each
    for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
        all_layers.push_back(LayerGenerator::generateFull(itemTypes[i], i,
                                                          container_proto.L,
                                                          container_proto.W));
        for (Layer& lyr : LayerGenerator::generateHalves(itemTypes[i], i,
                                                          container_proto.L,
                                                          container_proto.W))
            all_layers.push_back(std::move(lyr));
        for (Layer& lyr : LayerGenerator::generateQuarters(itemTypes[i], i,
                                                            container_proto.L,
                                                            container_proto.W))
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
        containers.push_back(container_proto);
    } else {
        auto merged  = BlockBuilder::mergeLayers(std::move(all_layers),
                                                   container_proto.L,
                                                   container_proto.W);
        containers   = BlockBuilder::buildBlocks(merged, itemTypes,
                                                   container_proto.L,
                                                   container_proto.W,
                                                   container_proto.H);
        log->info("Phase 1: {} container(s) used for blocks", containers.size());

        // ── Task 2 (animated): instrument Phase 1 containers ─────────────────
        if (animated_output) {
            for (const Container& c : containers) {
                anim_sol.containers.push_back(buildAnimatedPhase1Container(c));
                anim_sol.phase1_item_count += static_cast<int>(c.items.size());
            }
            // Diagnostic: print first 10 items of container 0 for Task 2 test.
            if (!anim_sol.containers.empty()) {
                const auto& c0    = anim_sol.containers[0];
                const int   limit = std::min(10, static_cast<int>(c0.items.size()));
                log->info("Animated Phase 1 — container 0, first {} item(s):", limit);
                for (int i = 0; i < limit; ++i) {
                    const auto& ap = c0.items[i];
                    log->info("  [{}] order={} layer={} z={} dz={}",
                              i, ap.placement_order, ap.layer_index, ap.z, ap.dz);
                }
            }
        }

        const auto resInfo = BlockBuilder::computeResiduals(itemTypes, containers,
                                                              container_proto.L,
                                                              container_proto.W,
                                                              container_proto.H);
        if (resInfo.spawn_new_pallet) {
            containers.push_back(container_proto);
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
    std::vector<GASnapshot> ga_snapshots;   // populated by GA when --animated-output

    if (residualTypes.empty()) {
        // All items packed in Phase 1 — GA adds no value here.
        log->info("All items packed in Phase 1, skipping GA");
        best_solution.containers = containers;
    } else {
        log->info("Phase 2: GA on {} residual type(s) across {} seed container(s)",
                  residualTypes.size(), containers.size());

        std::mt19937 rng{std::random_device{}()};

        // Under --animated-output, collect per-generation snapshots from the GA.
        const auto pop = NSGA2::run(
            residualTypes, residualCounts, itemTypes, containers, rng,
            /*relaxed=*/false,
            animated_output ? &ga_snapshots : nullptr);
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

        // ── Task 3 (animated): snapshot Phase 1 item counts before decode ────
        // Packer::decode appends Phase 2 items into the same containers vector.
        // We record how many items each container already has so we can
        // identify which items are newly added by the decoder after it returns.
        std::vector<int> phase1_counts;
        if (animated_output) {
            phase1_counts.reserve(containers.size());
            for (const Container& c : containers) {
                phase1_counts.push_back(static_cast<int>(c.items.size()));
            }
        }

        int unplaced = 0;
        best_solution = Packer::decode(
            best.chromosome, residualCounts, itemTypes, containers, unplaced);

        if (unplaced > 0)
            log->warn("{} item(s) could not be placed — item may exceed pallet size",
                      unplaced);

        // ── Task 3 (animated): tag Phase 2 items in anim_sol ─────────────────
        if (animated_output) {
            // Ensure anim_sol.containers covers every container in best_solution.
            // Containers that already existed get their Phase 2 items appended.
            // New containers (opened by the GA for residuals) start fresh.
            while (anim_sol.containers.size() < best_solution.containers.size()) {
                const Container& src = best_solution.containers[anim_sol.containers.size()];
                AnimatedContainer ac;
                ac.L = src.L; ac.W = src.W; ac.H = src.H;
                anim_sol.containers.push_back(std::move(ac));
                phase1_counts.push_back(0);   // no Phase 1 items in this new container
            }

            for (int ci = 0; ci < static_cast<int>(best_solution.containers.size()); ++ci) {
                const Container&   src  = best_solution.containers[ci];
                AnimatedContainer& adst = anim_sol.containers[ci];
                const int          p1n  = phase1_counts[ci];   // items already tagged

                // placement_order continues from where Phase 1 left off.
                int order = p1n;
                for (int ii = p1n; ii < static_cast<int>(src.items.size()); ++ii) {
                    const PlacedItem& p = src.items[ii];
                    AnimatedPlacedItem ap;
                    ap.item_type_index = p.item_type_index;
                    ap.orientation     = p.orientation;
                    ap.x  = p.x;  ap.y  = p.y;  ap.z  = p.z;
                    ap.dx = p.dx; ap.dy = p.dy; ap.dz = p.dz;
                    ap.phase           = 2;
                    ap.placement_order = order++;
                    ap.layer_index     = -1;   // Phase 2 items have no layer
                    adst.items.push_back(ap);
                    ++anim_sol.phase2_item_count;
                }
            }

            anim_sol.total_items = anim_sol.phase1_item_count + anim_sol.phase2_item_count;
            log->info("Animated Phase 2 — {} item(s) tagged across {} container(s)",
                      anim_sol.phase2_item_count,
                      best_solution.containers.size());
        }
    }

    // ── Task 4 (animated): convert GASnapshot → GAGenerationSnapshot ──────────
    if (animated_output) {
        for (const GASnapshot& snap : ga_snapshots) {
            GAGenerationSnapshot gsnap;
            gsnap.generation           = snap.generation;
            gsnap.best_container_count = snap.best_container_count;
            gsnap.best_avg_utilization = snap.best_avg_utilization;

            // Convert each Container in the snapshot to an AnimatedContainer.
            // layer_manifest is left empty — ga_history snapshots don't need it.
            for (const Container& c : snap.solution.containers) {
                AnimatedContainer ac;
                ac.L = c.L; ac.W = c.W; ac.H = c.H;
                int order = 0;
                for (const PlacedItem& p : c.items) {
                    AnimatedPlacedItem ap;
                    ap.item_type_index = p.item_type_index;
                    ap.orientation     = p.orientation;
                    ap.x  = p.x;  ap.y  = p.y;  ap.z  = p.z;
                    ap.dx = p.dx; ap.dy = p.dy; ap.dz = p.dz;
                    ap.phase           = 2;
                    ap.placement_order = order++;
                    ap.layer_index     = -1;
                    ac.items.push_back(ap);
                }
                gsnap.containers.push_back(std::move(ac));
            }
            anim_sol.ga_history.push_back(std::move(gsnap));
        }

        anim_sol.total_items = anim_sol.phase1_item_count + anim_sol.phase2_item_count;
        log->info("Animated GA history — {} snapshot(s) recorded", anim_sol.ga_history.size());
        if (!anim_sol.ga_history.empty()) {
            const auto& first = anim_sol.ga_history.front();
            log->info("  gen 0: {} container(s), avg_util={:.1f}%",
                      first.best_container_count,
                      first.best_avg_utilization * 100.0);
        }
    }

    // ── Output ────────────────────────────────────────────────────────────────
    const auto   t1    = std::chrono::steady_clock::now();
    const double secs  = std::chrono::duration<double>(t1 - t0).count();

    if (!writeJSON(best_solution, itemTypes, output_path)) {
        log->error("Failed to write JSON to '{}'", output_path);
        return 1;
    }
    log->info("Solution written to '{}' ({:.2f}s)", output_path, secs);

    if (animated_output) {
        const std::string anim_path =
            std::filesystem::path(output_path).parent_path() / "solution_animated.json";
        if (!writeAnimatedJSON(anim_sol, itemTypes, anim_path)) {
            log->error("Failed to write animated JSON to '{}'", anim_path);
            return 1;
        }
        log->info("Animated solution written to '{}'", anim_path);
    }

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
