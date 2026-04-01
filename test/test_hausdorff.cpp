#include <gtest/gtest.h>
#include "Hausdorff.h"

using Points = std::vector<std::array<int, 2>>;

// -------------------------------------------------------------------------
// Test 1: Identical point sets — distance is zero
// -------------------------------------------------------------------------
TEST(Hausdorff, IdenticalSetsDistanceIsZero)
{
    Points A = {{0, 0}, {100, 0}, {0, 100}, {100, 100}};
    Points B = A;  // exact copy
    EXPECT_DOUBLE_EQ(Hausdorff::distance(A, B), 0.0);
}

// -------------------------------------------------------------------------
// Test 2: Shifted sets — distance equals the shift magnitude
// -------------------------------------------------------------------------
TEST(Hausdorff, ShiftedSetsDistanceEqualsShift)
{
    // B is A shifted 100 mm along X.
    // For every point in A, the nearest point in B is 100 mm away (the shifted copy).
    // For the two points in B that have no matching point in A at distance 0
    // (the new rightmost column at x=200), nearest in A is at x=100 → 100 mm.
    // H(A,B) = max(100, 100) = 100.
    Points A = {{0, 0}, {100, 0}, {0, 100}, {100, 100}};
    Points B = {{100, 0}, {200, 0}, {100, 100}, {200, 100}};
    EXPECT_DOUBLE_EQ(Hausdorff::distance(A, B), 100.0);
}

// -------------------------------------------------------------------------
// Test 3: Fully disjoint sets separated by 300 mm on Y
// -------------------------------------------------------------------------
TEST(Hausdorff, DisjointSetsDistanceIsGap)
{
    // A lies at y=0, B at y=300.  Every point in A is 300 mm from its nearest
    // neighbour in B, and vice versa.  H(A,B) = 300.
    Points A = {{0, 0},   {100, 0}  };
    Points B = {{0, 300}, {100, 300}};
    EXPECT_DOUBLE_EQ(Hausdorff::distance(A, B), 300.0);
}