#pragma once

#include <vector>

#include "Config.h"
#include "Types.h"

// BlockBuilder — Phase 4, Tasks 4.3–4.10
//
// Assembles candidate Layer objects (produced by LayerGenerator) into Block
// objects that can be placed onto Container pallets.
//
//   4.3  Fill rate filtering — a layer is accepted only if the fraction of
//        its footprint area covered by items meets the per-type threshold:
//          Full / Half  ≥ Config::LAYER_MIN_FILL_FULL_HALF  (default 90 %)
//          Quarter      ≥ Config::LAYER_MIN_FILL_QUARTER     (default 85 %)
//
//   4.4  Layer merging — combine same-height quarter layers into half layers,
//        and same-height same-axis half layers into full layers.  Up to four
//        item types can appear in one merged full layer.
//
//   4.6  Block building — sort all layers by (type: Full first, then Half,
//        then Quarter) then (occupied area desc, weight desc, item_type_index).
//        Place full layers first; on height overflow open a new pallet.  Before
//        each subsequent layer, sort active pallets by remaining height ascending
//        so the most-filled pallet is tried first (maximises volume per pallet).
//        Then repeat for half layers and quarter layers with their placement rules.
//
//   4.7  Half-layer placement — the first half layer in a block goes to the
//        lower half zone; if the block is empty it goes to the "first" half
//        (left for Footprint-A, front for Footprint-B).  Each subsequent half
//        layer goes to whichever half zone is currently lower.
//
//   4.8  Quarter-layer placement — first quarter in a block goes to the lowest
//        quadrant; on an empty pallet it always goes to quadrant 0.  Each
//        subsequent quarter goes to the lowest quadrant of the four.
//
//   4.9  Hausdorff interlocking — before committing a layer on top of the
//        previous layer, four XY-symmetry variants of the new layer are tested
//        (original, H-flip, V-flip, HV-flip); the variant with the highest
//        symmetric Hausdorff distance between the two layers' corner vertices
//        is selected.  This implements Constraint 8.
//
//   4.10 Residual identification — after block building, count items packed
//        into layers per type; residual count = quantity − packed.  Compare
//        total residual volume against remaining usable block volume to decide
//        whether Phase 2 needs a new pallet.
//
// Paper ref: Ananno & Ribeiro 2024, Section IV-B-2, Figures 6–10.

namespace BlockBuilder {

// ── Task 4.3 ──────────────────────────────────────────────────────────────

// Returns true if layer.fill_rate meets the minimum threshold for its type.
// Full and Half layers require ≥ Config::LAYER_MIN_FILL_FULL_HALF.
// Quarter layers require ≥ Config::LAYER_MIN_FILL_QUARTER.
[[nodiscard]] bool passesFillRate(const Layer& layer);

// Removes every layer in `layers` whose fill_rate is below threshold.
// Modifies the vector in place.
void filterByFillRate(std::vector<Layer>& layers);

// ── Task 4.4 ──────────────────────────────────────────────────────────────

// Merges compatible layers and returns the combined set.
//
// Algorithm:
//   1. Quarter pairs of equal height → half layers (Footprint-B: L × W/2).
//      The second quarter's items are offset by (+pallet_l/2, 0).
//   2. Half pairs of equal height AND equal footprint axis → full layers.
//      Footprint-A pairs (L/2 × W): second offset by (+pallet_l/2, 0).
//      Footprint-B pairs (L × W/2): second offset by (0, +pallet_w/2).
//   3. Returns: original full layers
//              + full layers from step 2
//              + unmerged half layers (no same-axis partner found)
//              + unmerged quarter layers (odd one out in their height group)
//
// Fill rate of a merged layer = (a.fill_rate + b.fill_rate) / 2.
// This is exact because the two source layers always have equal footprint areas.
//
// The input vector is consumed (passed by value) to avoid an extra copy at
// call sites that have already built a throw-away candidate list.
[[nodiscard]] std::vector<Layer> mergeLayers(
    std::vector<Layer> layers,
    int pallet_l = Config::PALLET_L,
    int pallet_w = Config::PALLET_W);

// ── Tasks 4.6–4.9 ─────────────────────────────────────────────────────────

// buildBlocks — the main Phase 1 block-building entry point.
//
// Accepts the complete set of candidate layers for all item types (after
// filterByFillRate and mergeLayers have been applied).  Returns a vector of
// Containers whose placed_items hold every box committed during Phase 1.
//
// The algorithm places layer types in three passes:
//   Pass 1 — Full layers, sorted by (occupied_area desc, total_weight desc,
//             item_type_index asc).  Each layer tries the pallet with the
//             least remaining height that can still fit it; on failure a new
//             pallet is opened.
//   Pass 2 — Half layers, same sort key.  Placed in the lower half-zone of
//             the chosen pallet per Task 4.7.
//   Pass 3 — Quarter layers, same sort key.  Placed in the lowest quadrant
//             per Task 4.8.
//
// Hausdorff interlocking (Task 4.9) is applied at every stack step: the
// placed_items of the new layer are reordered into the 4-variant that
// maximises the Hausdorff distance from the layer below it.
//
// item_types is needed only for weight-based sorting and weight-constraint
// checking; it is not modified.
[[nodiscard]] std::vector<Container> buildBlocks(
    std::vector<Layer>              candidates,
    const std::vector<ItemType>&    item_types,
    int pallet_l      = Config::PALLET_L,
    int pallet_w      = Config::PALLET_W,
    int pallet_h      = Config::PALLET_H,
    int pallet_max_weight = Config::PALLET_MAX_WEIGHT);

// ── Task 4.10 ─────────────────────────────────────────────────────────────

// ResidualInfo — output of computeResiduals.
//
// residuals[i] = {item_type_index, remaining_count} for every type that has
// at least one item not covered by any layer placed in the containers.
// spawn_new_pallet is true when the total volume of residual items equals or
// exceeds the total remaining usable volume on the existing containers,
// meaning Phase 2 will need at least one fresh pallet.
struct ResidualInfo {
    std::vector<std::pair<int, int>> residuals;   // {type_index, count}
    bool spawn_new_pallet = false;
};

// computeResiduals — identify items that did not make it into any layer.
//
// Walks containers.placed_items, tallies packed counts per item_type_index,
// subtracts from item_types[i].q to get residual counts.
// Computes V_residual = sum(residual_volume) and
//          V_usable   = sum over containers of (pallet_l*pallet_w*(pallet_h − top_z)).
// Sets spawn_new_pallet = (V_residual >= V_usable).
[[nodiscard]] ResidualInfo computeResiduals(
    const std::vector<ItemType>&    item_types,
    const std::vector<Container>&   containers,
    int pallet_l  = Config::PALLET_L,
    int pallet_w  = Config::PALLET_W,
    int pallet_h  = Config::PALLET_H);

} // namespace BlockBuilder
