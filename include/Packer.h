#pragma once

#include <vector>
#include "Types.h"

// Packer — chromosome decoder for the genetic algorithm.
//
// decode() maps one Individual's chromosome to a PackingSolution by running the
// Extreme Points placement engine on residual items in chromosome order.  It is
// the bridge between the GA's search space (permutations of item-type indices)
// and the objective space (container count, utilization, wasted volume).
//
// Paper reference: Ananno & Ribeiro (2024), Section IV-C (Phase 2 decoder).
namespace Packer {

// Decode a chromosome into a PackingSolution.
//
// chromosome        — ordered permutation of residual item-type indices.
//                     The GA evolves this sequence; items are placed in this order.
// residualCounts    — residualCounts[i] is the number of items of type i that are
//                     residuals (did not form a Phase 1 layer).  Indexed by item-type
//                     index; types with count 0 do not appear in the chromosome.
// itemTypes         — the full item-type catalogue (shared read-only reference).
// seedContainers    — containers already holding Phase 1 blocks.  Residuals are
//                     placed on top of or beside those blocks.  May be empty (fully
//                     heterogeneous order where Phase 1 produced nothing).
// out_unplaced      — set to the number of items that could not be placed even in a
//                     freshly opened empty container (indicates an item larger than
//                     the pallet itself — should be zero for well-formed instances).
//
// Returns a PackingSolution with all containers used (seed + any new ones opened).
// The containers vector in the returned solution is always non-empty.
[[nodiscard]] PackingSolution decode(
    const std::vector<int>&      chromosome,
    const std::vector<int>&      residualCounts,
    const std::vector<ItemType>& itemTypes,
    const std::vector<Container>& seedContainers,
    int&                          out_unplaced);

} // namespace Packer
