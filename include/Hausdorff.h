#pragma once

#include <array>
#include <vector>

// Hausdorff distance between two 2D integer point sets.
//
// In the packing algorithm (Phase 4), this is applied to the projected XY
// corner vertices of items in adjacent layers. A higher Hausdorff distance
// means the layers interlock less uniformly — corners of one layer reach
// into gaps of the other — which improves pallet stability under transport loads.
//
// Symmetric Hausdorff distance:
//   H(A, B) = max( directed(A→B), directed(B→A) )
// where:
//   directed(A→B) = max over a∈A of  min over b∈B of  euclidean_dist(a, b)
namespace Hausdorff {

// Returns H(A, B) as defined above.
// Returns 0.0 if either set is empty.
[[nodiscard]] double distance(const std::vector<std::array<int, 2>>& A,
                               const std::vector<std::array<int, 2>>& B);

} // namespace Hausdorff
