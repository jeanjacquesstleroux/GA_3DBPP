#include <gtest/gtest.h>
#include "CenterOfMass.h"

// Helpers
static PlacedItem makeItem(int x, int y, int z, int dx, int dy, int dz,
                            int type_index = 0)
{
    PlacedItem p;
    p.x = x;  p.y = y;  p.z = z;
    p.dx = dx; p.dy = dy; p.dz = dz;
    p.item_type_index = type_index;
    return p;
}

static ItemType makeType(int mass)
{
    ItemType t;
    t.m = mass;
    t.l = 1; t.w = 1; t.h = 1; t.q = 1;
    return t;
}

// -------------------------------------------------------------------------
// Test 1: Empty container — COM is zero
// -------------------------------------------------------------------------
TEST(CenterOfMass, EmptyReturnsZero)
{
    std::vector<PlacedItem> items;
    std::vector<ItemType>   types;
    auto com = CenterOfMass::compute(items, types);
    EXPECT_DOUBLE_EQ(com[0], 0.0);
    EXPECT_DOUBLE_EQ(com[1], 0.0);
    EXPECT_DOUBLE_EQ(com[2], 0.0);
}

// -------------------------------------------------------------------------
// Test 2: Single item whose centre is the pallet's geometric centre → stable
// -------------------------------------------------------------------------
TEST(CenterOfMass, SingleCenteredItemIsStable)
{
    // Pallet: 1200×800.  Geometric centre: (600, 400).
    // Item: 400×400×400 placed at (400, 200, 0) → centre = (600, 400, 200). ✓
    std::vector<ItemType>   types  = { makeType(10) };
    std::vector<PlacedItem> items  = { makeItem(400, 200, 0, 400, 400, 400) };

    auto com = CenterOfMass::compute(items, types);
    EXPECT_DOUBLE_EQ(com[0], 600.0);
    EXPECT_DOUBLE_EQ(com[1], 400.0);

    Container c;  // default Euro pallet dims from Config.h
    EXPECT_TRUE(CenterOfMass::isStable(com, c));
}

// -------------------------------------------------------------------------
// Test 3: Two equal-mass items mirrored about the pallet centre → stable
// -------------------------------------------------------------------------
TEST(CenterOfMass, SymmetricItemsAreStable)
{
    // Item A centre: (100, 100, 100)   Item B centre: (1100, 700, 100)
    // COM.x = (10×100 + 10×1100) / 20 = 600   COM.y = (10×100 + 10×700) / 20 = 400
    std::vector<ItemType>   types = { makeType(10) };
    std::vector<PlacedItem> items = {
        makeItem(0,    0,   0, 200, 200, 200),   // centre (100, 100, 100)
        makeItem(1000, 600, 0, 200, 200, 200)    // centre (1100, 700, 100)
    };

    auto com = CenterOfMass::compute(items, types);
    EXPECT_DOUBLE_EQ(com[0], 600.0);
    EXPECT_DOUBLE_EQ(com[1], 400.0);

    Container c;
    EXPECT_TRUE(CenterOfMass::isStable(com, c));
}

// -------------------------------------------------------------------------
// Test 4: Heavy item far off-centre → COM deviates > 60 mm → not stable
// -------------------------------------------------------------------------
TEST(CenterOfMass, AsymmetricHeavyItemIsUnstable)
{
    // One very heavy item at the far end of the pallet.
    // Centre: (1100, 700, 100).  Deviation from (600, 400): Δx=500, Δy=300 — both >> 60 mm.
    std::vector<ItemType>   types = { makeType(100) };
    std::vector<PlacedItem> items = { makeItem(1000, 600, 0, 200, 200, 200) };

    auto com = CenterOfMass::compute(items, types);
    EXPECT_DOUBLE_EQ(com[0], 1100.0);
    EXPECT_DOUBLE_EQ(com[1],  700.0);

    Container c;
    EXPECT_FALSE(CenterOfMass::isStable(com, c));
}
