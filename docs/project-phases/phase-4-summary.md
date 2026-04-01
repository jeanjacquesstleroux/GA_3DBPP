# Phase 4: Packing Engine — Layer & Block Building
**Environment:** Ubuntu 24.04, GCC 13.3.0, C++20, CMake 3.28.3 + Ninja, vcpkg (x64-linux)

---

## 1. Introduction

### 1.1 What This Phase Accomplishes

Phase 3 gave the program the ability to reason about geometric constraints in isolation. Phase 4 uses those tools for the first time as part of a real packing strategy: the paper's constructive Phase 1 heuristic, which packs the bulk of a homogeneous order into neat, stable, interlocked layers.

By the end of Phase 4 the program can:

- **Generate candidate layers** for any item type — full, half, and quarter pallet footprints, both orientations, with dynamic shifting.
- **Classify and filter** layers by their footprint coverage (full ≥ 90%, half ≥ 90%, quarter ≥ 85%).
- **Merge compatible layers** — same-height quarters into halves, same-height halves into full layers — producing multi-item-type layers of up to four types.
- **Build blocks** by stacking layers onto Container pallets, respecting max height, balancing half and quarter zones, and opening new pallets when needed.
- **Interlock layers** using Hausdorff distance: four symmetry variants of each incoming layer are tested and the one that maximises vertex misalignment with the layer below is chosen.
- **Identify residual items** — those that did not make it into any layer — and determine whether Phase 2 can use existing pallet space or needs a new empty pallet.
- **Look up pallet specifications** by compile-time enum key rather than runtime string, eliminating a class of silent runtime bugs.

### 1.2 Design Decisions

**`PalletID` as an `enum class` instead of a string key.** The original registry used `std::unordered_map<std::string, PalletSpec>` and `lookup("EPAL_EUR_1")`. Any typo at a call site compiled silently and threw `std::out_of_range` at runtime. Replacing the key with `enum class PalletID` makes every invalid lookup a compile-time error. The enum enumerators follow the same `REGION_OPERATOR_WxL` naming pattern as the former strings, so no meaning is lost. A `std::hash<PalletID>` specialisation is required because `unordered_map` has no default hash for `enum class`; the specialisation casts the enumerator to `int` and reuses `std::hash<int>`.

**`namespace LayerGenerator` instead of a class.** All three generator functions (`generateFull`, `generateHalves`, `generateQuarters`) are stateless pure computations — they take item dimensions and pallet dimensions as inputs and return Layer objects. There is no shared state to encapsulate across calls. A namespace gives grouped naming (`LayerGenerator::generateFull`) without the overhead of constructing objects. This is the correct C++ idiom for a collection of related free functions.

**Internal `buildLayer` helper avoids code duplication across all three generators.** The three generators differ only in the footprint dimensions and `LayerType` they pass. A single `buildLayer(item, index, type, orientation, dx, dy, footprint_l, footprint_w)` handles grid computation, fill rate, dynamic shifting, and `PlacedItem` construction once. Each public generator calls it with the appropriate arguments.

**`shiftedPositions` distributes gaps to the extremities, not to the centre.** The paper specifies that unused space along each axis should push items toward the pallet edges: first item at 0, last item at `footprint - d`, interior items interpolated. This maximises edge contact for support and increases the vertex misalignment between adjacent layers for better interlocking.  Integer arithmetic with `+ 0.5` rounding prevents floating-point drift across many items.

**4-element quadrant array `q[0..3]` tracks the height frontier within each pallet.** When full, half, and quarter layers coexist on the same pallet the height surface is no longer flat. Tracking a single `z` per pallet would be wrong. Tracking one height per quadrant `(q[0] = [0,L/2)×[0,W/2), ...)` is the minimal granularity needed. An invariant is maintained throughout: after a Footprint-A half layer, `q[0]==q[2]` and `q[1]==q[3]`; after a Footprint-B half layer, `q[0]==q[1]` and `q[2]==q[3]`. This ensures every layer always sits on a flat surface within its zone.

