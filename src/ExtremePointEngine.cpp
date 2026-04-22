#include "ExtremePointEngine.h"

#include <algorithm>
#include <vector>

#include "AABB.h"
#include "SupportChecker.h"

namespace ExtremePointEngine {

// ─── Internal helpers ────────────────────────────────────────────────────────

// Returns the highest z' such that:
//   (a) z' ≤ max_z, and
//   (b) a surface exists at (px, py) at height z'.
//
// Surfaces: the pallet floor at z = 0 (always qualifies), and the top face of
// any placed item whose XY footprint covers (px, py).
//
// Inclusive bounds are used for surface coverage: pi.x ≤ px ≤ pi.x+pi.dx.
// This means a point exactly on an item's far edge is considered supported —
// the corner of the item's top face qualifies as a resting surface.
static int projectZ(int px, int py, int max_z, const Container& cont) {
    int best = 0;  // pallet floor always available
    for (const PlacedItem& pi : cont.items) {
        const int top = pi.z + pi.dz;
        if (top > max_z) continue;
        if (px >= pi.x && px <= pi.x + pi.dx &&
            py >= pi.y && py <= pi.y + pi.dy) {
            best = std::max(best, top);
        }
    }
    return best;
}

// Returns true if (px, py, pz) lies within the half-open volume of any item:
//   [pi.x, pi.x+pi.dx) × [pi.y, pi.y+pi.dy) × [pi.z, pi.z+pi.dz)
//
// Half-open bounds match the AABB convention. A point exactly on the far face
// (e.g. px == pi.x+pi.dx) is NOT interior — it is on the boundary of that item.
static bool isInterior(int px, int py, int pz, const Container& cont) {
    for (const PlacedItem& pi : cont.items) {
        if (px >= pi.x && px < pi.x + pi.dx &&
            py >= pi.y && py < pi.y + pi.dy &&
            pz >= pi.z && pz < pi.z + pi.dz) {
            return true;
        }
    }
    return false;
}

// ─── Internal helpers (continued) ───────────────────────────────────────────

// Remove interior EPs and deduplicate.  Does NOT apply dominance pruning.
// Used by init() so that EPs at higher z (on top of Phase 1 blocks) survive
// alongside low-z gap EPs generated from the same block surface.  If dominance
// were applied here, a 2 mm gap EP at (x, y, 0) would eliminate a perfectly
// valid (x, y, 28) EP that sits on top of the block — leaving the decoder with
// no way to continue once the low-z gap is AABB-blocked.
static void pruneNoDominate(std::vector<ExtremePoint>& eps, const Container& cont) {
    // ── Step 1: remove interior EPs ─────────────────────────────────────────
    eps.erase(
        std::remove_if(eps.begin(), eps.end(),
            [&](const ExtremePoint& ep) {
                return isInterior(ep.x, ep.y, ep.z, cont);
            }),
        eps.end());

    // ── Step 2: deduplicate ──────────────────────────────────────────────────
    std::sort(eps.begin(), eps.end(),
        [](const ExtremePoint& a, const ExtremePoint& b) {
            if (a.z != b.z) return a.z < b.z;
            if (a.x != b.x) return a.x < b.x;
            return a.y < b.y;
        });
    eps.erase(
        std::unique(eps.begin(), eps.end(),
            [](const ExtremePoint& a, const ExtremePoint& b) {
                return a.x == b.x && a.y == b.y && a.z == b.z;
            }),
        eps.end());
}

// ─── Public functions ────────────────────────────────────────────────────────

void init(std::vector<ExtremePoint>& eps, const Container& cont) {
    eps.clear();

    if (cont.items.empty()) {
        // Empty container: the only candidate position is the origin.
        eps.push_back({0, 0, 0});
        return;
    }

    // Phase 1 → Phase 2 handoff (Task 5.7):
    // Generate the standard 3 EPs for every already-placed item.  This creates:
    //   - EPs on top of block surfaces  (for stacking residuals on the block)
    //   - EPs adjacent to block edges   (for filling empty pallet space)
    // After generating, project → prune → sort to leave only the useful set.
    for (const PlacedItem& pi : cont.items) {
        generateFrom(pi, eps);
    }
    project(eps, cont);
    // Use interior+dedup only — NO dominance.  Dominance would eliminate valid
    // EPs on top of Phase 1 block surfaces when a lower-z gap EP happens to
    // have the same (x, y) prefix, leaving the decoder unable to fill those
    // surfaces after the gap is AABB-blocked.
    pruneNoDominate(eps, cont);
    sortEPs(eps);
}

void generateFrom(const PlacedItem& pi, std::vector<ExtremePoint>& eps) {
    // Right face: item to the right of pi at the same z level.
    eps.push_back({pi.x + pi.dx, pi.y,        pi.z        });
    // Back face: item behind pi at the same z level.
    eps.push_back({pi.x,         pi.y + pi.dy, pi.z        });
    // Top face: item directly on top of pi.
    eps.push_back({pi.x,         pi.y,         pi.z + pi.dz});
}

void project(std::vector<ExtremePoint>& eps, const Container& cont) {
    for (ExtremePoint& ep : eps) {
        ep.z = projectZ(ep.x, ep.y, ep.z, cont);
    }

    // Remove EPs that start at or beyond a container wall.
    // Any item placed at x ≥ L (or y ≥ W, z ≥ H) would immediately exceed bounds.
    eps.erase(
        std::remove_if(eps.begin(), eps.end(),
            [&](const ExtremePoint& ep) {
                return ep.x >= cont.L || ep.y >= cont.W || ep.z >= cont.H;
            }),
        eps.end());
}

void prune(std::vector<ExtremePoint>& eps, const Container& cont) {
    // ── Step 1: remove interior EPs ─────────────────────────────────────────
    eps.erase(
        std::remove_if(eps.begin(), eps.end(),
            [&](const ExtremePoint& ep) {
                return isInterior(ep.x, ep.y, ep.z, cont);
            }),
        eps.end());

    // ── Step 2: deduplicate ──────────────────────────────────────────────────
    // Sort by (z, x²+y²) so that std::unique can find adjacent equal triples.
    std::sort(eps.begin(), eps.end(),
        [](const ExtremePoint& a, const ExtremePoint& b) {
            if (a.z != b.z) return a.z < b.z;
            if (a.x != b.x) return a.x < b.x;
            return a.y < b.y;
        });
    eps.erase(
        std::unique(eps.begin(), eps.end(),
            [](const ExtremePoint& a, const ExtremePoint& b) {
                return a.x == b.x && a.y == b.y && a.z == b.z;
            }),
        eps.end());

    // ── Step 3: remove dominated EPs ────────────────────────────────────────
    // EP B is dominated by EP A when A.x ≤ B.x, A.y ≤ B.y, A.z ≤ B.z, A ≠ B.
    // A dominates B means A is at least as close to the origin in every axis,
    // so the sorted iteration always reaches A before B. Removing B keeps the
    // list compact without losing reachable placement positions.
    //
    // Note: interior pruning above ensures every surviving A is itself valid.
    const auto n = static_cast<int>(eps.size());
    std::vector<bool> dominated(static_cast<std::size_t>(n), false);
    for (int i = 0; i < n; ++i) {
        if (dominated[i]) continue;
        for (int j = 0; j < n; ++j) {
            if (i == j || dominated[j]) continue;
            // Does i dominate j?
            if (eps[i].x <= eps[j].x &&
                eps[i].y <= eps[j].y &&
                eps[i].z <= eps[j].z &&
                (eps[i].x < eps[j].x || eps[i].y < eps[j].y || eps[i].z < eps[j].z)) {
                dominated[j] = true;
            }
        }
    }

    std::vector<ExtremePoint> survivors;
    survivors.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (!dominated[i]) survivors.push_back(eps[i]);
    }
    eps = std::move(survivors);
}

