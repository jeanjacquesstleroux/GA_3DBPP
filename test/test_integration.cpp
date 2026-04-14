// test_integration.cpp — Phase 7 & 8 end-to-end pipeline tests.
//
// Phase 7 tests exercise the full Phase 1 + GA + solution-selection pipeline
// using small in-memory datasets so the suite stays fast and deterministic.
//
// Phase 8 tests extend coverage to:
//   - Support constraint (Constraint 4) via SupportChecker
//   - Edge cases: oversize items, tiny orders
//   - BR benchmark instance validation (thpack1, instance 0)

#include <gtest/gtest.h>
#include <algorithm>
#include <random>
#include <vector>

#include "AABB.h"
#include "BlockBuilder.h"
#include "BRReader.h"
#include "CenterOfMass.h"
#include "LayerGenerator.h"
#include "NSGA2.h"
#include "Packer.h"
#include "SupportChecker.h"
#include "Types.h"

// ─── Shared pipeline helper ───────────────────────────────────────────────────
//
// Runs Phase 1 (layer generation → filter → merge → buildBlocks), then the
// NSGA-II GA on any residuals, then decodes the best Pareto-front solution.
// Mirrors the logic in main.cpp so all integration tests share one path.

struct PipelineResult {
    PackingSolution solution;
    int             unplaced = 0;
};

// Solution selection: fewest containers (primary), least wasted volume (secondary).
// Replicates the selectBest() helper from main.cpp.
static const Individual& selectBest(const std::vector<Individual>& front)
{
    return *std::min_element(front.begin(), front.end(),
        [](const Individual& a, const Individual& b) {
            if (a.objectives[0] != b.objectives[0])
                return a.objectives[0] < b.objectives[0];
            return a.objectives[2] < b.objectives[2];
        });
}

static PipelineResult runPipeline(const std::vector<ItemType>& itemTypes,
                                   uint32_t seed = 42)
{
    // ── Phase 1 ────────────────────────────────────────────────────────────
    std::vector<Layer> all_layers;
    for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
        all_layers.push_back(LayerGenerator::generateFull(itemTypes[i], i));
        for (Layer& lyr : LayerGenerator::generateHalves(itemTypes[i], i))
            all_layers.push_back(std::move(lyr));
        for (Layer& lyr : LayerGenerator::generateQuarters(itemTypes[i], i))
            all_layers.push_back(std::move(lyr));
    }
    BlockBuilder::filterByFillRate(all_layers);

    std::vector<Container> containers;
    std::vector<int>       residualTypes;
    std::vector<int>       residualCounts(static_cast<int>(itemTypes.size()), 0);

    if (all_layers.empty()) {
        // Fully heterogeneous: Phase 1 skipped, all items go to GA.
        for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
            residualTypes.push_back(i);
            residualCounts[i] = itemTypes[i].q;
        }
        containers.push_back(Container{});
    } else {
        auto merged  = BlockBuilder::mergeLayers(std::move(all_layers));
        containers   = BlockBuilder::buildBlocks(merged, itemTypes);
        auto resInfo = BlockBuilder::computeResiduals(itemTypes, containers);
        if (resInfo.spawn_new_pallet)
            containers.push_back(Container{});
        for (const auto& [type_idx, count] : resInfo.residuals) {
            residualTypes.push_back(type_idx);
            residualCounts[type_idx] = count;
        }
    }

    // ── Phase 2 (if residuals exist) ──────────────────────────────────────
    PipelineResult result;

    if (residualTypes.empty()) {
        result.solution.containers = containers;
        return result;
    }

    std::mt19937 rng{seed};
    auto pop   = NSGA2::run(residualTypes, residualCounts, itemTypes, containers, rng);
    auto front = NSGA2::extractParetoFront(pop);

    const Individual& best = selectBest(front);
    result.solution = Packer::decode(
        best.chromosome, residualCounts, itemTypes, containers, result.unplaced);
    return result;
}

