#pragma once

#include <array>
#include <string>
#include <vector>

#include "Config.h"

// Unit describes the measurement system used for a pallet's native specification.
// Internally the algorithm always works in millimeters; conversion happens at I/O
// boundaries (CSV reader, BR parser) before any ItemType is constructed.
enum class Unit { Millimeters, Inches };

// ItemType represents a kind of box — its physical dimensions, mass, and quantity.
// It maps to the paper's item tuple [l, w, h, m, v, q].
// Note: volume is not stored as a field; it is computed by volume() to avoid redundancy.
struct ItemType {
    int l = 0;  // length   (mm)
    int w = 0;  // width    (mm)
    int h = 0;  // height   (mm)
    int m = 0;  // mass     (paper-defined unit)
    int q = 0;  // quantity (how many of this item type exist in the order)

    [[nodiscard]] int volume()   const { return l * w * h; }
    [[nodiscard]] int baseArea() const { return l * w; }
};

// Orientation encodes which of the two allowed Z-axis rotations was applied to an item.
// Rotated90 swaps the item's length and width.
enum class Orientation { Original, Rotated90 };

// PlacedItem represents one specific box physically placed inside a container.
// It records WHERE it is (x, y, z), HOW it was rotated (orientation),
// its effective post-rotation dimensions (dx, dy, dz), and WHICH ItemType it came from.
struct PlacedItem {
    int item_type_index = 0;                    // index into the ItemType vector
    Orientation orientation = Orientation::Original;
    int x  = 0;  // position of near corner (mm)
    int y  = 0;
    int z  = 0;
    int dx = 0;  // effective length after rotation (mm)
    int dy = 0;  // effective width  after rotation (mm)
    int dz = 0;  // effective height (unchanged by Z-axis rotation)

    // Returns the far corner of the placed box: (x+dx, y+dy, z+dz)
    [[nodiscard]] std::array<int, 3> extent() const {
        return {x + dx, y + dy, z + dz};
    }

    // Returns the geometric centre of the placed box
    [[nodiscard]] std::array<int, 3> center() const {
        return {x + dx / 2, y + dy / 2, z + dz / 2};
    }
};

// Container represents a single pallet that physically holds placed items.
// Defaults are the Euro pallet dimensions from Config.h.
// Override at construction via C++20 designated initializers:
//   Container c{.L=1000, .W=600, .H=1200};
struct Container {
    int L = Config::PALLET_L;  // pallet length (mm) — configurable
    int W = Config::PALLET_W;  // pallet width  (mm) — configurable
    int H = Config::PALLET_H;  // pallet height (mm) — configurable

    std::vector<PlacedItem> items;

    // Returns the fraction of pallet volume occupied by placed items (0.0–1.0).
    // PlacedItem already stores post-rotation dimensions (dx, dy, dz), so no
    // ItemType lookup is needed here.
    [[nodiscard]] double utilization() const {
        int used = 0;
        for (const PlacedItem& p : items) {
            used += p.dx * p.dy * p.dz;
        }
        return static_cast<double>(used) / (L * W * H);
    }

    // Returns the total mass of all placed items in this container.
    // Mass lives on ItemType, so we need the shared types vector to resolve each index.
    [[nodiscard]] int totalWeight(const std::vector<ItemType>& types) const {
        int total = 0;
        for (const PlacedItem& p : items) {
            total += types[p.item_type_index].m;
        }
        return total;
    }
};

// PalletID is the compile-time key used to look up a pallet in PalletRegistry.
// Using an enum class instead of a raw string means every invalid lookup is a
// compile-time error — no silent typos, no runtime std::out_of_range surprises.
// Naming convention follows the same REGION_OPERATOR_WxL pattern that was used
// for the former string keys, e.g. EPAL_EUR_1, NA_CHEP_BLUE_48x40, AU_AS4068_1165x1165.
enum class PalletID {
    // --- European pallets ---
    EPAL_EUR_1,
    CHEP_EURO_800x1200,
    CHEP_EURO_800x1200_P,
    CRAEMER_EURO_L1,
    CRAEMER_NP1,
    LPR_PR080,
    GOST_EURO_800x1200,
    EUR_COMPAT_800x1200,
    EPAL_EUR_2,
    EPAL_EUR_3,
    CHEP_UK_1200x1000,
    CHEP_UK_1200x1000_P,
    LPR_UK100,
    FIN_1000x1200,
    GOST_FIN_1000x1200,
    CHEP_SA_CODE8001,
    ZA_1200x1000,
    IRAM_MERCOSUR,
    PBR1_BRAZIL,
    PBR1_BRAZIL_P,
    CL_EUR_800x1200,
    CL_1000x1200,
    GCC_1200x1000_P,
    SA_1200x1200_P,
    EPAL_EUR_6,
    CHEP_HALF_800x600,
    LPR_DP610_600x1000,
    EPAL_EUR_7,
    CHEP_QUARTER_600x400_P,
    CABKA_NEST_D2,
    // --- EPAL CP chemical pallets ---
    CP1, CP2, CP3, CP4, CP5, CP6, CP7, CP8, CP9,
    // --- North American pallets ---
    NA_GMA_STRINGER_48x40,
    NA_GMA_BLOCK_48x40,
    NA_CHEP_BLUE_48x40,
    NA_PECO_RED_48x40,
    NA_IGPS_48x40,
    NA_ORBIS_HD_48x40,
    NA_ORBIS_LP_48x40,
    NA_ORBIS_HDSC_48x40,
    NA_CABKA_US5_48x40,
    NA_CABKA_ECO_48x40,
    NA_BUCKHORN_48x40,
    NA_48x48_DRUM,
    NA_42x42_TELECOM,
    NA_48x45_AUTO,
    NA_48x42_CHEM,
    NA_44x44_CHEM,
    NA_40x40_DAIRY,
    NA_36x36_BEV,
    NA_48x36_PAPER,
    NA_48x20_HALF,
    // --- Asia-Pacific pallets ---
    JP_T11_JIS,
    JP_T11_PLASTIC,
    KR_T11,
    JP_T12_1200x1000,
    CN_1200x1000,
    CN_1200x1000_P,
    CN_1100x1100,
    IN_1200x1000,
    IN_1200x800,
    ASEAN_1200x1000,
    LOSCAM_SEA_1200x1000,
    SG_1100x1100,
    AU_AS4068_1165x1165,
    AU_CHEP_1165x1165_P,
    AU_LOSCAM_1165x1165,
    AU_EXPORT_1100x1100_P,
    NZ_1200x1000,
    TW_1200x1000,
    // --- South America / Africa / Middle East ---
    PBR2_BRAZIL,
    ZA_1200x1200_DRUM,
};