**Hausdorff interlocking is computed on XY-projected corners only.** The paper measures the misalignment between the top vertices of the bottom layer and the bottom vertices of the top layer. Since both surfaces are at the same absolute z (the bottom layer's top face and the top layer's bottom face touch), only the XY positions differ. Projecting to 2D integer `{x, y}` pairs is exact and matches the `Hausdorff::distance` API from Phase 3.

**`computeResiduals` uses `long long` for volume totals.** A single pallet is 1200×800×1400 mm, giving a volume of 1,344,000,000 mm³ — larger than `INT_MAX` (2,147,483,647). An order with multiple pallets and many residual items can easily produce totals that overflow `int`. `long long` (64-bit) handles values up to ~9.2×10¹⁸, which is safe for any realistic order.

### 1.3 Files Created or Modified in This Phase

| File | Status | Purpose |
|------|--------|---------|
| `include/Types.h` | Modified | Added `PalletID` enum class, `PalletSpec` struct, `Unit` enum |
| `include/PalletRegistry.h` | Modified | Changed key type to `PalletID`; added `std::hash<PalletID>` |
| `src/PalletRegistry.cpp` | Created | Singleton registry — 70+ pallet specs from EUR, NA, APAC regions |
| `include/LayerGenerator.h` | Created | Declarations for `generateFull`, `generateHalves`, `generateQuarters` |
| `src/LayerGenerator.cpp` | Created | Layer generation engine with dynamic shifting |
| `include/BlockBuilder.h` | Created | Declarations for filter, merge, `buildBlocks`, `computeResiduals` |
| `src/BlockBuilder.cpp` | Created | Block-building engine, Hausdorff interlocking, residual computation |
| `test/test_layers.cpp` | Created | 16 tests covering all Phase 4 functionality |
| `CMakeLists.txt` | Modified | Added `PalletRegistry.cpp`, `LayerGenerator.cpp`, `BlockBuilder.cpp`, `test_layers.cpp` |

### 1.4 `Config.h` Additions

The following constants were added to `namespace Config` to support Phase 4:

| Constant | Value | Meaning |
|----------|-------|---------|
| `LAYER_MIN_FILL_FULL_HALF` | 0.90 | Minimum fill rate for Full and Half layers (90%) |
| `LAYER_MIN_FILL_QUARTER` | 0.85 | Minimum fill rate for Quarter layers (85%) |

These values are the empirical thresholds from the paper: "85%, 90% and 90% of minimum fill rate values for quarter, half and full layers respectively tend to produce the best results" (Section IV-B-2).

### 1.5 Prerequisites

- Phase 1 complete: `Types.h`, `Config.h` in place.
- Phase 2 complete: build system and I/O modules working.
- Phase 3 complete: `Hausdorff.h/.cpp` available (used by block building for interlocking).
- All 57 Phase 1–3 tests passing before Phase 4 begins.

---

## Step 1: Refactor `PalletRegistry` — string keys to `PalletID` enum

### Motivation

The original `PalletRegistry` used `std::unordered_map<std::string, PalletSpec>` and exposed `lookup(const std::string& key)`. Any string could be passed at a call site. A typo like `"EPAL_EUR1"` (missing underscore) compiled without error and threw `std::out_of_range` at runtime — only catchable in testing if the specific path was exercised.

### `PalletID` enum class in `Types.h`

```cpp
enum class PalletID {
    // --- European pallets ---
    EPAL_EUR_1,
    CHEP_EURO_800x1200,
    // ... 70+ enumerators
    // --- North American pallets ---
    NA_GMA_STRINGER_48x40,
    NA_CHEP_BLUE_48x40,
    // --- Asia-Pacific pallets ---
    AU_AS4068_1165x1165,
    JP_T11_JIS,
    // ...
};
```

Every formerly valid string key becomes an enumerator. A call site that used `"EPAL_EUR_1"` now writes `PalletID::EPAL_EUR_1`. A typo like `PalletID::EPAL_EUR1` is a compile error.

### `std::hash<PalletID>` specialisation

`std::unordered_map` requires a hash function for its key type. There is no built-in hash for `enum class` in the standard library. The specialisation is placed in `PalletRegistry.h`, before the `PalletRegistry` namespace, so the compiler finds it when instantiating `Registry`:

