#pragma once

#include <string>
#include <vector>

#include "Config.h"
#include "Types.h"

// ItemTypeSummary is one entry in a layer's item-type breakdown.
// Used by the UI's layer manifest panel to show "Type 3 × 12, Type 7 × 4".
struct ItemTypeSummary {
    int item_type_index = 0;
    int count           = 0;
};

// LayerManifestEntry describes one horizontal layer within a Phase 1 container.
// z_min/z_max are the mm bounds of the layer. item_type_summary breaks down
// which box types appear in it and how many of each.
struct LayerManifestEntry {
    int layer_index = 0;
    int z_min       = 0;
    int z_max       = 0;
    int item_count  = 0;
    std::vector<ItemTypeSummary> item_type_summary;
};

// AnimatedPlacedItem extends the core PlacedItem with three fields needed only
// for the animated JSON output. It mirrors PlacedItem's fields directly (no
// inheritance — Rule of Zero) and adds:
//   phase           — 1 (block/layer heuristic) or 2 (EP/GA residual)
//   placement_order — 0-based position within the container; Phase 1 items
//                     occupy 0..N-1, Phase 2 items continue at N..M-1
//   layer_index     — index into the container's layer_manifest; -1 for Phase 2
//                     items (written as JSON null by JSONWriter)
struct AnimatedPlacedItem {
    int         item_type_index = 0;
    Orientation orientation     = Orientation::Original;
    int x  = 0;
    int y  = 0;
    int z  = 0;
    int dx = 0;
    int dy = 0;
    int dz = 0;

    int phase           = 1;   // 1 or 2
    int placement_order = 0;   // global within its container
    int layer_index     = -1;  // -1 = no layer (Phase 2 items)
};

// AnimatedContainer extends a pallet with per-item animation metadata and a
// layer manifest. layer_manifest is empty for containers that hold only Phase 2
// items (pure-residual pallets opened by the GA). The JSONWriter skips writing
// layer_manifest when it is empty.
struct AnimatedContainer {
    int L = Config::PALLET_L;
    int W = Config::PALLET_W;
    int H = Config::PALLET_H;

    std::vector<AnimatedPlacedItem> items;
    std::vector<LayerManifestEntry> layer_manifest;
};

// GAGenerationSnapshot records the best individual's full packing at one
// recorded generation. best_container_count and best_avg_utilization mirror
// objectives[0] and -objectives[1] of the best Individual at that generation.
// containers holds the complete packing (Phase 1 base + Phase 2 residuals);
// layer_manifest is left empty in each container — ga_history snapshots do not
// need layer breakdown, and the JSONWriter skips it when empty.
struct GAGenerationSnapshot {
    int    generation           = 0;
    int    best_container_count = 0;
    double best_avg_utilization = 0.0;
    std::vector<AnimatedContainer> containers;
};

// AnimatedSolution is the top-level structure written to solution_animated.json.
// It is built in main.cpp after both phases complete, only when --animated-output
// is passed. It does not replace or modify PackingSolution — the normal output
// path is untouched.
struct AnimatedSolution {
    std::vector<AnimatedContainer>    containers;   // all pallets, unified
    std::vector<GAGenerationSnapshot> ga_history;   // empty if GA was skipped

    int total_items       = 0;
    int phase1_item_count = 0;
    int phase2_item_count = 0;
};