// PalletSpec is the runtime description of a physical pallet type looked up from
// the PalletRegistry.  All dimensions are stored in millimeters regardless of the
// pallet's native_unit (conversion happens at I/O time).
// H_pallet: the structural height of the pallet itself (e.g. 144 mm for EPAL EUR 1).
// H_load:   the maximum stacking height of cargo above the pallet deck (algorithm param).
// max_weight: maximum dynamic load in kg.
struct PalletSpec {
    std::string name;                     // human-readable, e.g. "EPAL EUR 1 / Euro pallet"
    int  L          = 0;                  // footprint length (mm)
    int  W          = 0;                  // footprint width  (mm)
    int  H_pallet   = 0;                  // structural pallet height (mm)
    int  H_load     = Config::PALLET_H;  // max cargo stacking height (mm)
    int  max_weight = Config::PALLET_MAX_WEIGHT; // kg
    Unit native_unit = Unit::Millimeters; // unit system of the original spec
};

// LayerType describes what fraction of the pallet footprint a layer covers.
// Full = entire L×W area; Half = one half; Quarter = one quarter.
enum class LayerType { Full, Half, Quarter };

// Layer represents a single horizontal slice of the pallet filled with one item type
// arranged in a regular grid. height is the item's h dimension (all items in a layer
// share the same height). item_count is how many individual boxes the grid contains.
// fill_rate is the fraction of the layer's footprint area occupied by items (0.0–1.0).
// placed_items stores the XY grid positions of every box in the layer (z is relative
// to the layer base and is set to 0 here; BlockBuilder writes absolute z coordinates
// when assembling layers into blocks).
struct Layer {
    int       item_type_index = 0;
    LayerType type            = LayerType::Full;
    int       item_count      = 0;   // number of boxes in this layer's grid
    int       height          = 0;   // layer thickness in mm (= item height)
    double    fill_rate       = 0.0; // fraction of footprint area covered (0.0–1.0)
    std::vector<PlacedItem> placed_items; // XY positions of every box in the grid
};

// Block is a vertical stack of layers placed on a container.
// z_base is the mm height at which the bottom of this block sits.
// Layers are stacked in order: layers[0] is at z_base, layers[1] above it, and so on.
// container_index is the index into the containers vector that owns this block.
struct Block {
    std::vector<Layer> layers;
    int container_index = 0;  // which pallet (Container) this block lives on
    int z_base          = 0;  // bottom z position of this block on the container (mm)

    // Returns the total height of all layers stacked in this block (mm).
    [[nodiscard]] int totalHeight() const {
        int h = 0;
        for (const Layer& l : layers) { h += l.height; }
        return h;
    }
};

// ExtremePoint is a candidate 3D position inside a container where the next item
// may be placed. The GA placement engine generates these at the projected corners
// of already-placed items, then tries each one when fitting the next box.
struct ExtremePoint {
    int x = 0;
    int y = 0;
    int z = 0;
};

// Individual represents one candidate solution in the genetic algorithm population.
// chromosome is an ordered permutation of item-type indices — the GA evolves this sequence.
// objectives holds the multi-objective fitness scores evaluated from the chromosome.
// rank and crowding_distance are NSGA-II selection bookkeeping values, set by the GA engine.
// aux_max_util is an auxiliary metric (not used by NSGA-II dominance/crowding) that records
// the utilization of the single most-filled container.  It serves as a final tiebreaker in
// selectBest() when all three objectives are equal — which occurs for high-density BR instances
// where every feasible chromosome uses the same number of containers and all items are placed.
struct Individual {
    std::vector<int>    chromosome;         // permutation of item-type indices
    std::vector<double> objectives;         // fitness values: [container_count, -avg_util, wasted_vol]
    int    rank               = 0;          // Pareto front rank (0 = best)
    double crowding_distance  = 0.0;        // NSGA-II crowding distance
    double aux_max_util       = 0.0;        // max single-container utilization (0.0–1.0), tiebreaker only
};

// PackingSolution is the top-level result of one complete packing run.
// It holds all containers (pallets) that were used, and can compute the
// mean volume utilization across them as a summary fitness score.
struct PackingSolution {
    std::vector<Container> containers;

    // Returns the mean utilization across all containers (0.0–1.0).
    // Returns 0.0 if there are no containers to avoid division by zero.
    [[nodiscard]] double avgUtilization() const {
        if (containers.empty()) return 0.0;
        double sum = 0.0;
        for (const Container& c : containers) {
            sum += c.utilization();
        }
        return sum / static_cast<double>(containers.size());
    }
};