#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>

#include "ExtremePointEngine.h"
#include "Types.h"
#include "Config.h"

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Returns true if eps contains an EP at exactly (x, y, z).
static bool hasEP(const std::vector<ExtremePoint>& eps, int x, int y, int z) {
    return std::any_of(eps.begin(), eps.end(), [x, y, z](const ExtremePoint& ep) {
        return ep.x == x && ep.y == y && ep.z == z;
    });
}

// Builds a PlacedItem with all fields set explicitly (no implicit zeros).
static PlacedItem makePi(int type_idx, int x, int y, int z, int dx, int dy, int dz,
                          Orientation ori = Orientation::Original) {
    PlacedItem pi;
    pi.item_type_index = type_idx;
    pi.orientation = ori;
    pi.x = x; pi.y = y; pi.z = z;
    pi.dx = dx; pi.dy = dy; pi.dz = dz;
    return pi;
}

// Builds an ItemType with given l, w, h (mass defaults to 0).
static ItemType makeItem(int l, int w, int h, int q = 1, int m = 0) {
    ItemType it;
    it.l = l; it.w = w; it.h = h; it.q = q; it.m = m;
    return it;
}

// ─── Task 5.1: EP Initialisation ─────────────────────────────────────────────

// An empty container has no items, so the only candidate position is the origin.
TEST(ExtremePointEngine, InitEmptyContainerProducesSingleOriginEP) {
    Container cont;
    std::vector<ExtremePoint> eps;

    ExtremePointEngine::init(eps, cont);

    ASSERT_EQ(eps.size(), 1u);
    EXPECT_EQ(eps[0].x, 0);
    EXPECT_EQ(eps[0].y, 0);
    EXPECT_EQ(eps[0].z, 0);
}

// After a Phase 1 block is placed, init must produce at least one EP on the
// top surface so Phase 2 can stack residuals on top.
TEST(ExtremePointEngine, InitWithBlockProducesTopSurfaceEP) {
    Container cont;
    // Single item covering the left quarter of the pallet floor.
    cont.items.push_back(makePi(0, 0, 0, 0, 300, 200, 100));

    std::vector<ExtremePoint> eps;
    ExtremePointEngine::init(eps, cont);

    // There must be at least one EP at z = 100 (top of the block).
    bool has_top = std::any_of(eps.begin(), eps.end(),
        [](const ExtremePoint& ep) { return ep.z == 100; });
    EXPECT_TRUE(has_top);
}

// ─── Task 5.2: EP Generation ─────────────────────────────────────────────────

// Placing one item at (100, 50, 0) with dx=300, dy=200, dz=100 must generate
// the three canonical raw EPs before projection.
TEST(ExtremePointEngine, GenerateFromProducesThreeEPs) {
    // generateFrom appends directly; start with an empty vector.
    std::vector<ExtremePoint> eps;
    PlacedItem pi = makePi(0, 100, 50, 0, 300, 200, 100);

    ExtremePointEngine::generateFrom(pi, eps);

    ASSERT_EQ(eps.size(), 3u);
    // Right face: (100+300, 50, 0) = (400, 50, 0)
    EXPECT_TRUE(hasEP(eps, 400, 50, 0));
    // Back face:  (100, 50+200, 0) = (100, 250, 0)
    EXPECT_TRUE(hasEP(eps, 100, 250, 0));
    // Top face:   (100, 50, 0+100) = (100, 50, 100)
    EXPECT_TRUE(hasEP(eps, 100, 50, 100));
}

// ─── Task 5.3: EP Projection ─────────────────────────────────────────────────

// An EP above empty pallet space (nothing below it) must snap to the floor (z=0).
TEST(ExtremePointEngine, ProjectionSnapsToFloorWhenNothingBelow) {
    Container cont;  // no items
    std::vector<ExtremePoint> eps = {{500, 0, 300}};

    ExtremePointEngine::project(eps, cont);

    ASSERT_EQ(eps.size(), 1u);
    EXPECT_EQ(eps[0].z, 0);
    EXPECT_EQ(eps[0].x, 500);
}

