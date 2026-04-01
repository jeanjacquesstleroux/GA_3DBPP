#pragma once

#include "Types.h"

// Axis-Aligned Bounding Box collision and bounds utilities.
//
// All items in this system are axis-aligned (only Z-axis 90° rotation is allowed),
// so overlap detection reduces to six integer comparisons across three axes.
//
// Convention: a PlacedItem occupies the half-open volume
//   [x, x+dx) × [y, y+dy) × [z, z+dz)
// Two items with touching faces share no interior volume and do NOT overlap.
namespace AABB {

// Returns true iff items a and b share interior volume.
// Touching faces (e.g. a.x + a.dx == b.x) return false — strict < on each axis.
[[nodiscard]] inline bool overlaps(const PlacedItem& a, const PlacedItem& b)
{
    return a.x        < b.x + b.dx &&
           b.x        < a.x + a.dx &&
           a.y        < b.y + b.dy &&
           b.y        < a.y + a.dy &&
           a.z        < b.z + b.dz &&
           b.z        < a.z + a.dz;
}

// Returns true iff the placed item lies entirely within the container boundaries.
// The item must satisfy 0 ≤ pos and pos + dim ≤ container_dim on every axis.
// An item whose far corner exactly equals the container wall is valid (≤, not <).
[[nodiscard]] inline bool fitsInContainer(const PlacedItem& p, const Container& c)
{
    return p.x >= 0       && p.y >= 0       && p.z >= 0      &&
           p.x + p.dx <= c.L &&
           p.y + p.dy <= c.W &&
           p.z + p.dz <= c.H;
}

} // namespace AABB
