#pragma once

#include <array>
#include <vector>
#include "Config.h"
#include "Types.h"

// Center-of-mass computation and horizontal stability check.
//
// The COM is the mass-weighted average position of all placed items in a container.
// Stability is assessed in the XY plane only: tipping is a horizontal problem.
// A solution is stable if the COM's XY position stays within Config::COM_MAX_DEVIATION
// mm of the pallet's geometric center (L/2, W/2).
class CenterOfMass {
public:
    // Computes the 3D mass-weighted centroid of all placed items.
    // Returns {0, 0, 0} if items is empty or every item has zero mass.
    // Requires item_types to resolve each PlacedItem's mass via item_type_index.
    [[nodiscard]] static std::array<double, 3> compute(
        const std::vector<PlacedItem>& items,
        const std::vector<ItemType>&   item_types);

    // Returns true iff the XY position of com is within Config::COM_MAX_DEVIATION mm
    // of the container's geometric center (container.L/2, container.W/2).
    [[nodiscard]] static bool isStable(const std::array<double, 3>& com,
                                       const Container&              container);
};
