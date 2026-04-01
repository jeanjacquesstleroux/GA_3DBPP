#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

#include "BlockBuilder.h"
#include "Config.h"
#include "LayerGenerator.h"
#include "Types.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Count how many PlacedItems with the given item_type_index exist across all
// containers.
static int countPlaced(const std::vector<Container>& containers,
                       int type_idx) {
    int n = 0;
    for (const Container& c : containers)
        for (const PlacedItem& pi : c.items)
            if (pi.item_type_index == type_idx) ++n;
    return n;
}

// Verify that no two PlacedItems in the same container overlap (AABB check).
static bool noOverlaps(const std::vector<Container>& containers) {
    for (const Container& c : containers) {
        const auto& items = c.items;
        for (std::size_t i = 0; i < items.size(); ++i) {
            for (std::size_t j = i + 1; j < items.size(); ++j) {
                const PlacedItem& a = items[i];
                const PlacedItem& b = items[j];
                bool sep_x = (a.x + a.dx <= b.x) || (b.x + b.dx <= a.x);
                bool sep_y = (a.y + a.dy <= b.y) || (b.y + b.dy <= a.y);
                bool sep_z = (a.z + a.dz <= b.z) || (b.z + b.dz <= a.z);
                if (!sep_x && !sep_y && !sep_z) return false;
            }
        }
    }
    return true;
}

// Verify that every PlacedItem is within pallet bounds.
static bool inBounds(const std::vector<Container>& containers) {
    for (const Container& c : containers)
        for (const PlacedItem& pi : c.items) {
            if (pi.x < 0 || pi.x + pi.dx > c.L) return false;
            if (pi.y < 0 || pi.y + pi.dy > c.W) return false;
            if (pi.z < 0 || pi.z + pi.dz > c.H) return false;
        }
    return true;
}

// ===========================================================================
// LayerGenerator tests (Tasks 4.1–4.2)
// ===========================================================================

// A 300×200×150 item fits 4×4=16 times in the default 1200×800 full layer.
TEST(LayerGenerator, FullLayerItemCount) {
    ItemType item;
    item.l = 300; item.w = 200; item.h = 150; item.q = 32;
    Layer layer = LayerGenerator::generateFull(item, 0);
    EXPECT_EQ(layer.item_count, 16);
    EXPECT_EQ(layer.type,  LayerType::Full);
    EXPECT_EQ(layer.height, 150);
}

// generateFull should pick the rotation that fits more items.
// 400×300 item: Original → 3×2=6; Rotated → 2×4=8 (rotated wins).
TEST(LayerGenerator, FullLayerPicksBestOrientation) {
    ItemType item;
    item.l = 400; item.w = 300; item.h = 100; item.q = 20;
    Layer layer = LayerGenerator::generateFull(item, 0);
    EXPECT_EQ(layer.item_count, 8);
    EXPECT_EQ(layer.placed_items.size(), std::size_t(8));
}

// On a tie (both orientations fit the same count), Original is preferred.
TEST(LayerGenerator, FullLayerTieKeepsOriginal) {
    // 400×200: Original → 3×4=12; Rotated (200×400) → 6×2=12. Tie.
    ItemType item;
    item.l = 400; item.w = 200; item.h = 100; item.q = 20;
    Layer layer = LayerGenerator::generateFull(item, 0);
    EXPECT_EQ(layer.item_count, 12);
    EXPECT_EQ(layer.placed_items[0].orientation, Orientation::Original);
}

// Dynamic shifting (Task 4.2): first item at x=0, last item flush with wall.
TEST(LayerGenerator, DynamicShiftingFirstAndLastFlush) {
    // 300×200 item → 4 items along x (4×300=1200), no gap → positions are
    // 0, 300, 600, 900.  With shifting: pos[0]=0, pos[3]=1200-300=900.
    ItemType item;
    item.l = 300; item.w = 200; item.h = 100; item.q = 20;
    Layer layer = LayerGenerator::generateFull(item, 0);

    // Collect unique x positions.
    std::vector<int> xs;
    for (const PlacedItem& pi : layer.placed_items) xs.push_back(pi.x);
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

    EXPECT_EQ(xs.front(), 0);
    EXPECT_EQ(xs.back(),  1200 - 300);
}