// An EP directly above a placed item's top face must snap to that top face,
// not fall through to the floor.
TEST(ExtremePointEngine, ProjectionSnapsToItemTopFace) {
    Container cont;
    cont.items.push_back(makePi(0, 0, 0, 0, 400, 300, 100));  // top at z=100

    // EP raw z=200 at (0,0): surface below is the item's top at 100.
    std::vector<ExtremePoint> eps = {{0, 0, 200}};
    ExtremePointEngine::project(eps, cont);

    ASSERT_EQ(eps.size(), 1u);
    EXPECT_EQ(eps[0].z, 100);
}

// An EP at x ≥ container length must be removed — no item can start there.
TEST(ExtremePointEngine, ProjectionRemovesOutOfBoundsEP) {
    Container cont;  // L=1200, W=800, H=1400 (from Config)
    std::vector<ExtremePoint> eps = {
        {0,         0, 0},   // valid
        {cont.L,    0, 0},   // x == L — invalid (item would start at the wall)
        {0,    cont.W, 0},   // y == W — invalid
    };

    ExtremePointEngine::project(eps, cont);

    ASSERT_EQ(eps.size(), 1u);
    EXPECT_EQ(eps[0].x, 0);
}

// ─── Task 5.4: EP Maintenance (prune) ────────────────────────────────────────

// An EP whose coordinate lies within a placed item's half-open volume must be
// removed: any item placed there would immediately collide.
TEST(ExtremePointEngine, PruneRemovesInteriorEP) {
    Container cont;
    // Item at (0,0,0) occupying [0,600)×[0,400)×[0,200).
    cont.items.push_back(makePi(0, 0, 0, 0, 600, 400, 200));

    // EP at (100, 100, 100): strictly inside the item in all three axes.
    std::vector<ExtremePoint> eps = {{100, 100, 100}};
    ExtremePointEngine::prune(eps, cont);

    EXPECT_TRUE(eps.empty());
}

// An EP on the right face of an item (x == pi.x+pi.dx) is NOT interior —
// items can be placed touching that face.
TEST(ExtremePointEngine, PruneKeepsEPOnItemFace) {
    Container cont;
    cont.items.push_back(makePi(0, 0, 0, 0, 300, 200, 100));

    // EP exactly at x = 300 (right face), y=0, z=0.
    std::vector<ExtremePoint> eps = {{300, 0, 0}};
    ExtremePointEngine::prune(eps, cont);

    EXPECT_EQ(eps.size(), 1u);
}

// Exact-duplicate EPs must be deduplicated to one copy.
TEST(ExtremePointEngine, PruneDeduplicates) {
    Container cont;
    std::vector<ExtremePoint> eps = {{0,0,0}, {0,0,0}, {0,0,0}};
    ExtremePointEngine::prune(eps, cont);
    EXPECT_EQ(eps.size(), 1u);
}

// A dominated EP (strictly farther from origin in all axes than another) is removed.
TEST(ExtremePointEngine, PruneRemovesDominatedEP) {
    Container cont;
    // EP A = (0, 0, 0) dominates EP B = (100, 50, 0)
    // because A.x ≤ B.x, A.y ≤ B.y, A.z ≤ B.z with at least one strict.
    std::vector<ExtremePoint> eps = {{0, 0, 0}, {100, 50, 0}};
    ExtremePointEngine::prune(eps, cont);

    ASSERT_EQ(eps.size(), 1u);
    EXPECT_EQ(eps[0].x, 0);
    EXPECT_EQ(eps[0].y, 0);
    EXPECT_EQ(eps[0].z, 0);
}

// ─── Task 5.5: EP Sorting ────────────────────────────────────────────────────

// Lower z must come first.  Within the same z, the EP closer to the origin
// (smaller x²+y²) must come first.
TEST(ExtremePointEngine, SortByZThenByDistanceToOrigin) {
    std::vector<ExtremePoint> eps = {
        {600, 0,   0},   // z=0, dist²=360000
        {  0, 0, 200},   // z=200
        {  0, 0,   0},   // z=0, dist²=0  ← should be first
        {300, 0,   0},   // z=0, dist²=90000
    };

    ExtremePointEngine::sortEPs(eps);

    EXPECT_EQ(eps[0].x, 0);   EXPECT_EQ(eps[0].z, 0);   // (0,0,0)
    EXPECT_EQ(eps[1].x, 300); EXPECT_EQ(eps[1].z, 0);   // (300,0,0)
    EXPECT_EQ(eps[2].x, 600); EXPECT_EQ(eps[2].z, 0);   // (600,0,0)
    EXPECT_EQ(eps[3].z, 200);                            // anything at z=200 last
}

