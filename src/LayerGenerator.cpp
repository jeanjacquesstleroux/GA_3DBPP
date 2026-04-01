#include "LayerGenerator.h"

// ---------------------------------------------------------------------------
// Internal helpers (not exposed in the header)
// ---------------------------------------------------------------------------

// shiftedPositions — Task 4.2: dynamic shifting.
//
// Given N items of dimension d fit along an axis of length F, compute the
// mm start-position of every item so that:
//   - item[0]   starts at 0         (flush with near wall)
//   - item[N-1] starts at F - d     (flush with far wall)
//   - interior items are evenly spaced between the two
//
// This distributes unused space (F - N*d) as gaps *between* items, pushing
// each item toward the nearest pallet extremity.  The result is a symmetric
// load distribution and better interlocking between adjacent layers.
//
// Edge cases:
//   N == 0: returns empty vector
//   N == 1: returns {0}  (nowhere to shift; item stays at near wall)
//
static std::vector<int> shiftedPositions(int N, int d, int F) {
    if (N <= 0) return {};
    if (N == 1) return {0};

    std::vector<int> pos(N);
    pos[0]     = 0;
    pos[N - 1] = F - d;

    // Interior items: evenly interpolate between pos[0] and pos[N-1].
    // Using integer arithmetic with rounding to avoid fp drift accumulating
    // across many items.
    for (int i = 1; i < N - 1; ++i) {
        // Cast to double for division; round back to int.
        pos[i] = static_cast<int>(
            static_cast<double>(i) * (F - d) / (N - 1) + 0.5);
    }
    return pos;
}

// buildLayer — construct a Layer from a precomputed grid.
//
// footprint_l, footprint_w: the region being filled (mm).
// item_dx, item_dy: effective item dimensions after orientation (mm).
// item_dz: item height (unchanged by Z rotation).
// orientation: which rotation was applied.
// item_type_index, type: forwarded to the Layer struct.
//
static Layer buildLayer(const ItemType&   item,
                        int               item_type_index,
                        LayerType         type,
                        Orientation       orientation,
                        int               item_dx,
                        int               item_dy,
                        int               footprint_l,
                        int               footprint_w) {
    const int items_x = footprint_l / item_dx;
    const int items_y = footprint_w / item_dy;
    const int count   = items_x * items_y;

    Layer layer;
    layer.item_type_index = item_type_index;
    layer.type            = type;
    layer.item_count      = count;
    layer.height          = item.h;          // dz is always the original height

    const double footprint_area = static_cast<double>(footprint_l) * footprint_w;
    layer.fill_rate = (footprint_area > 0.0)
        ? (static_cast<double>(count) * item_dx * item_dy) / footprint_area
        : 0.0;

    if (count == 0) return layer;

    // Compute dynamic-shifted X and Y positions (Task 4.2).
    const std::vector<int> xs = shiftedPositions(items_x, item_dx, footprint_l);
    const std::vector<int> ys = shiftedPositions(items_y, item_dy, footprint_w);

    // Build every PlacedItem in the grid.  z = 0 here; BlockBuilder writes
    // the absolute z coordinate when assembling layers into blocks.
    layer.placed_items.reserve(count);
    for (int xi = 0; xi < items_x; ++xi) {
        for (int yi = 0; yi < items_y; ++yi) {
            PlacedItem pi;
            pi.item_type_index = item_type_index;
            pi.orientation     = orientation;
            pi.x               = xs[xi];
            pi.y               = ys[yi];
            pi.z               = 0;
            pi.dx              = item_dx;
            pi.dy              = item_dy;
            pi.dz              = item.h;
            layer.placed_items.push_back(pi);
        }
    }
    return layer;
}

// bestOfTwo — Task 4.1: compare two candidate layers and return the better one.
//
// The layer that fits more items wins.  On a tie, the Original-orientation
// layer is preferred (whichever is passed as 'orig').
//
static Layer bestOfTwo(Layer orig, Layer rotated) {
    return (rotated.item_count > orig.item_count) ? std::move(rotated)
                                                  : std::move(orig);
}

// ---------------------------------------------------------------------------
// Public API — LayerGenerator namespace
// ---------------------------------------------------------------------------

Layer LayerGenerator::generateFull(const ItemType& item,
                                   int             item_type_index,
                                   int             pallet_l,
                                   int             pallet_w) {
    // Original orientation: dx = l, dy = w
    Layer orig = buildLayer(item, item_type_index, LayerType::Full,
                            Orientation::Original,
                            item.l, item.w,
                            pallet_l, pallet_w);

    // Rotated 90°: dx = w, dy = l  (Z-axis rotation swaps l and w)
    Layer rot  = buildLayer(item, item_type_index, LayerType::Full,
                            Orientation::Rotated90,
                            item.w, item.l,
                            pallet_l, pallet_w);

    return bestOfTwo(std::move(orig), std::move(rot));
}

std::vector<Layer> LayerGenerator::generateHalves(const ItemType& item,
                                                   int             item_type_index,
                                                   int             pallet_l,
                                                   int             pallet_w) {
    // Two half-footprint shapes:
    //   A: (pallet_l/2) × pallet_w   — left half
    //   B:  pallet_l    × (pallet_w/2)— bottom half
    // Each tried in both orientations → up to 4 candidates returned.
    // Integer division (/) truncates — correct behaviour for fitting boxes.

    const int half_l = pallet_l / 2;
    const int half_w = pallet_w / 2;

    std::vector<Layer> results;
    results.reserve(4);

    // Footprint A: half_l × pallet_w
    {
        Layer a_orig = buildLayer(item, item_type_index, LayerType::Half,
                                  Orientation::Original,
                                  item.l, item.w,
                                  half_l, pallet_w);
        Layer a_rot  = buildLayer(item, item_type_index, LayerType::Half,
                                  Orientation::Rotated90,
                                  item.w, item.l,
                                  half_l, pallet_w);
        if (a_orig.item_count > 0) results.push_back(std::move(a_orig));
        if (a_rot.item_count  > 0) results.push_back(std::move(a_rot));
    }

    // Footprint B: pallet_l × half_w
    {
        Layer b_orig = buildLayer(item, item_type_index, LayerType::Half,
                                  Orientation::Original,
                                  item.l, item.w,
                                  pallet_l, half_w);
        Layer b_rot  = buildLayer(item, item_type_index, LayerType::Half,
                                  Orientation::Rotated90,
                                  item.w, item.l,
                                  pallet_l, half_w);
        if (b_orig.item_count > 0) results.push_back(std::move(b_orig));
        if (b_rot.item_count  > 0) results.push_back(std::move(b_rot));
    }

    return results;
}

std::vector<Layer> LayerGenerator::generateQuarters(const ItemType& item,
                                                     int             item_type_index,
                                                     int             pallet_l,
                                                     int             pallet_w) {
    // One quarter-footprint: (pallet_l/2) × (pallet_w/2).
    // Both orientations tried → up to 2 candidates returned.

    const int half_l = pallet_l / 2;
    const int half_w = pallet_w / 2;

    std::vector<Layer> results;
    results.reserve(2);

    Layer orig = buildLayer(item, item_type_index, LayerType::Quarter,
                            Orientation::Original,
                            item.l, item.w,
                            half_l, half_w);
    Layer rot  = buildLayer(item, item_type_index, LayerType::Quarter,
                            Orientation::Rotated90,
                            item.w, item.l,
                            half_l, half_w);

    if (orig.item_count > 0) results.push_back(std::move(orig));
    if (rot.item_count  > 0) results.push_back(std::move(rot));

    return results;
}