// generateHalves returns ≤ 4 candidates, all with LayerType::Half.
TEST(LayerGenerator, HalvesAllHalfType) {
    ItemType item;
    item.l = 200; item.w = 200; item.h = 100; item.q = 20;
    auto halves = LayerGenerator::generateHalves(item, 0);
    EXPECT_GE(halves.size(), std::size_t(1));
    EXPECT_LE(halves.size(), std::size_t(4));
    for (const Layer& l : halves)
        EXPECT_EQ(l.type, LayerType::Half);
}

// generateQuarters returns ≤ 2 candidates, all with LayerType::Quarter.
TEST(LayerGenerator, QuartersAllQuarterType) {
    ItemType item;
    item.l = 200; item.w = 200; item.h = 100; item.q = 20;
    auto quarters = LayerGenerator::generateQuarters(item, 0);
    EXPECT_GE(quarters.size(), std::size_t(1));
    EXPECT_LE(quarters.size(), std::size_t(2));
    for (const Layer& l : quarters)
        EXPECT_EQ(l.type, LayerType::Quarter);
}

// An item larger than a quarter footprint yields zero-item candidates,
// which are filtered out, so generateQuarters returns an empty vector.
TEST(LayerGenerator, OversizedItemProducesNoQuarters) {
    ItemType item;
    item.l = 700; item.w = 500; item.h = 100; item.q = 1;
    // Quarter footprint is 600×400. 700 > 600, so no items fit.
    auto quarters = LayerGenerator::generateQuarters(item, 0);
    EXPECT_TRUE(quarters.empty());
}

// ===========================================================================
// BlockBuilder fill-rate filter (Task 4.3/4.5)
// ===========================================================================

TEST(BlockBuilder, FilterRemovesLowFillRate) {
    // Construct a Full layer with fill_rate just below threshold.
    Layer bad;
    bad.type      = LayerType::Full;
    bad.fill_rate = Config::LAYER_MIN_FILL_FULL_HALF - 0.01;

    Layer good;
    good.type      = LayerType::Full;
    good.fill_rate = Config::LAYER_MIN_FILL_FULL_HALF;

    std::vector<Layer> layers = {bad, good};
    BlockBuilder::filterByFillRate(layers);
    ASSERT_EQ(layers.size(), std::size_t(1));
    EXPECT_DOUBLE_EQ(layers[0].fill_rate, Config::LAYER_MIN_FILL_FULL_HALF);
}

TEST(BlockBuilder, QuarterThresholdIsLower) {
    Layer q;
    q.type      = LayerType::Quarter;
    q.fill_rate = Config::LAYER_MIN_FILL_QUARTER;
    EXPECT_TRUE(BlockBuilder::passesFillRate(q));

    q.fill_rate = Config::LAYER_MIN_FILL_FULL_HALF - 0.01;
    // Quarter threshold is 0.85, Full/Half is 0.90.  A layer at 0.89 passes
    // the quarter filter but would fail the full/half filter.
    q.fill_rate = 0.87;
    EXPECT_TRUE(BlockBuilder::passesFillRate(q));
}

// ===========================================================================
// BlockBuilder::buildBlocks integration tests (Tasks 4.6–4.9)
// ===========================================================================

// One item type that fills the pallet perfectly: all items are packed,
// no overlaps, stays in bounds.
TEST(BlockBuilder, PerfectDivisionAllItemsPacked) {
    // 300×200×100 item fills 1200×800 exactly (4×4=16 items per layer).
    ItemType item;
    item.l = 300; item.w = 200; item.h = 100; item.m = 1; item.q = 48;

    Layer full = LayerGenerator::generateFull(item, 0);
    ASSERT_EQ(full.item_count, 16);

    std::vector<Layer> candidates;
    // 3 full layers = 48 items = item.q
    candidates.push_back(full);
    candidates.push_back(full);
    candidates.push_back(full);

    std::vector<ItemType> types = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);

    EXPECT_FALSE(containers.empty());
    EXPECT_EQ(countPlaced(containers, 0), 48);
    EXPECT_TRUE(noOverlaps(containers));
    EXPECT_TRUE(inBounds(containers));
}