```cpp
namespace std {
    template <>
    struct hash<PalletID> {
        size_t operator()(PalletID id) const noexcept {
            return std::hash<int>{}(static_cast<int>(id));
        }
    };
} // namespace std
```

`static_cast<int>(id)` converts the enumerator to its underlying integer value. `std::hash<int>` is then applied — a constant-time, high-quality hash already in the standard library.

### Updated `PalletRegistry` namespace

```cpp
namespace PalletRegistry {
    using Registry = std::unordered_map<PalletID, PalletSpec>;
    [[nodiscard]] const Registry& get();
    [[nodiscard]] const PalletSpec& lookup(PalletID id);
}
```

The error message thrown by `lookup` on a missing ID now includes the numeric value (`std::to_string(static_cast<int>(id))`), which tells the developer which enumerator is absent from `PalletRegistry.cpp` — not a call-site typo, but a missing `add()` call.

### `PalletSpec` — human-readable name retained

The `name` field (`std::string name`) is kept on `PalletSpec` for display purposes: UIs and log output still need a readable label like `"EPAL EUR 1 / Euro pallet"`. The string is no longer the key, so it cannot be mistyped into a lookup. The `name` field is purely informational.

**Key points:**
- The `enum class` provides zero-overhead key lookup — the hash of an `int` is a single multiply, no string comparison or hashing.
- Placing the `std::hash` specialisation in `namespace std` is valid and idiomatic for user-defined types; it is explicitly permitted by the C++ standard.
- `H_load` on `PalletSpec` defaults to `Config::PALLET_H` — the algorithm's cargo stacking zone height — rather than `H_pallet` (the structural height of the pallet itself). This distinction matters: a 144 mm pallet structure sits below the cargo zone, so cargo stacking begins at 144 mm above the ground but the algorithm's coordinate system starts at z = 0 (the pallet deck).

---

## Step 2: Implement `LayerGenerator` — Tasks 4.1 and 4.2

### Two-orientation selection (Task 4.1)

For any footprint, two item orientations are possible: `Original` (`dx = l, dy = w`) and `Rotated90` (`dx = w, dy = l`). The number of items that fit along each axis is `floor(footprint_dim / item_dim)`. The total count is `items_x × items_y`. The orientation with the higher count wins; on a tie, `Original` is kept.

A shared `bestOfTwo` helper makes the policy explicit:

```cpp
static Layer bestOfTwo(Layer orig, Layer rotated) {
    return (rotated.item_count > orig.item_count) ? std::move(rotated)
                                                  : std::move(orig);
}
```

`std::move` prevents an unnecessary copy — both `orig` and `rotated` are local Layer objects whose contents can be transferred rather than duplicated.

### Dynamic shifting (Task 4.2)

After selecting the winning orientation, items are not packed flush to the near wall. The `shiftedPositions` function distributes the unused space:

```cpp
static std::vector<int> shiftedPositions(int N, int d, int F) {
    if (N <= 0) return {};
    if (N == 1) return {0};

    std::vector<int> pos(N);
    pos[0]     = 0;
    pos[N - 1] = F - d;
    for (int i = 1; i < N - 1; ++i) {
        pos[i] = static_cast<int>(
            static_cast<double>(i) * (F - d) / (N - 1) + 0.5);
    }
    return pos;
}
```

- `pos[0] = 0` — first item flush with the near wall.
- `pos[N-1] = F - d` — last item flush with the far wall.
- Interior items interpolated with `+0.5` rounding — the `0.5` converts truncation toward zero into rounding to nearest, preventing accumulated drift across many items.

**Why distribute to the edges?** The paper notes that pushing items to the pallet extremities improves two things: (a) edge contact improves the support score for the next layer sitting on top; (b) asymmetric item placement between adjacent layers increases the Hausdorff distance, improving interlocking under transport loads.

### `generateFull`, `generateHalves`, `generateQuarters`

All three public functions delegate to the internal `buildLayer` helper. `buildLayer` computes the grid, calls `shiftedPositions` for both axes, and constructs all `PlacedItem` objects with `z = 0` — BlockBuilder writes the absolute z when stacking. The `LayerType` field is set in `buildLayer` based on which footprint is being filled.

