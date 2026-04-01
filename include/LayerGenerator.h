#pragma once

#include <vector>

#include "Config.h"
#include "Types.h"

// LayerGenerator — Phase 4, Tasks 4.1–4.5
//
// Generates candidate Layer objects for a single item type across three pallet
// footprint sizes:
//
//   Full    = pallet_l  × pallet_w        (entire pallet floor)
//   Half    = pallet_l/2 × pallet_w  OR
//             pallet_l  × pallet_w/2      (one of two half-regions)
//   Quarter = pallet_l/2 × pallet_w/2    (one quadrant)
//
// For each footprint the item is tried in both orientations (Original and
// Rotated90).  The item orientation that fits more boxes wins; on a tie the
// Original orientation is kept.
//
// Dynamic shifting (Task 4.2) is applied inside every generator: leftover space
// along each axis is distributed so that boxes are pushed toward the pallet
// extremities, leaving the gap in the centre.  This improves stability and
// interlocking between adjacent layers.
//
// Fill rate = (item_count × item_dx × item_dy) / footprint_area.
// The LayerType field on the returned Layer is set here; threshold filtering
// is the caller's responsibility (see BlockBuilder, Task 4.5).
//
// Paper ref: Ananno & Ribeiro 2024, Section IV-B-2, Figures 6–10.

namespace LayerGenerator {

// Generate one full-pallet layer for a single item type.
// Both orientations are tried; the orientation that fits more items is used.
// On a tie the Original orientation is kept.
// placed_items are positioned in [0, pallet_l) × [0, pallet_w) with z = 0.
[[nodiscard]] Layer generateFull(const ItemType& item,
                                 int             item_type_index,
                                 int             pallet_l = Config::PALLET_L,
                                 int             pallet_w = Config::PALLET_W);

// Generate up to 4 half-pallet candidate layers for a single item type.
// Two footprint-------------------------------------------------------s are tried:
//   A) (pallet_l/2) × pallet_w   — left-half region
//   B) pallet_l × (pallet_w/2)   — bottom-half region
// Each footprint is tried with both item orientations, giving up to 4 Layer
// objects.  placed_items are in [0, footprint_l) × [0, footprint_w) with z = 0.
// The caller (BlockBuilder) adds the XY offset when placing on the pallet.
[[nodiscard]] std::vector<Layer> generateHalves(const ItemType& item,
                                                int             item_type_index,
                                                int             pallet_l = Config::PALLET_L,
                                                int             pallet_w = Config::PALLET_W);

// Generate up to 2 quarter-pallet candidate layers for a single item type.
// One footprint: (pallet_l/2) × (pallet_w/2).
// Both item orientations are tried → up to 2 Layer objects.
// placed_items are in [0, pallet_l/2) × [0, pallet_w/2) with z = 0.
[[nodiscard]] std::vector<Layer> generateQuarters(const ItemType& item,
                                                  int             item_type_index,
                                                  int             pallet_l = Config::PALLET_L,
                                                  int             pallet_w = Config::PALLET_W);

} // namespace LayerGenerator