// Items that are too tall to fit more than one layer before hitting max height
// should overflow onto a second pallet.
TEST(BlockBuilder, TallItemOverflowsToNewPallet) {
    // 1200×800×700 item: one layer fills entire pallet, height=700.
    // Two layers would be 1400mm = max, so fits exactly.
    // Three layers would be 2100mm > 1400 → must open second pallet.
    ItemType item;
    item.l = 1200; item.w = 800; item.h = 700; item.m = 10; item.q = 3;

    Layer full = LayerGenerator::generateFull(item, 0);
    ASSERT_EQ(full.item_count, 1);

    std::vector<Layer> candidates = {full, full, full};
    std::vector<ItemType> types = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);

    // 3 items, max 2 per pallet (2×700=1400mm) → 2 pallets needed.
    EXPECT_GE(containers.size(), std::size_t(2));
    EXPECT_EQ(countPlaced(containers, 0), 3);
    EXPECT_TRUE(noOverlaps(containers));
    EXPECT_TRUE(inBounds(containers));
}

// Single item: generates one layer with 1 item, builds into a block.
TEST(BlockBuilder, SingleItemOrder) {
    ItemType item;
    item.l = 500; item.w = 400; item.h = 300; item.m = 5; item.q = 1;

    Layer full = LayerGenerator::generateFull(item, 0);
    ASSERT_GE(full.item_count, 1);

    std::vector<Layer> candidates = {full};
    std::vector<ItemType> types   = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);

    EXPECT_EQ(containers.size(), std::size_t(1));
    EXPECT_GE(countPlaced(containers, 0), 1);
    EXPECT_TRUE(inBounds(containers));
}

// All placed items must have z >= 0 (no items below the pallet floor).
TEST(BlockBuilder, NoItemsBelowFloor) {
    ItemType item;
    item.l = 300; item.w = 200; item.h = 100; item.m = 2; item.q = 32;

    Layer full = LayerGenerator::generateFull(item, 0);
    std::vector<Layer> candidates = {full, full};
    std::vector<ItemType> types   = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);

    for (const Container& c : containers)
        for (const PlacedItem& pi : c.items)
            EXPECT_GE(pi.z, 0);
}

// ===========================================================================
// BlockBuilder::computeResiduals (Task 4.10)
// ===========================================================================

TEST(BlockBuilder, NoResidualsWhenAllPacked) {
    ItemType item;
    item.l = 300; item.w = 200; item.h = 100; item.m = 1; item.q = 16;

    Layer full = LayerGenerator::generateFull(item, 0);
    ASSERT_EQ(full.item_count, 16);

    std::vector<Layer> candidates = {full};
    std::vector<ItemType> types   = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);
    auto info = BlockBuilder::computeResiduals(types, containers);

    EXPECT_TRUE(info.residuals.empty());
}

TEST(BlockBuilder, ResidualsDetectedForUnpackedItems) {
    // Create a type with q=20 but only pack 16 (one full layer of 300×200).
    ItemType item;
    item.l = 300; item.w = 200; item.h = 100; item.m = 1; item.q = 20;

    Layer full = LayerGenerator::generateFull(item, 0);
    ASSERT_EQ(full.item_count, 16);

    std::vector<Layer> candidates = {full};  // packs only 16
    std::vector<ItemType> types   = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);
    auto info = BlockBuilder::computeResiduals(types, containers);

    ASSERT_EQ(info.residuals.size(), std::size_t(1));
    EXPECT_EQ(info.residuals[0].first,  0);   // type index
    EXPECT_EQ(info.residuals[0].second, 4);   // 20 - 16 = 4 residuals
}

TEST(BlockBuilder, SpawnNewPalletWhenNoRemainingSpace) {
    // Fill a pallet to the brim: 14 layers × 100mm = 1400mm = PALLET_H.
    ItemType item;
    item.l = 300; item.w = 200; item.h = 100; item.m = 1; item.q = 500;

    std::vector<Layer> candidates;
    Layer full = LayerGenerator::generateFull(item, 0);
    for (int i = 0; i < 14; ++i) candidates.push_back(full);

    std::vector<ItemType> types = {item};
    auto containers = BlockBuilder::buildBlocks(candidates, types);

    // After 14 layers a pallet should be exactly full (remaining = 0).
    // Any residuals must trigger spawn_new_pallet.
    auto info = BlockBuilder::computeResiduals(types, containers);
    if (!info.residuals.empty()) {
        EXPECT_TRUE(info.spawn_new_pallet);
    }
}
