#include "Packer.h"
#include "ExtremePointEngine.h"

// ─── Implementation notes ────────────────────────────────────────────────────
//
// Container management during decode
// ────────────────────────────────────
// The decoder tracks one "active" container index.  Placement always tries the
// active container first.  If placeItem returns false (all EPs exhausted), the
// decoder advances:
//
//   1. If there is a next existing seed container, move to it (its EP list was
//      already initialised at the start of decode).
//   2. If all existing containers are exhausted, open a new empty Container,
//      initialise its EP list to {0,0,0}, and move to it.
//   3. If placeItem fails even in a brand-new empty container, the item is
//      physically larger than one pallet (infeasible for this instance).
//      out_unplaced is incremented and the item is skipped; the empty container
//      that was just opened is cleaned up so the solution stays tidy.
//
// The decoder never goes backwards — once the active index advances past a
// container, that container is considered closed for the remainder of this
// chromosome's evaluation.  The GA evolves chromosomes that make good use of
// the sequential placement order.
//
// EP list lifecycle
// ─────────────────
// Each container gets its own EP list (eps[i]).  For seed containers that
// already hold Phase 1 blocks, ExtremePointEngine::init seeds the list from
// those placed items.  For newly opened empty containers, init produces the
// single origin EP {0,0,0}.  placeItem modifies eps[active] in place after
// every successful placement (generates three new EPs, projects, prunes, sorts).

namespace Packer {

PackingSolution decode(
    const std::vector<int>&       chromosome,
    const std::vector<int>&       residualCounts,
    const std::vector<ItemType>&  itemTypes,
    const std::vector<Container>& seedContainers,
    int&                          out_unplaced,
    bool                          relaxed)
{
    out_unplaced = 0;

    PackingSolution sol;
    sol.containers = seedContainers;  // deep copy; Phase 1 blocks are preserved

    // Prototype used when Phase 2 needs to open additional pallets.  Copy dims
    // from the first seed container so new pallets match whatever pallet size
    // the caller set up (Euro pallet or BR benchmark container).
    Container container_proto;
    if (!sol.containers.empty()) {
        container_proto.L = sol.containers[0].L;
        container_proto.W = sol.containers[0].W;
        container_proto.H = sol.containers[0].H;
    }

    // Initialise one EP list per container.  Seed containers may already hold
    // Phase 1 blocks; init() generates EPs from their top surfaces and adjacent
    // edges.  An empty vector of EPs for each slot is created first so that
    // eps[i] is always valid to pass by reference.
    std::vector<std::vector<ExtremePoint>> eps(sol.containers.size());
    for (int i = 0; i < static_cast<int>(sol.containers.size()); ++i) {
        ExtremePointEngine::init(eps[i], sol.containers[i]);
    }

    // Guarantee at least one container exists so active = 0 is always valid.
    if (sol.containers.empty()) {
        sol.containers.push_back(container_proto);
        eps.emplace_back();
        ExtremePointEngine::init(eps.back(), sol.containers.back());
    }

    int active = 0;  // index of the container currently accepting placements

    for (int type_idx : chromosome) {
        const int count = residualCounts[type_idx];

        for (int item = 0; item < count; ++item) {
            bool placed     = false;
            bool just_opened = false;  // true after we open a fresh empty container

            while (!placed) {
                placed = ExtremePointEngine::placeItem(
                    sol.containers[active], itemTypes, type_idx, eps[active],
                    /*debug=*/false, relaxed);

                if (!placed) {
                    const int next = active + 1;

                    if (next < static_cast<int>(sol.containers.size())) {
                        // Advance to the next existing container; its EP list
                        // was initialised from Phase 1 blocks at the top of this
                        // function and is ready to use.
                        active      = next;
                        just_opened = false;
                    } else if (!just_opened) {
                        // All existing containers are exhausted.  Open a new
                        // empty pallet; its only EP is the origin {0,0,0}.
                        sol.containers.push_back(container_proto);
                        eps.emplace_back();
                        ExtremePointEngine::init(eps.back(), sol.containers.back());
                        active      = next;
                        just_opened = true;
                        // Loop continues: placeItem will be tried again on the
                        // fresh container in the next while-iteration.
                    } else {
                        // just_opened is true: we already tried a brand-new empty
                        // container and it also failed.  The item is larger than
                        // one full pallet — infeasible for this instance.
                        // Clean up the empty container so the solution stays tidy.
                        sol.containers.pop_back();
                        eps.pop_back();
                        --active;
                        ++out_unplaced;
                        break;  // move on to the next item
                    }
                }
            }
        }
    }

    return sol;
}

} // namespace Packer