// Convenience: total items placed across all containers.
static int totalPlaced(const PackingSolution& sol)
{
    int n = 0;
    for (const Container& c : sol.containers) n += static_cast<int>(c.items.size());
    return n;
}

// Convenience: total items expected from the order.
static int totalExpected(const std::vector<ItemType>& itemTypes)
{
    int n = 0;
    for (const ItemType& it : itemTypes) n += it.q;
    return n;
}

// ─── Constraint helpers (Constraints 1, 2, 4 from the paper) ────────────────

// Constraint 2: no two items in the same container share interior volume.
static bool noOverlaps(const PackingSolution& sol)
{
    for (const Container& c : sol.containers) {
        const auto& v = c.items;
        for (int i = 0; i < static_cast<int>(v.size()); ++i)
            for (int j = i + 1; j < static_cast<int>(v.size()); ++j)
                if (AABB::overlaps(v[i], v[j])) return false;
    }
    return true;
}

// Constraint 1: every placed item lies within its container's walls.
static bool allInBounds(const PackingSolution& sol)
{
    for (const Container& c : sol.containers)
        for (const PlacedItem& pi : c.items)
            if (!AABB::fitsInContainer(pi, c)) return false;
    return true;
}

// Constraint 4: every item above the pallet floor has adequate physical support.
// Items resting directly on the floor (z == 0) are always supported.
static bool allSupported(const PackingSolution& sol)
{
    SupportChecker checker;
    for (const Container& c : sol.containers) {
        for (int i = 0; i < static_cast<int>(c.items.size()); ++i) {
            const PlacedItem& pi = c.items[i];
            if (pi.z == 0) continue;  // floor-resting: unconditionally supported
            std::vector<PlacedItem> others;
            others.reserve(c.items.size() - 1);
            for (int j = 0; j < static_cast<int>(c.items.size()); ++j)
                if (j != i) others.push_back(c.items[j]);
            if (!checker.isSupported(pi, others)) return false;
        }
    }
    return true;
}

// ─── Custom-dims pipeline helper (for BR benchmark instances) ────────────────
//
// Identical to runPipeline but accepts explicit pallet dimensions so that
// BR benchmark problems (587×233×220 mm) can be run without modifying Config.h.

struct BRPipelineResult {
    PackingSolution solution;
    int             unplaced = 0;
};

static const Individual& selectBestLocal(const std::vector<Individual>& front)
{
    return *std::min_element(front.begin(), front.end(),
        [](const Individual& a, const Individual& b) {
            if (a.objectives[0] != b.objectives[0])
                return a.objectives[0] < b.objectives[0];
            return a.objectives[2] < b.objectives[2];
        });
}

static BRPipelineResult runPipelineDims(const std::vector<ItemType>& itemTypes,
                                         int PL, int PW, int PH,
                                         uint32_t seed = 42)
{
    std::vector<Layer> all_layers;
    for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
        all_layers.push_back(LayerGenerator::generateFull(itemTypes[i], i, PL, PW));
        for (Layer& lyr : LayerGenerator::generateHalves(itemTypes[i], i, PL, PW))
            all_layers.push_back(std::move(lyr));
        for (Layer& lyr : LayerGenerator::generateQuarters(itemTypes[i], i, PL, PW))
            all_layers.push_back(std::move(lyr));
    }
    BlockBuilder::filterByFillRate(all_layers);

    // Helper: spawn a container with BR dims instead of Config defaults.
    auto makeContainer = [PL, PW, PH]() {
        Container c;
        c.L = PL; c.W = PW; c.H = PH;
        return c;
    };

    std::vector<Container> containers;
    std::vector<int>       residualTypes;
    std::vector<int>       residualCounts(static_cast<int>(itemTypes.size()), 0);

    if (all_layers.empty()) {
        for (int i = 0; i < static_cast<int>(itemTypes.size()); ++i) {
            residualTypes.push_back(i);
            residualCounts[i] = itemTypes[i].q;
        }
        containers.push_back(makeContainer());
    } else {
        auto merged  = BlockBuilder::mergeLayers(std::move(all_layers), PL, PW);
        containers   = BlockBuilder::buildBlocks(merged, itemTypes, PL, PW, PH);
        auto resInfo = BlockBuilder::computeResiduals(itemTypes, containers, PL, PW, PH);
        if (resInfo.spawn_new_pallet)
            containers.push_back(makeContainer());
        for (const auto& [type_idx, count] : resInfo.residuals) {
            residualTypes.push_back(type_idx);
            residualCounts[type_idx] = count;
        }
    }

    BRPipelineResult result;
    if (residualTypes.empty()) {
        result.solution.containers = containers;
        return result;
    }

    std::mt19937 rng{seed};
    auto pop   = NSGA2::run(residualTypes, residualCounts, itemTypes, containers, rng);
    auto front = NSGA2::extractParetoFront(pop);

    const Individual& best = selectBestLocal(front);
    result.solution = Packer::decode(
        best.chromosome, residualCounts, itemTypes, containers, result.unplaced);
    return result;
}