| Function | Footprints tried | Candidates returned |
|----------|-----------------|---------------------|
| `generateFull` | `pallet_l × pallet_w` | 1 (best orientation) |
| `generateHalves` | `(pallet_l/2) × pallet_w` and `pallet_l × (pallet_w/2)` | Up to 4 (both footprints × both orientations, zero-count filtered) |
| `generateQuarters` | `(pallet_l/2) × (pallet_w/2)` | Up to 2 (both orientations, zero-count filtered) |

**Key points:**
- All placed items in a layer share the same `dx`, `dy`, `dz` and `orientation`, which is consistent with the single-item-type invariant for Phase 1 layers.
- `z = 0` in every generated layer. This is intentional: the layer does not know where it will sit in a block. BlockBuilder adds the absolute z coordinate when stacking.
- Integer division in `footprint / item_dim` truncates — exactly the right behaviour for fitting as many whole items as possible without exceeding the footprint.

---

## Step 3: Implement `BlockBuilder` — fill-rate filter and classification (Tasks 4.3, 4.5)

### `passesFillRate`

A layer's fill rate is the fraction of its footprint area covered by items. The acceptable minimum depends on layer type:

```cpp
bool BlockBuilder::passesFillRate(const Layer& layer) {
    switch (layer.type) {
        case LayerType::Full:
        case LayerType::Half:
            return layer.fill_rate >= Config::LAYER_MIN_FILL_FULL_HALF;
        case LayerType::Quarter:
            return layer.fill_rate >= Config::LAYER_MIN_FILL_QUARTER;
    }
    return false;  // unreachable; satisfies -Wreturn-type
}
```

Quarter layers have a lower threshold (85%) because their footprint is already one-quarter of the pallet — a partially-filled quarter still contributes meaningfully to overall volume utilisation.

### `filterByFillRate`

```cpp
void BlockBuilder::filterByFillRate(std::vector<Layer>& layers) {
    layers.erase(
        std::remove_if(layers.begin(), layers.end(),
            [](const Layer& l) { return !passesFillRate(l); }),
        layers.end());
}
```

The erase–remove idiom is the standard way to remove elements from a `std::vector` by predicate. `std::remove_if` partitions the vector: all elements satisfying the predicate are moved to the end and a past-the-valid-end iterator is returned. `erase` then deletes that trailing range. A raw loop with `erase` inside would invalidate iterators; this pattern is safe and O(n).

---

## Step 4: Implement `BlockBuilder` — layer merging (Task 4.4)

### Purpose

After generating candidates for all item types, some quarter layers of different item types may have the same height. Two same-height quarters can be placed side by side to form a half layer covering two item types. Two same-height halves of the same footprint axis can form a full layer covering up to four item types. Merging maximises coverage and reduces the number of layers the block-builder must place.

### Footprint axis detection

A half layer is Footprint-A (`L/2 × W`) if its items' maximum x-extent is within the left half of the pallet (`max(x + dx) ≤ pallet_l / 2`). Otherwise it is Footprint-B (`L × W/2`). This heuristic works because half layers generated by `LayerGenerator::generateHalves` place items strictly within their footprint region, so the x-extent boundary is always clean.

### Merging algorithm

```
1. Group quarter layers by height.
   For each height group, pair adjacent quarters:
     merged half = items from quarter[i] ++ items from quarter[i+1] offset +pallet_l/2 in x.
   Odd quarters go to unmerged_quarters.

2. Collect all halves (original + newly created from step 1).
   Group by (height, footprint_axis).
   For each group, pair adjacent halves:
     Footprint-A pairs: second offset +pallet_l/2 in x.
     Footprint-B pairs: second offset +pallet_w/2 in y.
   Odd halves go to unmerged_halves.

3. Return: original full layers
         + full layers from step 2
         + unmerged halves
         + unmerged quarters.
```

The merged layer's `fill_rate = (a.fill_rate + b.fill_rate) / 2`. This is exact when the two source footprints are equal, which is always true for the three merge scenarios.

