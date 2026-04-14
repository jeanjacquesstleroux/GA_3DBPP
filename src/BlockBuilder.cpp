#include "BlockBuilder.h"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "Hausdorff.h"

// ---------------------------------------------------------------------------
// Internal helpers — not exposed in the header
// ---------------------------------------------------------------------------

// Maximum X extent (right edge) of all placed items in a layer.
// Used to identify the footprint axis of half layers.
static int maxExtentX(const Layer& layer) {
    int mx = 0;
    for (const PlacedItem& pi : layer.placed_items) {
        mx = std::max(mx, pi.x + pi.dx);
    }
    return mx;
}

// A half layer is Footprint-A (covers L/2 × W) when all items stay within
// the left half of the pallet — i.e. max X extent ≤ pallet_l / 2.
// If max X extent > pallet_l / 2, the layer is Footprint-B (L × W/2).
static bool isFootprintA(const Layer& layer, int pallet_l) {
    return maxExtentX(layer) <= pallet_l / 2;
}

// Build a merged layer from two source layers.
//
//   a            — first source (items used as-is)
//   b            — second source (items shifted by offset_x / offset_y)
//   result_type  — LayerType of the merged layer (Half or Full)
//
// Fill rate of the merged layer = (a.fill_rate + b.fill_rate) / 2.
// This formula is exact when the two source footprint areas are equal, which
// is always the case for the three merge scenarios used here:
//   quarter + quarter  (both L/2 × W/2)
//   half-A  + half-A   (both L/2 × W)
//   half-B  + half-B   (both L   × W/2)
//
// item_type_index on the merged layer is set to a's value; it serves only as
// a sort key in Task 4.6.  The individual PlacedItems carry their own
// item_type_index values, which are the authoritative source of truth.
static Layer makeMerged(const Layer& a, const Layer& b,
                        int offset_x, int offset_y,
                        LayerType result_type) {
    Layer merged;
    merged.item_type_index = a.item_type_index;
    merged.type            = result_type;
    merged.height          = a.height;
    merged.item_count      = a.item_count + b.item_count;
    merged.fill_rate       = (a.fill_rate + b.fill_rate) / 2.0;

    merged.placed_items.reserve(merged.item_count);

    // Copy a's items unchanged.
    for (const PlacedItem& pi : a.placed_items) {
        merged.placed_items.push_back(pi);
    }
    // Copy b's items with the placement offset applied.
    for (PlacedItem pi : b.placed_items) {
        pi.x += offset_x;
        pi.y += offset_y;
        merged.placed_items.push_back(pi);
    }
    return merged;
}