// ─── Test 1: Homogeneous order ────────────────────────────────────────────────
// One item type with enough quantity to fill layers.  Phase 1 packs the bulk;
// GA handles any residual.  Total placed must equal total ordered.

TEST(FullPipeline, HomogeneousOrderAllItemsPlaced)
{
    ItemType item;
    item.l = 300; item.w = 200; item.h = 200; item.m = 5; item.q = 12;
    const std::vector<ItemType> items = {item};

    const auto result = runPipeline(items);

    EXPECT_EQ(result.unplaced, 0);
    EXPECT_EQ(totalPlaced(result.solution), totalExpected(items));
}

// ─── Test 2: Heterogeneous order (Phase 1 skipped) ───────────────────────────
// Three unique item types, q=1 each — no type has sufficient quantity to form
// a layer.  Phase 1 is skipped; GA must pack all three via EP placement.

TEST(FullPipeline, HeterogeneousOrderAllItemsPlaced)
{
    std::vector<ItemType> items(3);
    items[0].l = 100; items[0].w = 100; items[0].h = 100; items[0].m = 1; items[0].q = 1;
    items[1].l = 200; items[1].w = 150; items[1].h =  80; items[1].m = 2; items[1].q = 1;
    items[2].l = 150; items[2].w = 100; items[2].h = 120; items[2].m = 1; items[2].q = 1;

    const auto result = runPipeline(items);

    EXPECT_EQ(result.unplaced, 0);
    EXPECT_EQ(totalPlaced(result.solution), totalExpected(items));
}

// ─── Test 3: Mixed order (Phase 1 + GA) ──────────────────────────────────────
// Two item types: one has enough quantity to form layers (Phase 1), the other
// is a singleton residual for the GA.

TEST(FullPipeline, MixedOrderAllItemsPlaced)
{
    std::vector<ItemType> items(2);
    // Type 0: enough to fill layers
    items[0].l = 300; items[0].w = 200; items[0].h = 150; items[0].m = 3; items[0].q = 8;
    // Type 1: single item, goes to GA
    items[1].l = 250; items[1].w = 180; items[1].h = 120; items[1].m = 2; items[1].q = 1;

    const auto result = runPipeline(items);

    EXPECT_EQ(result.unplaced, 0);
    EXPECT_EQ(totalPlaced(result.solution), totalExpected(items));
}

// ─── Test 4: Constraint 2 — no AABB violations anywhere ──────────────────────

TEST(FullPipeline, NoAABBViolationsInSolution)
{
    std::vector<ItemType> items(2);
    items[0].l = 200; items[0].w = 200; items[0].h = 150; items[0].m = 3; items[0].q = 4;
    items[1].l = 300; items[1].w = 200; items[1].h = 100; items[1].m = 2; items[1].q = 3;

    const auto result = runPipeline(items);

    EXPECT_TRUE(noOverlaps(result.solution))
        << "Two or more placed items share interior volume";
}