**Key points:**
- `std::map<int, std::vector<Layer>> by_height` groups layers by integer height key in O(n log n). The integer key avoids floating-point equality issues.
- Merged layers carry the `item_type_index` of the first source layer (`a.item_type_index`). This field is used only as a tiebreaker sort key in block building. The `PlacedItem` objects inside the layer each carry their own authoritative `item_type_index`.
- The `pairAndMerge` helper processes pairs `(0,1), (2,3), ...` greedily. Greedy pairing is O(n) per group and produces the maximum number of merged layers. A globally optimal pairing across groups is not needed because within-height groups are always interchangeable.

---

## Step 5: Implement `BlockBuilder::buildBlocks` — block stacking engine (Tasks 4.6–4.9)

### Overview

`buildBlocks` takes the complete set of filtered and merged candidate layers, distributes them across `Container` objects, and returns those containers with all placed items committed. It operates in three sequential passes: full layers, then half layers, then quarter layers.

### `PalletState` — height frontier tracking

An internal `PalletState` struct (not exposed in the header) maintains the 4-quadrant height array for each active pallet:

```
q[0] = [0,   L/2) × [0,   W/2)   near-left
q[1] = [L/2, L  ) × [0,   W/2)   far-left
q[2] = [0,   L/2) × [W/2, W  )   near-right
q[3] = [L/2, L  ) × [W/2, W  )   far-right
```

`remaining() = max_h − max(q[0..3])` is the available height above the current tallest zone. Pallets are sorted by `remaining()` ascending before each layer placement so the most-filled pallet is tried first — this maximises volume use per pallet.

### Pass 1 — full layers (Task 4.6)

Full layers require a flat surface: all four quadrants at equal height. For each layer (sorted by occupied area desc → weight desc → item_type_index asc):

1. Sort active pallets by remaining height ascending.
2. Try each pallet in order: if `q[0]==q[1]==q[2]==q[3]` and `z + height ≤ max_h`, place there.
3. If no pallet fits, open a new one.

After placement all four quadrant heights advance equally: `q[i] = z + layer.height`.

### Pass 2 — half layers (Task 4.7)

Half layers are placed in one of two half-zones per pallet. For Footprint-A (L/2 × W) the zones are left `(q[0], q[2])` and right `(q[1], q[3])`; for Footprint-B (L × W/2) they are front `(q[0], q[1])` and back `(q[2], q[3])`.

**Placement rule:** the lower zone is chosen. On a tie the "first" zone (zone 0) is preferred, matching the paper: "the first half layer is always placed on the first half of the pallet." After placement, both quadrant indices in the chosen zone advance.

When the pallet's `isHalfA()` check (max x-extent ≤ pallet_l/2) identifies the footprint type, the correct XY offset is applied to the layer's items:
- Footprint-A right zone: `+pallet_l/2` in x.
- Footprint-B back zone: `+pallet_w/2` in y.

### Pass 3 — quarter layers (Task 4.8)

Quarter layers go to the single lowest quadrant among the four. On an empty pallet (all quadrants at 0) the paper specifies quadrant 0 — the first quadrant. Each quadrant has a fixed XY offset:

| Quadrant | x offset | y offset |
|----------|----------|----------|
| 0 | 0 | 0 |
| 1 | pallet_l/2 | 0 |
| 2 | 0 | pallet_w/2 |
| 3 | pallet_l/2 | pallet_w/2 |

### Hausdorff interlocking (Task 4.9)

Before committing any layer on top of an existing stack, `bestInterlockVariant` is called:

```cpp
static Layer bestInterlockVariant(const Layer& prev, Layer next,
                                   int footprint_l, int footprint_w) {
    const auto prev_corners = collectTopCorners(prev.placed_items);
    Layer best = next;
    double best_d = Hausdorff::distance(prev_corners,
                                         collectTopCorners(next.placed_items));

    const bool flips[3][2] = {{true, false}, {false, true}, {true, true}};
    for (const auto& [fx, fy] : flips) {
        Layer variant = flipLayer(next, fx, fy, footprint_l, footprint_w);
        double d = Hausdorff::distance(prev_corners,
                                        collectTopCorners(variant.placed_items));
        if (d > best_d) { best_d = d; best = std::move(variant); }
    }
    return best;
}
```

