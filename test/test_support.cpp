#include <gtest/gtest.h>
#include "SupportChecker.h"

// Use inset=0 throughout so vertex coordinates match footprint edges exactly,
// making expected values straightforward to reason about.
static SupportChecker checker(/*vertex_inset=*/0);

static PlacedItem makeItem(int x, int y, int z, int dx, int dy, int dz)
{
    PlacedItem p;
    p.x = x;  p.y = y;  p.z = z;
    p.dx = dx; p.dy = dy; p.dz = dz;
    return p;
}

// -------------------------------------------------------------------------
// Test 1: Floor support — z == 0 always passes regardless of others
// -------------------------------------------------------------------------
TEST(SupportChecker, FloorIsAlwaysSupported)
{
    auto item = makeItem(0, 0, 0, 400, 300, 200);
    std::vector<PlacedItem> others;  // empty — no other items needed
    EXPECT_TRUE(checker.isSupported(item, others));
}

// -------------------------------------------------------------------------
// Test 2: Tier 1 — 4 vertices supported, 100% area coverage
// -------------------------------------------------------------------------
TEST(SupportChecker, FullSupportTier1)
{
    // Supporter is larger than item in XY — all 4 corners land inside it.
    // Supporter top face at z=200, item bottom at z=200.
    auto supporter = makeItem(0, 0, 0, 600, 500, 200);
    auto item      = makeItem(0, 0, 200, 400, 300, 100);
    EXPECT_TRUE(checker.isSupported(item, {supporter}));
}

// -------------------------------------------------------------------------
// Test 3: Tier 2 — 3 vertices supported, >50% area coverage
// -------------------------------------------------------------------------
TEST(SupportChecker, ThreeVerticesTier2)
{
    // Two non-overlapping supporters that together cover 3 of the 4 corners:
    //   Supporter A: x=[0,400] y=[0,200] — covers the bottom two corners
    //   Supporter B: x=[0,200] y=[200,300] — covers the top-left corner only
    // Top-right corner (400,300) is unsupported.
    //
    // Item: x=[0,400] y=[0,300] z=200, so base area = 120 000 mm²
    // A overlap: 400×200 = 80 000
    // B overlap: 200×100 = 20 000
    // Total covered = 100 000 / 120 000 = 83 % >= 50 % → Tier 2
    auto suppA = makeItem(0,   0,   0, 400, 200, 200);
    auto suppB = makeItem(0,   200, 0, 200, 100, 200);
    auto item  = makeItem(0,   0, 200, 400, 300, 100);
    EXPECT_TRUE(checker.isSupported(item, {suppA, suppB}));
}

// -------------------------------------------------------------------------
// Test 4: Tier 3 — 2 vertices supported, exactly 75% area coverage
// -------------------------------------------------------------------------
TEST(SupportChecker, TwoVerticesTier3)
{
    // Supporter covers left 300 mm of item's 400 mm width.
    // Left two corners (x=0) are inside [0,300]; right two (x=400) are not.
    // Coverage = 300×300 / (400×300) = 75 % → exactly Tier 3
    auto supporter = makeItem(0, 0, 0, 300, 300, 200);
    auto item      = makeItem(0, 0, 200, 400, 300, 100);
    EXPECT_TRUE(checker.isSupported(item, {supporter}));
}

// -------------------------------------------------------------------------
// Test 5: Floating — no items directly below
// -------------------------------------------------------------------------
TEST(SupportChecker, FloatingNotSupported)
{
    // Supporter top is at z=100, but item bottom is at z=200 — 100 mm gap.
    auto supporter = makeItem(0, 0, 0, 400, 300, 100);  // top at z=100
    auto item      = makeItem(0, 0, 200, 400, 300, 100); // bottom at z=200
    EXPECT_FALSE(checker.isSupported(item, {supporter}));
}

// -------------------------------------------------------------------------
// Test 6: Below every threshold — 2 vertices, only 25% area coverage
// -------------------------------------------------------------------------
TEST(SupportChecker, BelowThresholdNotSupported)
{
    // Supporter covers only left 100 mm of item's 400 mm width.
    // 2 vertices supported (left corners), but coverage = 100×300 / (400×300) = 25%
    // Tier 3 requires 75% → fails all tiers.
    auto supporter = makeItem(0, 0, 0, 100, 300, 200);
    auto item      = makeItem(0, 0, 200, 400, 300, 100);
    EXPECT_FALSE(checker.isSupported(item, {supporter}));
}