// ─── Test 5: Constraint 1 — all items within container bounds ────────────────

TEST(FullPipeline, AllItemsWithinContainerBounds)
{
    std::vector<ItemType> items(2);
    items[0].l = 200; items[0].w = 200; items[0].h = 150; items[0].m = 3; items[0].q = 4;
    items[1].l = 300; items[1].w = 200; items[1].h = 100; items[1].m = 2; items[1].q = 3;

    const auto result = runPipeline(items);

    EXPECT_TRUE(allInBounds(result.solution))
        << "One or more placed items extend outside their container's walls";
}

// ─── Test 6: Single item type, single item ───────────────────────────────────
// Edge case: order with q=1 of one type.  Phase 1 cannot form a layer (q=1
// is rarely enough for 90% fill rate on a 1200×800 pallet).  GA places it.

TEST(FullPipeline, SingleItemPlacedSuccessfully)
{
    ItemType item;
    item.l = 400; item.w = 300; item.h = 200; item.m = 5; item.q = 1;
    const std::vector<ItemType> items = {item};

    const auto result = runPipeline(items);

    EXPECT_EQ(result.unplaced, 0);
    EXPECT_EQ(totalPlaced(result.solution), 1);
}

// ─── Test 7: Solution fits in one container for a tiny order ─────────────────
// Two small items — together they consume a tiny fraction of one pallet.
// The selected solution must use exactly 1 container.

TEST(FullPipeline, TinyOrderUsesOneContainer)
{
    std::vector<ItemType> items(2);
    items[0].l = 100; items[0].w = 100; items[0].h = 100; items[0].m = 1; items[0].q = 1;
    items[1].l = 150; items[1].w = 100; items[1].h = 100; items[1].m = 1; items[1].q = 1;

    const auto result = runPipeline(items);

    EXPECT_EQ(result.unplaced, 0);
    EXPECT_EQ(static_cast<int>(result.solution.containers.size()), 1);
}

// ─── Test 8: selectBest prefers fewer containers ──────────────────────────────
// Unit test for the Pareto selection heuristic: a 1-container solution must
// always beat a 2-container solution regardless of utilization or waste.

TEST(SelectionTest, PrefersFewerContainers)
{
    std::vector<Individual> front(2);
    front[0].objectives = {1.0, -0.7, 500000.0};   // 1 container, higher waste
    front[1].objectives = {2.0, -0.9,  50000.0};   // 2 containers, lower waste

    const Individual& best = selectBest(front);
    EXPECT_DOUBLE_EQ(best.objectives[0], 1.0);
}

// ─── Test 9: selectBest breaks ties on wasted volume ─────────────────────────

TEST(SelectionTest, TiebreaksOnWastedVolume)
{
    std::vector<Individual> front(2);
    front[0].objectives = {2.0, -0.7, 400000.0};   // same container count, more waste
    front[1].objectives = {2.0, -0.8, 100000.0};   // same container count, less waste

    const Individual& best = selectBest(front);
    EXPECT_DOUBLE_EQ(best.objectives[2], 100000.0);
}

// ─── Test 10: Both constraint checks pass on homogeneous order ────────────────
// Combines Tests 4 & 5 for a homogeneous order to confirm Phase 1 output is
// also free from violations.

TEST(FullPipeline, HomogeneousOrderSatisfiesConstraints)
{
    ItemType item;
    item.l = 300; item.w = 200; item.h = 200; item.m = 5; item.q = 8;
    const std::vector<ItemType> items = {item};

    const auto result = runPipeline(items);

    EXPECT_TRUE(noOverlaps(result.solution));
    EXPECT_TRUE(allInBounds(result.solution));
}