`collectTopCorners` extracts all four XY corners of each item's footprint — four 2D integer points per item. `flipLayer` mirrors positions within the footprint:
- H-flip: `x' = footprint_l − x − dx`
- V-flip: `y' = footprint_w − y − dy`
- HV-flip: both

This implements Constraint 8 (layer interlocking) exactly as the paper specifies: "the algorithm considers 4 different variations of one layer: the original pattern, the pattern with horizontal symmetry, the pattern with vertical symmetry and the pattern with both horizontal and vertical symmetry. The version with a higher distance is selected."

### `commitLayer` — absolute z coordinate assignment

```cpp
static Layer commitLayer(Container& cont, Layer layer,
                          int offset_x, int offset_y, int z_base) {
    for (PlacedItem& pi : layer.placed_items) {
        pi.x += offset_x;
        pi.y += offset_y;
        pi.z  = z_base;
        cont.items.push_back(pi);
    }
    return layer;
}
```

`commitLayer` applies the half/quarter XY offset and writes the absolute z into every `PlacedItem` before pushing it to the container. It returns the modified layer so the caller can use it as `prev` for the next Hausdorff call.

---

## Step 6: Implement `BlockBuilder::computeResiduals` — Task 4.10

```cpp
BlockBuilder::ResidualInfo BlockBuilder::computeResiduals(
    const std::vector<ItemType>& item_types,
    const std::vector<Container>& containers,
    int pallet_l, int pallet_w, int pallet_h)
{
    std::vector<int> packed(item_types.size(), 0);
    for (const Container& cont : containers)
        for (const PlacedItem& pi : cont.items)
            ++packed[pi.item_type_index];

    ResidualInfo info;
    long long v_residual = 0;
    for (int i = 0; i < static_cast<int>(item_types.size()); ++i) {
        int remaining = item_types[i].q - packed[i];
        if (remaining > 0) {
            info.residuals.emplace_back(i, remaining);
            v_residual += static_cast<long long>(item_types[i].volume()) * remaining;
        }
    }

    long long v_usable = 0;
    for (const Container& cont : containers) {
        int top_z = 0;
        for (const PlacedItem& pi : cont.items)
            top_z = std::max(top_z, pi.z + pi.dz);
        v_usable += static_cast<long long>(pallet_l) * pallet_w * (pallet_h - top_z);
    }

    info.spawn_new_pallet = (v_residual >= v_usable);
    return info;
}
```

**Key points:**
- `packed[i]` counts the actual number of `PlacedItem` objects with `item_type_index == i`. It does not assume that item quantities were accurately reflected by the layer generator — it counts what is physically in the containers.
- `long long` is mandatory for volume totals. A single Euro pallet has 1,200 × 800 × 1,400 = 1,344,000,000 mm³, which exceeds `INT_MAX`. Multiplying by even a small residual count would overflow a 32-bit integer silently.
- `spawn_new_pallet` uses `>=`: if residual volume equals usable volume exactly, there is no margin for Phase 2's placement overhead. A new pallet is the conservative and correct choice.
- The `ResidualInfo` struct stores `{type_index, remaining_count}` pairs rather than a modified copy of `item_types`. Phase 2 uses the original `item_types` for dimensions and mass; only the residual counts are new information.

---

## Step 7: Write `test/test_layers.cpp` — Task 4.11

16 test cases across two suites:

### `LayerGenerator` suite (7 cases)

| Test | What it checks |
|------|----------------|
| `FullLayerItemCount` | 300×200 item in 1200×800 pallet → 16 items in one full layer |
| `FullLayerPicksBestOrientation` | 400×300 item: rotated orientation fits 8 vs 6 original → rotated chosen |
| `FullLayerTieKeepsOriginal` | 400×200 item: both orientations fit 12 → Original preferred |
| `DynamicShiftingFirstAndLastFlush` | First item at x=0, last item at x=pallet_l−item_l |
| `HalvesAllHalfType` | All returned layers carry `LayerType::Half`; count in [1, 4] |
| `QuartersAllQuarterType` | All returned layers carry `LayerType::Quarter`; count in [1, 2] |
| `OversizedItemProducesNoQuarters` | 700×500 item in 600×400 quarter → empty vector returned |