// Greedily pair adjacent elements of a layer group and merge them.
// Odd layer (if any) is left in `unmerged`.
// Returns the newly created merged layers.
static std::vector<Layer> pairAndMerge(std::vector<Layer>& group,
                                       std::vector<Layer>& unmerged,
                                       int offset_x, int offset_y,
                                       LayerType result_type) {
    std::vector<Layer> merged_out;
    // Iterate over pairs (0,1), (2,3), ...
    for (std::size_t i = 0; i + 1 < group.size(); i += 2) {
        merged_out.push_back(
            makeMerged(group[i], group[i + 1], offset_x, offset_y, result_type));
    }
    // If the group has an odd count, the last element has no partner.
    if (group.size() % 2 != 0) {
        unmerged.push_back(std::move(group.back()));
    }
    return merged_out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool BlockBuilder::passesFillRate(const Layer& layer) {
    switch (layer.type) {
        case LayerType::Full:
        case LayerType::Half:
            return layer.fill_rate >= Config::LAYER_MIN_FILL_FULL_HALF;
        case LayerType::Quarter:
            return layer.fill_rate >= Config::LAYER_MIN_FILL_QUARTER;
    }
    return false; // unreachable; satisfies -Wreturn-type
}

void BlockBuilder::filterByFillRate(std::vector<Layer>& layers) {
    layers.erase(
        std::remove_if(layers.begin(), layers.end(),
            [](const Layer& l) { return !passesFillRate(l); }),
        layers.end());
}

std::vector<Layer> BlockBuilder::mergeLayers(std::vector<Layer> layers,
                                              int pallet_l,
                                              int pallet_w) {
    std::vector<Layer> full_layers;
    std::vector<Layer> half_layers;
    std::vector<Layer> quarter_layers;
    std::vector<Layer> unmerged_halves;
    std::vector<Layer> unmerged_quarters;

    // ── Partition by type ────────────────────────────────────────────────
    for (Layer& l : layers) {
        switch (l.type) {
            case LayerType::Full:    full_layers.push_back(std::move(l));    break;
            case LayerType::Half:    half_layers.push_back(std::move(l));    break;
            case LayerType::Quarter: quarter_layers.push_back(std::move(l)); break;
        }
    }

    // ── Step 1: Merge quarter pairs into half layers ─────────────────────
    // Group quarters by height.  Within each height group, pair them greedily.
    // Offset: second quarter shifted +pallet_l/2 along X.
    // Result footprint: L × W/2 (Footprint-B half).
    {
        // key = item height (mm)
        std::map<int, std::vector<Layer>> by_height;
        for (Layer& q : quarter_layers) {
            by_height[q.height].push_back(std::move(q));
        }

        for (auto& [height, group] : by_height) {
            auto new_halves = pairAndMerge(group, unmerged_quarters,
                                           pallet_l / 2, 0,
                                           LayerType::Half);
            for (Layer& h : new_halves) {
                half_layers.push_back(std::move(h));
            }
        }
    }

    // ── Step 2: Merge half pairs into full layers ────────────────────────
    // Group halves by (height, footprint_axis).
    // Footprint-A pairs: second shifted +pallet_l/2 along X.
    // Footprint-B pairs: second shifted +pallet_w/2 along Y.
    {
        // key = { height, is_footprint_A }
        using Key = std::pair<int, bool>;
        std::map<Key, std::vector<Layer>> by_key;
        for (Layer& h : half_layers) {
            bool fp_a = isFootprintA(h, pallet_l);
            by_key[{h.height, fp_a}].push_back(std::move(h));
        }

        for (auto& [key, group] : by_key) {
            const bool fp_a = key.second;
            const int  ox   = fp_a ? pallet_l / 2 : 0;
            const int  oy   = fp_a ? 0             : pallet_w / 2;

            auto new_fulls = pairAndMerge(group, unmerged_halves,
                                          ox, oy,
                                          LayerType::Full);
            for (Layer& f : new_fulls) {
                full_layers.push_back(std::move(f));
            }
        }
    }

    // ── Assemble result ──────────────────────────────────────────────────
    // Order: full layers (original + merged), then unmerged halves,
    // then unmerged quarters.  BlockBuilder Task 4.6 will sort by
    // (occupied area, weight, item type) before stacking.
    std::vector<Layer> result;
    result.reserve(full_layers.size() +
                   unmerged_halves.size() +
                   unmerged_quarters.size());

    for (Layer& l : full_layers)        result.push_back(std::move(l));
    for (Layer& l : unmerged_halves)    result.push_back(std::move(l));
    for (Layer& l : unmerged_quarters)  result.push_back(std::move(l));

    return result;
}

// ===========================================================================
// buildBlocks and helpers
// ===========================================================================

// ---------------------------------------------------------------------------
// PalletState — tracks the height frontier of the 4 quadrants of one pallet.
//
// The pallet footprint is split into 4 equal quadrants numbered 0-3:
//   q[0] = [0,   L/2) × [0,   W/2)   near-left
//   q[1] = [L/2, L  ) × [0,   W/2)   far-left
//   q[2] = [0,   L/2) × [W/2, W  )   near-right
//   q[3] = [L/2, L  ) × [W/2, W  )   far-right
//
// Invariant maintained by this module:
//   After every full-layer placement:  q[0]==q[1]==q[2]==q[3]
//   After every Footprint-A half:      q[0]==q[2]  AND  q[1]==q[3]
//   After every Footprint-B half:      q[0]==q[1]  AND  q[2]==q[3]
//
// These invariants ensure every layer sits on a flat surface.
// ---------------------------------------------------------------------------
struct PalletState {
    int container_index = -1;
    std::array<int, 4> q = {0, 0, 0, 0};  // quadrant top-z heights (mm)
    int max_h            = 0;

    // Height of the tallest quadrant.
    [[nodiscard]] int topZ() const {
        return *std::max_element(q.begin(), q.end());
    }

    // Remaining height above the tallest quadrant.
    [[nodiscard]] int remaining() const { return max_h - topZ(); }
};

// ---------------------------------------------------------------------------
// Quadrant index sets for each half and each quarter position.
//
// Footprint-A half layers span pallet_l/2 × pallet_w (vertical split):
//   halfA_left  → q[0] and q[2]  (x in [0, L/2))
//   halfA_right → q[1] and q[3]  (x in [L/2, L))
//
// Footprint-B half layers span pallet_l × pallet_w/2 (horizontal split):
//   halfB_front → q[0] and q[1]  (y in [0, W/2))
//   halfB_back  → q[2] and q[3]  (y in [W/2, W))
// ---------------------------------------------------------------------------

// Returns true if the layer is a Footprint-A half (max x-extent ≤ pallet_l/2).
static bool isHalfA(const Layer& layer, int pallet_l) {
    int mx = 0;
    for (const PlacedItem& pi : layer.placed_items) {
        mx = std::max(mx, pi.x + pi.dx);
    }
    return mx <= pallet_l / 2;
}

// ---------------------------------------------------------------------------
// Hausdorff interlocking helpers (Task 4.9)
//
// collectTopCorners: returns the 4 XY corner positions of the top face of
//   every item in a layer.  The z coordinate is ignored — only XY matters for
//   the misalignment measure.
//
// flipLayer: returns a copy of `layer` with item positions mirrored.
//   flip_x=true → x' = footprint_l - x - dx  (horizontal mirror)
//   flip_y=true → y' = footprint_w - y - dy  (vertical mirror)
// ---------------------------------------------------------------------------

static std::vector<std::array<int, 2>>
collectTopCorners(const std::vector<PlacedItem>& items) {
    std::vector<std::array<int, 2>> pts;
    pts.reserve(items.size() * 4);
    for (const PlacedItem& pi : items) {
        pts.push_back({pi.x,          pi.y         });
        pts.push_back({pi.x + pi.dx,  pi.y         });
        pts.push_back({pi.x,          pi.y + pi.dy });
        pts.push_back({pi.x + pi.dx,  pi.y + pi.dy });
    }
    return pts;
}

static Layer flipLayer(Layer layer, bool flip_x, bool flip_y,
                       int footprint_l, int footprint_w) {
    for (PlacedItem& pi : layer.placed_items) {
        if (flip_x) pi.x = footprint_l - pi.x - pi.dx;
        if (flip_y) pi.y = footprint_w - pi.y - pi.dy;
    }
    return layer;
}

// bestInterlockVariant — Task 4.9.
//
// Given the layer already placed below (`prev`) and the candidate `next`,
// returns the symmetry variant of `next` (among 4) that maximises the
// symmetric Hausdorff distance between the two layers' corner vertices.
// footprint_l/w are the dimensions of the zone both layers occupy.
static Layer bestInterlockVariant(const Layer& prev, Layer next,
                                  int footprint_l, int footprint_w) {
    const auto prev_corners = collectTopCorners(prev.placed_items);

    Layer best  = next;
    double best_d = Hausdorff::distance(prev_corners,
                                         collectTopCorners(next.placed_items));

    // Try the other 3 flips; keep the one with the largest Hausdorff distance.
    const bool flips[3][2] = {{true, false}, {false, true}, {true, true}};
    for (const auto& [fx, fy] : flips) {
        Layer variant = flipLayer(next, fx, fy, footprint_l, footprint_w);
        double d = Hausdorff::distance(prev_corners,
                                        collectTopCorners(variant.placed_items));
        if (d > best_d) {
            best_d = d;
            best   = std::move(variant);
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// commitLayer — write all PlacedItems from `layer` into `cont`, applying
//   an XY offset (for half/quarter placement) and setting absolute z.
// Also returns the layer with absolute z written into placed_items (used
// by the next Hausdorff call as `prev`).
// ---------------------------------------------------------------------------
static Layer commitLayer(Container& cont, Layer layer,
                          int offset_x, int offset_y, int z_base) {
    for (PlacedItem& pi : layer.placed_items) {
        pi.x += offset_x;
        pi.y += offset_y;
        pi.z  = z_base;
        cont.items.push_back(pi);
    }
    return layer;  // returned for use as prev-layer in next Hausdorff call
}

// ---------------------------------------------------------------------------
// layerWeight — total mass of all items in a layer (needs item_types).
// ---------------------------------------------------------------------------
static int layerWeight(const Layer& layer, const std::vector<ItemType>& types) {
    int w = 0;
    for (const PlacedItem& pi : layer.placed_items) {
        w += types[pi.item_type_index].m;
    }
    return w;
}

// ---------------------------------------------------------------------------
// sortKey — occupied_area descending, then weight descending, then
//           item_type_index ascending.  Used for all three passes.
//
// occupied_area = item_count × item_dx × item_dy
//   We compute it directly from the first placed item to avoid re-deriving
//   footprint dimensions.
// ---------------------------------------------------------------------------
static void sortLayers(std::vector<Layer>& layers,
                        const std::vector<ItemType>& types) {
    std::sort(layers.begin(), layers.end(),
        [&](const Layer& a, const Layer& b) {
            // Occupied area for a
            int area_a = 0, area_b = 0;
            if (!a.placed_items.empty()) {
                const PlacedItem& pi = a.placed_items[0];
                area_a = a.item_count * pi.dx * pi.dy;
            }
            if (!b.placed_items.empty()) {
                const PlacedItem& pi = b.placed_items[0];
                area_b = b.item_count * pi.dx * pi.dy;
            }
            if (area_a != area_b) return area_a > area_b;  // descending

            int w_a = layerWeight(a, types);
            int w_b = layerWeight(b, types);
            if (w_a != w_b) return w_a > w_b;              // descending

            return a.item_type_index < b.item_type_index;  // ascending
        });
}

// ===========================================================================
// Public: buildBlocks (Tasks 4.6–4.9)
// ===========================================================================

std::vector<Container> BlockBuilder::buildBlocks(
    std::vector<Layer>           candidates,
    const std::vector<ItemType>& item_types,
    int pallet_l,
    int pallet_w,
    int pallet_h,
    int pallet_max_weight)
{
    (void)pallet_max_weight;  // reserved for future weight-constraint checking
    std::vector<Container> containers;
    std::vector<PalletState> states;

    // Lambda: open a new empty pallet and return its index.
    auto newPallet = [&]() -> int {
        int idx = static_cast<int>(containers.size());
        Container c;
        c.L = pallet_l; c.W = pallet_w; c.H = pallet_h;
        containers.push_back(std::move(c));
        PalletState ps;
        ps.container_index = idx;
        ps.max_h           = pallet_h;
        states.push_back(ps);
        return idx;
    };

    // Lambda: sort active pallets by remaining height ascending so the most-
    // filled pallet is tried first (maximises volume use per pallet).
    // Paper: "existing blocks are sorted by remaining available packing height
    // in ascending order" before each subsequent layer placement.
    auto sortPallets = [&]() {
        std::sort(states.begin(), states.end(),
            [](const PalletState& a, const PalletState& b) {
                return a.remaining() < b.remaining();
            });
    };

    // ── Helper: find the last committed layer on the given quadrant set.
    // We need it for Hausdorff interlocking — "what was placed here before?"
    // We reconstruct it from the container's placed_items at the current z.
    // Returns a synthetic Layer with only the placed_items at z == top_z.
    auto topLayerAt = [&](const Container& cont, int top_z) -> Layer {
        Layer prev;
        for (const PlacedItem& pi : cont.items) {
            if (pi.z + pi.dz == top_z) {  // item whose top face is at top_z
                prev.placed_items.push_back(pi);
            }
        }
        return prev;
    };

    // ── Partition candidates into three passes ──────────────────────────────
    std::vector<Layer> full_pass, half_pass, quarter_pass;
    for (Layer& l : candidates) {
        switch (l.type) {
            case LayerType::Full:    full_pass.push_back(std::move(l));    break;
            case LayerType::Half:    half_pass.push_back(std::move(l));    break;
            case LayerType::Quarter: quarter_pass.push_back(std::move(l)); break;
        }
    }

    sortLayers(full_pass,    item_types);
    sortLayers(half_pass,    item_types);
    sortLayers(quarter_pass, item_types);

    // ── Quantity tracking: never place more items than ItemType::q ───────────
    std::vector<int> remaining(item_types.size());
    for (int i = 0; i < (int)item_types.size(); ++i)
        remaining[i] = item_types[i].q;

    // canCommit: true iff every item type in `layer` has enough stock left.
    auto canCommit = [&](const Layer& layer) -> bool {
        std::vector<int> needed(remaining.size(), 0);
        for (const PlacedItem& pi : layer.placed_items)
            ++needed[pi.item_type_index];
        for (int i = 0; i < (int)needed.size(); ++i)
            if (needed[i] > remaining[i]) return false;
        return true;
    };

    // doCommit: deduct placed item counts from remaining after a commit.
    auto doCommit = [&](const Layer& layer) {
        for (const PlacedItem& pi : layer.placed_items)
            --remaining[pi.item_type_index];
    };

    // ── Pass 1: Full layers ─────────────────────────────────────────────────
    if (!full_pass.empty()) newPallet();

    for (Layer& layer : full_pass) {
        if (!canCommit(layer)) continue;  // skip: insufficient remaining stock
        sortPallets();
        bool placed = false;
        for (PalletState& ps : states) {
            // A full layer requires all 4 quadrants to be at the same height.
            int z = ps.q[0];
            if (ps.q[0] != ps.q[1] || ps.q[0] != ps.q[2] || ps.q[0] != ps.q[3])
                continue;  // uneven surface — skip this pallet
            if (z + layer.height > ps.max_h) continue;  // would exceed max height

            Container& cont = containers[ps.container_index];

            // Hausdorff interlocking: if there's a previous layer here, pick
            // the best flip variant.
            Layer to_place = layer;
            if (z > 0) {
                Layer prev = topLayerAt(cont, z);
                if (!prev.placed_items.empty()) {
                    to_place = bestInterlockVariant(prev, to_place,
                                                    pallet_l, pallet_w);
                }
            }

            commitLayer(cont, to_place, 0, 0, z);
            doCommit(to_place);
            for (int i = 0; i < 4; ++i) ps.q[i] = z + layer.height;
            placed = true;
            break;
        }
        if (!placed) {
            int idx = newPallet();
            sortPallets();
            PalletState& ps = states.back();  // the new pallet has max remaining
            // Find newly added state (it's the one with container_index == idx)
            for (PalletState& s : states) {
                if (s.container_index == idx) {
                    commitLayer(containers[idx], layer, 0, 0, 0);
                    doCommit(layer);
                    for (int i = 0; i < 4; ++i) s.q[i] = layer.height;
                    break;
                }
            }
            (void)ps;
        }
    }

    // ── Pass 2: Half layers ─────────────────────────────────────────────────
    for (Layer& layer : half_pass) {
        if (!canCommit(layer)) continue;
        sortPallets();
        bool placed = false;

        for (PalletState& ps : states) {
            Container& cont = containers[ps.container_index];
            bool fp_a = isHalfA(layer, pallet_l);

            // Determine which half-zones exist and their current z heights.
            // Footprint-A: zones are (q[0],q[2]) and (q[1],q[3])
            // Footprint-B: zones are (q[0],q[1]) and (q[2],q[3])
            int z0, z1;      // z heights of "zone 0" and "zone 1"
            int ox0, oy0;    // XY offset to apply when placing in zone 0
            int ox1, oy1;    // XY offset to apply when placing in zone 1
            // which q-indices belong to each zone
            std::array<int,2> qi0, qi1;

            if (fp_a) {
                z0 = ps.q[0];  // invariant: q[0]==q[2]
                z1 = ps.q[1];  // invariant: q[1]==q[3]
                ox0 = 0;          oy0 = 0;
                ox1 = pallet_l/2; oy1 = 0;
                qi0 = {0, 2};
                qi1 = {1, 3};
            } else {
                z0 = ps.q[0];  // invariant: q[0]==q[1]
                z1 = ps.q[2];  // invariant: q[2]==q[3]
                ox0 = 0; oy0 = 0;
                ox1 = 0; oy1 = pallet_w/2;
                qi0 = {0, 1};
                qi1 = {2, 3};
            }

            // Choose the lower zone; on tie prefer zone 0 ("first" half).
            bool use_zone0 = (z0 <= z1);
            int z_place = use_zone0 ? z0 : z1;
            int ox      = use_zone0 ? ox0 : ox1;
            int oy      = use_zone0 ? oy0 : oy1;
            const std::array<int,2>& qi = use_zone0 ? qi0 : qi1;

            if (z_place + layer.height > ps.max_h) continue;  // won't fit

            // Hausdorff interlocking against the layer already at this z.
            Layer to_place = layer;
            if (z_place > 0) {
                Layer prev = topLayerAt(cont, z_place);
                if (!prev.placed_items.empty()) {
                    int fp_l = fp_a ? pallet_l / 2 : pallet_l;
                    int fp_w = fp_a ? pallet_w     : pallet_w / 2;
                    to_place = bestInterlockVariant(prev, to_place, fp_l, fp_w);
                }
            }

            commitLayer(cont, to_place, ox, oy, z_place);
            doCommit(to_place);
            for (int qi_idx : qi) ps.q[qi_idx] = z_place + layer.height;
            placed = true;
            break;
        }

        if (!placed) {
            // No existing pallet can fit this half layer — open a new one.
            int idx = newPallet();
            PalletState* ps_ptr = nullptr;
            for (PalletState& s : states) {
                if (s.container_index == idx) { ps_ptr = &s; break; }
            }
            if (ps_ptr) {
                bool fp_a = isHalfA(layer, pallet_l);
                int ox = 0, oy = 0;
                std::array<int,2> qi = fp_a ? std::array<int,2>{0,2}
                                             : std::array<int,2>{0,1};
                commitLayer(containers[idx], layer, ox, oy, 0);
                doCommit(layer);
                for (int qi_idx : qi) ps_ptr->q[qi_idx] = layer.height;
            }
        }
    }

    // ── Pass 3: Quarter layers ──────────────────────────────────────────────
    for (Layer& layer : quarter_pass) {
        if (!canCommit(layer)) continue;
        sortPallets();
        bool placed = false;

        for (PalletState& ps : states) {
            // Find the quadrant with the minimum current z.
            int min_q = 0;
            for (int i = 1; i < 4; ++i) {
                if (ps.q[i] < ps.q[min_q]) min_q = i;
            }

            // If this is the first layer on an empty pallet, force quadrant 0.
            bool block_empty = (ps.q[0] == 0 && ps.q[1] == 0 &&
                                ps.q[2] == 0 && ps.q[3] == 0);
            if (block_empty) min_q = 0;

            int z_place = ps.q[min_q];
            if (z_place + layer.height > ps.max_h) continue;

            // XY offsets for each quadrant:
            //  q[0]: (0,          0         )
            //  q[1]: (pallet_l/2, 0         )
            //  q[2]: (0,          pallet_w/2)
            //  q[3]: (pallet_l/2, pallet_w/2)
            const int ox_map[4] = {0, pallet_l/2, 0,          pallet_l/2};
            const int oy_map[4] = {0, 0,          pallet_w/2, pallet_w/2};

            Container& cont = containers[ps.container_index];

            Layer to_place = layer;
            if (z_place > 0) {
                Layer prev = topLayerAt(cont, z_place);
                if (!prev.placed_items.empty()) {
                    to_place = bestInterlockVariant(prev, to_place,
                                                    pallet_l/2, pallet_w/2);
                }
            }

            commitLayer(cont, to_place,
                        ox_map[min_q], oy_map[min_q], z_place);
            doCommit(to_place);
            ps.q[min_q] = z_place + layer.height;
            placed = true;
            break;
        }

        if (!placed) {
            int idx = newPallet();
            PalletState* ps_ptr = nullptr;
            for (PalletState& s : states) {
                if (s.container_index == idx) { ps_ptr = &s; break; }
            }
            if (ps_ptr) {
                // First layer on empty pallet → quadrant 0.
                commitLayer(containers[idx], layer, 0, 0, 0);
                doCommit(layer);
                ps_ptr->q[0] = layer.height;
            }
        }
    }

    return containers;
}

// ===========================================================================
// Task 4.10: computeResiduals
// ===========================================================================

BlockBuilder::ResidualInfo BlockBuilder::computeResiduals(
    const std::vector<ItemType>& item_types,
    const std::vector<Container>& containers,
    int pallet_l,
    int pallet_w,
    int pallet_h)
{
    // Count how many items of each type were placed across all containers.
    std::vector<int> packed(item_types.size(), 0);
    for (const Container& cont : containers) {
        for (const PlacedItem& pi : cont.items) {
            ++packed[pi.item_type_index];
        }
    }

    // Build residual list: types with leftover items.
    ResidualInfo info;
    long long v_residual = 0;
    for (int i = 0; i < static_cast<int>(item_types.size()); ++i) {
        int remaining = item_types[i].q - packed[i];
        if (remaining > 0) {
            info.residuals.emplace_back(i, remaining);
            v_residual += static_cast<long long>(item_types[i].volume()) * remaining;
        }
    }

    // Compute usable volume remaining on existing containers.
    // For each container, usable = (pallet_h - top_z) × pallet_l × pallet_w.
    // top_z = max z+dz across all placed items on that container.
    long long v_usable = 0;
    for (const Container& cont : containers) {
        int top_z = 0;
        for (const PlacedItem& pi : cont.items) {
            top_z = std::max(top_z, pi.z + pi.dz);
        }
        v_usable += static_cast<long long>(pallet_l) * pallet_w * (pallet_h - top_z);
    }

    info.spawn_new_pallet = (v_residual >= v_usable);
    return info;
}