// ════════════════════════════════════════════════════════════════════════════
// Phase 8 tests — Task 8.1 (support), Task 8.4 (edge cases),
//                 Task 8.5 (BR benchmark constraint audit)
// ════════════════════════════════════════════════════════════════════════════

// ─── Test 11: Support constraint on mixed order ───────────────────────────────
// Every item placed above the pallet floor must satisfy at least one of the
// three support tiers.  Phase 1 layers produce flat stacks (always supported);
// Phase 2 EP placement checks support before committing each item.

TEST(FullPipeline, MixedOrderSupportSatisfied)
{
    std::vector<ItemType> items(2);
    items[0].l = 300; items[0].w = 200; items[0].h = 150; items[0].m = 3; items[0].q = 8;
    items[1].l = 250; items[1].w = 180; items[1].h = 120; items[1].m = 2; items[1].q = 1;

    const auto result = runPipeline(items);

    EXPECT_TRUE(allSupported(result.solution))
        << "One or more placed items lack adequate support from below";
}

// ─── Test 12: Full 4-constraint audit ────────────────────────────────────────
// Runs the mixed order and checks AABB, bounds, and support simultaneously.

TEST(FullPipeline, FullConstraintAuditMixedOrder)
{
    std::vector<ItemType> items(2);
    items[0].l = 200; items[0].w = 200; items[0].h = 150; items[0].m = 3; items[0].q = 4;
    items[1].l = 300; items[1].w = 200; items[1].h = 100; items[1].m = 2; items[1].q = 3;

    const auto result = runPipeline(items);

    EXPECT_TRUE(noOverlaps(result.solution))    << "AABB overlap detected";
    EXPECT_TRUE(allInBounds(result.solution))   << "Item outside container walls";
    EXPECT_TRUE(allSupported(result.solution))  << "Item lacks physical support";
}

// ─── Test 13: Oversize item is handled gracefully ─────────────────────────────
// An item larger than the pallet in both horizontal dimensions cannot be placed
// in any orientation.  The pipeline must not crash; it must report unplaced > 0.

TEST(FullPipeline, OversizeItemIsUnplaced)
{
    ItemType item;
    item.l = 1500; item.w = 1500; item.h = 200; item.m = 1; item.q = 1;
    const std::vector<ItemType> items = {item};

    const auto result = runPipeline(items);

    EXPECT_GT(result.unplaced, 0)
        << "Expected the oversize item to be reported as unplaced";
}

// ─── Tests 14–15: BR benchmark instance validation ────────────────────────────
// Loads the first problem from thpack1.txt (3 item types, 587×233×220 mm
// container) and verifies the full pipeline produces a valid solution.
// Uses runPipelineDims so the algorithm uses the BR container size, not the
// default Euro pallet.

TEST(BRBenchmark, FirstInstanceAllItemsPlaced)
{
    auto problems = loadBRFile("../../data/br_benchmark/thpack1.txt");
    ASSERT_GE(static_cast<int>(problems.size()), 1)
        << "Could not load thpack1.txt — check working directory";

    const BRProblem& prob = problems[0];
    const auto result = runPipelineDims(prob.items, prob.L, prob.W, prob.H);

    const int expected = totalExpected(prob.items);
    EXPECT_EQ(result.unplaced, 0);
    EXPECT_EQ(totalPlaced(result.solution), expected);
}

TEST(BRBenchmark, FirstInstanceNoConstraintViolations)
{
    auto problems = loadBRFile("../../data/br_benchmark/thpack1.txt");
    ASSERT_GE(static_cast<int>(problems.size()), 1);

    const BRProblem& prob = problems[0];
    const auto result = runPipelineDims(prob.items, prob.L, prob.W, prob.H);

    EXPECT_TRUE(noOverlaps(result.solution))    << "AABB overlap in BR instance 0";
    EXPECT_TRUE(allInBounds(result.solution))   << "Item out of bounds in BR instance 0";
    EXPECT_TRUE(allSupported(result.solution))  << "Unsupported item in BR instance 0";
}
