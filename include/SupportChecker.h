#pragma once

#include <vector>
#include "Config.h"
#include "Types.h"

// Checks whether a placed item has adequate physical support from below.
//
// Support sources: the container floor (z == 0) always provides full support.
// For items above the floor, support comes from the top faces of items directly
// beneath — those whose (z + dz) equals the new item's z.
//
// Three tiers are tested in order; the item passes if it satisfies any one tier:
//   Tier 1: >= 4 inset base vertices supported  AND  >= 40% base area covered
//   Tier 2: >= 3 inset base vertices supported  AND  >= 50% base area covered
//   Tier 3: >= 2 inset base vertices supported  AND  >= 75% base area covered
//
// "Inset base vertices" are the item's four bottom corners pulled inward by
// vertex_inset mm on each axis before testing (reduces sensitivity to exact
// edge contact, per the paper's practical adjustment).
class SupportChecker {
public:
    explicit SupportChecker(int vertex_inset = Config::SUPPORT_VERTEX_INSET);

    // Returns true if item has adequate support given all other placed items.
    // `others` must NOT include `item` itself.
    [[nodiscard]] bool isSupported(const PlacedItem&              item,
                                   const std::vector<PlacedItem>& others) const;

private:
    int inset_;

    // Returns the number of the item's 4 inset base vertices that fall within
    // any supporter's XY footprint.
    [[nodiscard]] int countSupportedVertices(
        const PlacedItem&              item,
        const std::vector<PlacedItem>& supporters) const;

    // Returns the fraction of the item's base area covered by supporters (0.0–1.0).
    // Overlapping supporters may slightly overestimate coverage, but in practice
    // supporters at the same z level rarely overlap (that would violate AABB).
    [[nodiscard]] double baseAreaCoverage(
        const PlacedItem&              item,
        const std::vector<PlacedItem>& supporters) const;
};
