#include <gtest/gtest.h>
#include "AABB.h"

// Helper: build a PlacedItem from position + dimensions.
static PlacedItem makeItem(int x, int y, int z, int dx, int dy, int dz)
{
    PlacedItem p;
    p.x = x;  p.y = y;  p.z = z;
    p.dx = dx; p.dy = dy; p.dz = dz;
    return p;
}

// Helper: build a Container with given dimensions.
static Container makeContainer(int L, int W, int H)
{
    Container c;
    c.L = L;  c.W = W;  c.H = H;
    return c;
}

// -------------------------------------------------------------------------
// AABB::overlaps
// -------------------------------------------------------------------------

TEST(AABB, SeparateItemsDoNotOverlap)
{
    // a occupies [0,100) × [0,100) × [0,100)
    // b occupies [200,300) × [0,100) × [0,100)  — gap on X axis
    auto a = makeItem(0,   0, 0, 100, 100, 100);
    auto b = makeItem(200, 0, 0, 100, 100, 100);
    EXPECT_FALSE(AABB::overlaps(a, b));
}

TEST(AABB, TouchingFacesDoNotOverlap)
{
    // a's right face (x=100) exactly meets b's left face (x=100) — touching, not overlapping
    auto a = makeItem(0,   0, 0, 100, 100, 100);
    auto b = makeItem(100, 0, 0, 100, 100, 100);
    EXPECT_FALSE(AABB::overlaps(a, b));
}

TEST(AABB, OneMmOverlapDetected)
{
    // b starts at x=99, so it overlaps a by 1 mm on the X axis
    auto a = makeItem(0,  0, 0, 100, 100, 100);
    auto b = makeItem(99, 0, 0, 100, 100, 100);
    EXPECT_TRUE(AABB::overlaps(a, b));
}

TEST(AABB, SamePositionOverlaps)
{
    // Identical boxes — full overlap
    auto a = makeItem(0, 0, 0, 100, 100, 100);
    auto b = makeItem(0, 0, 0, 100, 100, 100);
    EXPECT_TRUE(AABB::overlaps(a, b));
}

TEST(AABB, ContainedItemOverlaps)
{
    // b is entirely inside a
    auto a = makeItem(0,  0,  0,  400, 400, 400);
    auto b = makeItem(50, 50, 50, 100, 100, 100);
    EXPECT_TRUE(AABB::overlaps(a, b));
}

TEST(AABB, OverlapOnTwoAxesButNotThirdIsNotOverlap)
{
    // XY overlap, but a is on the floor (z 0–100) and b is above it (z 100–200)
    // Touching on Z face — not an interior overlap
    auto a = makeItem(0, 0,   0, 100, 100, 100);
    auto b = makeItem(0, 0, 100, 100, 100, 100);
    EXPECT_FALSE(AABB::overlaps(a, b));
}

// -------------------------------------------------------------------------
// AABB::fitsInContainer
// -------------------------------------------------------------------------

TEST(AABB, ItemExactlyFitsInContainer)
{
    // Item fills the entire container — far corners touch all walls
    auto c = makeContainer(1200, 800, 1400);
    auto p = makeItem(0, 0, 0, 1200, 800, 1400);
    EXPECT_TRUE(AABB::fitsInContainer(p, c));
}

TEST(AABB, ItemExceedsContainerLength)
{
    auto c = makeContainer(1200, 800, 1400);
    auto p = makeItem(0, 0, 0, 1201, 800, 1400);  // 1 mm too long
    EXPECT_FALSE(AABB::fitsInContainer(p, c));
}