### `BlockBuilder` suite (9 cases)

| Test | What it checks |
|------|----------------|
| `FilterRemovesLowFillRate` | Layer at `threshold − 0.01` is removed; layer at threshold survives |
| `QuarterThresholdIsLower` | Quarter layer at 87% passes; would fail Full/Half threshold of 90% |
| `PerfectDivisionAllItemsPacked` | 48 items in 3 layers → all committed, no overlaps, in bounds |
| `TallItemOverflowsToNewPallet` | 3 layers of 700mm each → requires 2 pallets (2×700=1400=max) |
| `SingleItemOrder` | One-item order builds without error; item in bounds |
| `NoItemsBelowFloor` | All placed items have z ≥ 0 |
| `NoResidualsWhenAllPacked` | q=16, 1 full layer of 16 → `residuals` empty |
| `ResidualsDetectedForUnpackedItems` | q=20, 1 full layer of 16 → residual count = 4 |
| `SpawnNewPalletWhenNoRemainingSpace` | 14 layers × 100mm = 1400mm fills pallet → `spawn_new_pallet = true` |

**Key points on `noOverlaps` helper:** the test helper performs an O(n²) pairwise AABB check on every container. This is too slow for production but correct for tests with small item counts. The three separation conditions mirror `AABB::overlaps` exactly: two items are non-overlapping if they are separated on at least one axis.

---

## Step 8: Update `CMakeLists.txt`

Three new `.cpp` files were added to both the main executable and the test target. One new test file was added to the test target only:

```cmake
add_executable(GA_3DBPP
    ...
    src/LayerGenerator.cpp    # added
    src/BlockBuilder.cpp      # added
    src/PalletRegistry.cpp    # added
)

add_executable(GA_3DBPPTests
    ...
    test/test_layers.cpp      # added
    src/LayerGenerator.cpp    # added
    src/BlockBuilder.cpp      # added
    src/PalletRegistry.cpp    # added
)
```

`BlockBuilder.cpp` includes `Hausdorff.h` and links against the `Hausdorff.cpp` translation unit already present in both targets from Phase 3. No new `target_link_libraries` calls were needed.

---

## Final file structure added in Phase 4

```
include/
├── Types.h            # Modified: added PalletID enum, PalletSpec, Unit enum
├── PalletRegistry.h   # Modified: Registry keyed by PalletID; std::hash<PalletID>
├── LayerGenerator.h   # generateFull(), generateHalves(), generateQuarters()
└── BlockBuilder.h     # passesFillRate(), filterByFillRate(), mergeLayers(),
                       # buildBlocks(), ResidualInfo, computeResiduals()

src/
├── PalletRegistry.cpp # Singleton: 70+ pallet specs (EUR, NA, APAC regions)
├── LayerGenerator.cpp # buildLayer(), shiftedPositions(), bestOfTwo()
└── BlockBuilder.cpp   # PalletState, Hausdorff helpers, three-pass block builder

test/
└── test_layers.cpp    # 16 cases: layer count, orientation, shifting, fill-rate,
                       # overflow, overlaps, bounds, residuals
```

Full test suite after Phase 4: **73 tests, 100% passing** under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## What changes in Phase 5

Phase 5 implements the Extreme Points (EP) placement strategy that the genetic algorithm uses to evaluate chromosomes in Phase 2. Where Phase 1 (layers and blocks) handles homogeneous items that repeat enough to form regular grids, Phase 2 (EP) handles the residuals — items that could not form a layer. The EP engine initialises candidate positions from the top surfaces of the blocks built in Phase 4, generates three new EPs per placed item, projects them down to the nearest supporting surface, and removes dominated or interior points. Placement iterates over sorted EPs (lowest z first, then closest to origin) and tries each in both orientations until all items are placed or all EPs are exhausted.