// ─── Task 5.6: Placement Procedure ───────────────────────────────────────────

// Placing a single item onto an empty pallet must succeed and commit the item.
TEST(ExtremePointEngine, PlaceItemOnEmptyPalletSucceeds) {
    Container cont;
    std::vector<ItemType> types = {makeItem(300, 200, 100, 1)};
    std::vector<ExtremePoint> eps;
    ExtremePointEngine::init(eps, cont);

    const bool ok = ExtremePointEngine::placeItem(cont, types, 0, eps);

    EXPECT_TRUE(ok);
    ASSERT_EQ(cont.items.size(), 1u);
    EXPECT_EQ(cont.items[0].x, 0);
    EXPECT_EQ(cont.items[0].y, 0);
    EXPECT_EQ(cont.items[0].z, 0);
    EXPECT_EQ(cont.items[0].dz, 100);
}

// Placing multiple items sequentially must pack them without collision.
TEST(ExtremePointEngine, PlaceMultipleItemsNoOverlap) {
    Container cont;
    const int N = 4;
    std::vector<ItemType> types = {makeItem(300, 200, 100, N)};
    std::vector<ExtremePoint> eps;
    ExtremePointEngine::init(eps, cont);

    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(ExtremePointEngine::placeItem(cont, types, 0, eps))
            << "Failed to place item " << i;
    }

    ASSERT_EQ(static_cast<int>(cont.items.size()), N);

    // O(N²) AABB overlap check.
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            const PlacedItem& a = cont.items[i];
            const PlacedItem& b = cont.items[j];
            const bool overlap =
                a.x < b.x + b.dx && b.x < a.x + a.dx &&
                a.y < b.y + b.dy && b.y < a.y + a.dy &&
                a.z < b.z + b.dz && b.z < a.z + a.dz;
            EXPECT_FALSE(overlap) << "Items " << i << " and " << j << " overlap";
        }
    }
}

// If an item is too large to fit anywhere in the container, placeItem returns false.
TEST(ExtremePointEngine, PlaceItemReturnsFalseWhenNoEPFits) {
    Container cont;
    // Item larger than the container on every axis.
    std::vector<ItemType> types = {makeItem(cont.L + 1, cont.W + 1, cont.H + 1, 1)};
    std::vector<ExtremePoint> eps;
    ExtremePointEngine::init(eps, cont);

    const bool ok = ExtremePointEngine::placeItem(cont, types, 0, eps);

    EXPECT_FALSE(ok);
    EXPECT_TRUE(cont.items.empty());
}

// ─── Task 5.7: Phase 1 → Phase 2 Handoff ─────────────────────────────────────

// After a Phase 1 block occupies part of the pallet, init must produce an EP
// adjacent to the block at floor level so Phase 2 can fill the empty half.
TEST(ExtremePointEngine, HandoffFindsAdjacentFloorEP) {
    Container cont;
    // Block covers the left half (x: 0–600) of the pallet at floor level.
    cont.items.push_back(makePi(0, 0, 0, 0, 600, 800, 100));

    std::vector<ExtremePoint> eps;
    ExtremePointEngine::init(eps, cont);

    // EP at (600, 0, 0) should be available for placing items in the right half.
    EXPECT_TRUE(hasEP(eps, 600, 0, 0));
}

// After init from blocks, placing an item on the top surface of a Phase 1 block
// must succeed (the EP at z=block_top must be valid for placement).
TEST(ExtremePointEngine, HandoffPlaceOnTopOfBlock) {
    Container cont;
    // Phase 1 block: full 1200×800 base, height 100.
    cont.items.push_back(makePi(0, 0, 0, 0, 1200, 800, 100));

    std::vector<ExtremePoint> eps;
    ExtremePointEngine::init(eps, cont);

    std::vector<ItemType> types = {makeItem(300, 200, 100, 1)};
    const bool ok = ExtremePointEngine::placeItem(cont, types, 0, eps);

    EXPECT_TRUE(ok);
    // The placed item must be on top of the block (z == 100).
    ASSERT_EQ(cont.items.size(), 2u);
    EXPECT_EQ(cont.items.back().z, 100);
}
