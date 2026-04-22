#pragma once

#include <vector>
#include "Types.h"

// Extreme Points (EP) placement engine — Phase 2 of the packing algorithm.
//
// An ExtremePoint is a 3D coordinate where the near-bottom-left corner of the
// next item may be placed. The engine maintains a sorted list of EPs and uses
// them to place residual items (those that did not form Phase 1 layers/blocks).
//
// Paper reference: Ananno & Ribeiro (2024), Section IV-B-3, Figure 11.
//
// All functions are stateless: the EP list and Container are passed by reference
// and mutated in place. The same idiom used by LayerGenerator and BlockBuilder.
namespace ExtremePointEngine {

// ─── Task 5.1 / 5.7: Initialise the EP list for a container ─────────────────
//
// Empty container  → single EP at {0, 0, 0}.
// Container with Phase 1 blocks → generates the 3 standard EPs per placed item
// (right, behind, top), then projects, prunes, and sorts the full set.
// This captures both top-surface EPs (for stacking) and adjacent EPs (for
// placing beside blocks in empty pallet space).
void init(std::vector<ExtremePoint>& eps, const Container& cont);

// ─── Task 5.2: Generate 3 new EPs from a just-placed item ───────────────────
//
// After placing an item at (x, y, z) with dims (dx, dy, dz), three candidate
// positions are created where the next item may go:
//   [x+dx,  y,    z  ]  — right face
//   [x,     y+dy, z  ]  — back face
//   [x,     y,    z+dz]  — top face
//
// These are raw (unprojected) EPs. Call project() → prune() → sortEPs() after
// all new EPs for a round have been added.
void generateFrom(const PlacedItem& pi, std::vector<ExtremePoint>& eps);

// ─── Task 5.3: Project EPs downward to the nearest supporting surface ────────
//
// For each EP at (px, py, pz_raw), finds the highest z' ≤ pz_raw at which a
// supporting surface exists directly below (px, py): either a placed item's
// top face or the pallet floor (z = 0). Snaps the EP to z'.
//
// Surface coverage uses inclusive bounds [pi.x, pi.x+pi.dx] so that a corner
// point exactly on an item's edge is considered supported.
//
// EPs whose x ≥ cont.L, y ≥ cont.W, or z ≥ cont.H after projection are
// removed — no item can start at or beyond the container wall.
void project(std::vector<ExtremePoint>& eps, const Container& cont);

// ─── Task 5.4: Remove interior, dominated, and duplicate EPs ────────────────
//
// Interior  : EP (px, py, pz) lies inside a placed item's half-open volume
//             [pi.x, pi.x+pi.dx) × [pi.y, pi.y+pi.dy) × [pi.z, pi.z+pi.dz).
//             Any item placed at an interior EP would immediately collide.
//
// Dominated : EP B is dominated by EP A if A.x ≤ B.x, A.y ≤ B.y, A.z ≤ B.z
//             and A ≠ B. A is at least as close to the origin in every
//             dimension, so it is always tried before B during sorted iteration.
//
// Duplicates: exact (x, y, z) matches — the EP list must not contain copies.
void prune(std::vector<ExtremePoint>& eps, const Container& cont);

// ─── Task 5.5: Sort EPs by priority ─────────────────────────────────────────
//
// Primary key  : z ascending (lower EPs are tried first to discourage tall
//                columns — paper: "Lower EPs score higher").
// Tiebreaker   : squared distance to origin (x²+y²) ascending (integer, exact,
//                avoids sqrt — paper: "closer to the origin is ranked first").
void sortEPs(std::vector<ExtremePoint>& eps);

// ─── Task 5.6: Attempt to place one copy of an item type ────────────────────
//
// Iterates the sorted EP list and tries placing item_types[type_idx] at each EP:
//   1. Try Orientation::Original → check bounds, non-collision (AABB), support.
//   2. If any check fails, try Orientation::Rotated90 with the same checks.
//   3. If both orientations fail, advance to the next EP.
//   4. On success: commit item to cont.items, remove the used EP, generate 3
//      new EPs via generateFrom, then project → prune → sortEPs in place.
//   5. Returns true on success, false if all EPs are exhausted (penalise the
//      individual in the GA fitness evaluation).
//
// COM stability (soft constraint, Constraint 3) is intentionally NOT checked
// here: it is a fitness-function concern, and zero-mass items (common in tests)
// would make the check degenerate. Support (Constraint 4, soft) IS checked
// because EP projection guarantees a surface exists below but not that it covers
// enough of the item's base area.
[[nodiscard]] bool placeItem(Container&                    cont,
                              const std::vector<ItemType>&  item_types,
                              int                           type_idx,
                              std::vector<ExtremePoint>&    eps,
                              bool                          debug   = false,
                              bool                          relaxed = false);

} // namespace ExtremePointEngine