void sortEPs(std::vector<ExtremePoint>& eps) {
    // Primary  : z ascending  — lower EPs score higher (paper: discourages columns)
    // Secondary: x²+y² ascending (integer, no sqrt) — closer to origin ranked first
    std::sort(eps.begin(), eps.end(),
        [](const ExtremePoint& a, const ExtremePoint& b) {
            if (a.z != b.z) return a.z < b.z;
            const int da = a.x * a.x + a.y * a.y;
            const int db = b.x * b.x + b.y * b.y;
            return da < db;
        });
}

bool placeItem(Container&                   cont,
               const std::vector<ItemType>& item_types,
               int                          type_idx,
               std::vector<ExtremePoint>&   eps,
               bool debug,
               bool relaxed)
{
    const ItemType& it = item_types[type_idx];
    const SupportChecker sc;  // uses Config::SUPPORT_VERTEX_INSET by default

    if (debug) {
        printf("[DBG] placeItem type=%d (l=%d w=%d h=%d), %d EPs, %d items in cont\n",
               type_idx, it.l, it.w, it.h,
               (int)eps.size(), (int)cont.items.size());
        for (int ei = 0; ei < (int)eps.size() && ei < 6; ++ei)
            printf("  EP[%d]=(%d,%d,%d)\n", ei, eps[ei].x, eps[ei].y, eps[ei].z);
    }

    for (auto it_ep = eps.begin(); it_ep != eps.end(); ++it_ep) {
        // Copy the EP coordinates before any mutation of the eps vector.
        const int ex = it_ep->x;
        const int ey = it_ep->y;
        const int ez = it_ep->z;

        for (const Orientation ori : {Orientation::Original, Orientation::Rotated90}) {
            PlacedItem pi;
            pi.item_type_index = type_idx;
            pi.orientation     = ori;
            pi.x  = ex;
            pi.y  = ey;
            pi.z  = ez;
            pi.dx = (ori == Orientation::Original) ? it.l : it.w;
            pi.dy = (ori == Orientation::Original) ? it.w : it.l;
            pi.dz = it.h;

            // Hard constraint 2: item must not exceed container bounds.
            if (!AABB::fitsInContainer(pi, cont)) {
                if (debug) printf("  EP(%d,%d,%d) ori=%d: FAIL bounds\n",ex,ey,ez,(int)ori);
                continue;
            }

            // Hard constraint 2: no volumetric collision with existing items.
            bool collides = false;
            int  coll_idx = -1;
            for (int ci2 = 0; ci2 < (int)cont.items.size(); ++ci2) {
                if (AABB::overlaps(pi, cont.items[ci2])) {
                    collides = true;
                    coll_idx = ci2;
                    break;
                }
            }
            if (collides) {
                if (debug) printf("  EP(%d,%d,%d) ori=%d dx=%d dy=%d: FAIL AABB vs item[%d]\n",
                                  ex,ey,ez,(int)ori,pi.dx,pi.dy,coll_idx);
                continue;
            }

            // Soft constraint 4 (support): EP projection guarantees a surface
            // exists below the item, but does not verify that it covers enough
            // of the base area.  SupportChecker enforces the tiered area/vertex
            // thresholds from Config.h.
            // In relaxed mode (benchmark comparison vs. Table 7) the support
            // check is skipped so packing density matches the paper's relaxed
            // results: items need only satisfy bounds + AABB non-collision.
            if (!relaxed && !sc.isSupported(pi, cont.items)) {
                if (debug) printf("  EP(%d,%d,%d) ori=%d dx=%d dy=%d: FAIL support\n",
                                  ex,ey,ez,(int)ori,pi.dx,pi.dy);
                continue;
            }

            if (debug) printf("  EP(%d,%d,%d) ori=%d dx=%d dy=%d: PLACED\n",
                              ex,ey,ez,(int)ori,pi.dx,pi.dy);

            // All checked constraints pass — commit the placement.
            cont.items.push_back(pi);

            // Remove the used EP (each EP supports exactly one placement).
            it_ep = eps.erase(it_ep);  // it_ep now points to the element after the erased one

            // Generate 3 new raw EPs, then project → prune → sort.
            // Use pruneNoDominate so that EPs above Phase 1 block surfaces are not
            // eliminated by lower-z gap EPs that happen to share the same (x,y)
            // prefix.  Interior removal (step 1) still discards EPs that land inside
            // any placed item, keeping the list compact.
            generateFrom(pi, eps);
            project(eps, cont);
            pruneNoDominate(eps, cont);
            sortEPs(eps);

            return true;
        }
    }

    // All EPs exhausted without a valid placement — caller should penalise.
    return false;
}

} // namespace ExtremePointEngine
