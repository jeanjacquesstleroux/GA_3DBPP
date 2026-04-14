# Phase 7 Summary — Main Entry Point & Integration

## 1. Overview

### 1.1 What Was Accomplished

Phase 7 wired every prior component into a single executable pipeline and verified it end-to-end. The key deliverables were:

- **`Packer::decode`** — translates a GA chromosome into a `PackingSolution` by running Extreme Point placement sequentially across containers.
- **Rewritten `src/main.cpp`** — full CLI pipeline: parse arguments → read CSV → Phase 1 (layer/block building) → Phase 2 (NSGA-II GA on residuals) → select best Pareto-front solution → write JSON.
- **`test/test_integration.cpp`** — 10 end-to-end tests covering homogeneous, heterogeneous, and mixed orders; AABB/bounds constraint checks; solution-selection unit tests.
- **Bug fix in `BlockBuilder::buildBlocks`** — added quantity tracking (`remaining[]`) so Phase 1 never places more items of a type than `ItemType::q` allows.

### 1.2 Design Decisions

**`Packer::decode` sequential-container model.**  
The chromosome is a permutation of residual item-type indices. Items are placed one at a time via `ExtremePointEngine::placeItem`. If the active container is full, the algorithm advances to the next existing container before opening a new one. A `just_opened` flag prevents an infinite loop when an item is physically too large for any pallet — that item is counted as `unplaced` and skipped.

**Solution selection heuristic (Task 7.3).**  
`selectBest` uses `std::min_element` with a two-level comparator: fewest containers (primary), least wasted volume in mm³ (secondary). This exactly matches the paper's stated preference: compact first, then heterogeneity minimized.

**`BlockBuilder::buildBlocks` quantity fix.**  
The pre-existing bug caused Phase 1 to stack layers without respecting `ItemType::q`. The fix adds a `std::vector<int> remaining` initialized from each type's `q`, with `canCommit` / `doCommit` lambdas that check and decrement counts around every `commitLayer` call in all three passes (full, half, quarter). Layers that would require more items than available are skipped via `continue`.

**Fully heterogeneous order branch.**  
If all layers are filtered out by `filterByFillRate` (every item type is unique, no type fills ≥ 90 % of a pallet footprint), Phase 1 is skipped entirely. All item types become residuals and one empty container is seeded so the GA has a starting pallet.

### 1.3 Files Created / Modified

| File | Action | Purpose |
|------|--------|---------|
| `include/Packer.h` | Created | Declares `Packer::decode` |
| `src/Packer.cpp` | Created | Implements chromosome → `PackingSolution` via EP placement |
| `src/main.cpp` | Rewritten | Full pipeline orchestration with spdlog logging and stdout summary |
| `test/test_integration.cpp` | Created | 10 end-to-end and constraint-check tests |
| `src/BlockBuilder.cpp` | Modified | Added quantity tracking in `buildBlocks` |
| `test/test_layers.cpp` | Modified | Fixed `SingleItemOrder` test to use `q=10` (consistent with correct behavior) |
| `CMakeLists.txt` | Modified | Added `src/Packer.cpp`, `src/GeneticOperators.cpp`, `src/NSGA2.cpp` (Phase 6), `test/test_integration.cpp` |

### 1.4 Configuration

No new `Config.h` constants were introduced. Phase 7 reuses the GA parameters from Phase 6 (`GA_POPULATION`, `GA_MU`, `GA_LAMBDA`, etc.) and pallet dimensions from `Config::PALLET_*`.

### 1.5 Prerequisites

All prior phases must be complete:
- Phase 1 (Types), Phase 2 (I/O), Phase 3 (Geometry), Phase 4 (Layer/Block Building), Phase 5 (Extreme Points), Phase 6 (NSGA-II + GeneticOperators + Packer).

---

## 2. Step-by-Step Implementation Log

### Step 1 — Declared `Packer::decode` (Task 7.1)

`include/Packer.h` declares one static method:

```cpp
static PackingSolution decode(
    const std::vector<int>&      chromosome,
    const std::vector<int>&      residualCounts,
    const std::vector<ItemType>& itemTypes,
    const std::vector<Container>& seedContainers,
    int& out_unplaced);
```

`out_unplaced` is an `int&` out-parameter. It is incremented for every item that could not be placed on any container (physically infeasible — item larger than pallet). The caller uses it to apply a penalty to the fitness objectives.

### Step 2 — Implemented `Packer::decode` (Task 7.1)

`src/Packer.cpp` copies `seedContainers` to preserve Phase 1 blocks, then initializes one EP list per container (calling `ExtremePointEngine::initEPs` for each). The main loop processes the chromosome left-to-right:

```
for each type_idx in chromosome:
    for count in residualCounts[type_idx]:
        placed = false
        while not placed:
            try placeItem on containers[active]
            if success: placed = true
            else if active+1 < containers.size(): advance active
            else if not just_opened: open new container, just_opened = true
            else: ++out_unplaced; break   ← item is physically infeasible
```

The `just_opened` flag is reset to `false` whenever the active container changes to an already-existing one, and set to `true` when a brand-new container is opened. This ensures we attempt placement once on a new container; if it still fails, the item cannot fit anywhere and is skipped rather than looping forever.

### Step 3 — Wrote `src/main.cpp` (Tasks 7.2–7.4)

Key sections of `main.cpp`:

**Argument parsing:** `argv[1]` = input CSV path; optional `argv[2]` = output JSON path (defaults to `output/solution.json`).

**Directory creation:** `std::filesystem::create_directories` ensures the output directory exists before writing.

**Phase 1 pipeline:**
```cpp
// Generate + filter layers for all item types
for each itemType i:
    generate Full, Half, Quarter layers → push into all_layers
BlockBuilder::filterByFillRate(all_layers);

if all_layers.empty():
    // Fully heterogeneous — skip Phase 1, all items are residuals
    containers.push_back(Container{});
    residualTypes = all type indices; residualCounts = all quantities
else:
    merged     = BlockBuilder::mergeLayers(all_layers)
    containers = BlockBuilder::buildBlocks(merged, itemTypes)
    resInfo    = BlockBuilder::computeResiduals(itemTypes, containers)
    if resInfo.spawn_new_pallet: containers.push_back(Container{})
    residualTypes / residualCounts = resInfo.residuals
```

**Phase 2 (GA):**
```cpp
if residualTypes.empty():
    best_solution.containers = containers  // all packed in Phase 1
else:
    pop   = NSGA2::run(residualTypes, residualCounts, itemTypes, containers, rng)
    front = NSGA2::extractParetoFront(pop)
    best  = selectBest(front)
    best_solution = Packer::decode(best.chromosome, residualCounts, itemTypes,
                                   containers, unplaced)
```

**Output:** `writeJSON(best_solution, itemTypes, output_path)` then a stdout summary block showing order ID, containers used, average utilization %, and elapsed time.

**Timing:** `std::chrono::steady_clock` wraps Phase 1 + Phase 2. The elapsed duration is printed in seconds with two decimal places.

### Step 4 — Implemented `selectBest` (Task 7.3)

```cpp
static const Individual& selectBest(const std::vector<Individual>& front)
{
    return *std::min_element(front.begin(), front.end(),
        [](const Individual& a, const Individual& b) {
            if (a.objectives[0] != b.objectives[0])
                return a.objectives[0] < b.objectives[0];   // fewer containers
            return a.objectives[2] < b.objectives[2];       // less wasted volume
        });
}
```

`objectives[0]` is the container count (minimize). `objectives[2]` is total wasted volume in mm³ (minimize). `objectives[1]` (negative average utilization) is not used in selection — it is implicitly correlated with `objectives[2]` and the paper uses the two-level heuristic described above.

### Step 5 — Wrote `test/test_integration.cpp` (Task 7.5)

Ten tests exercise the full pipeline using small in-memory datasets:

| Test | What It Checks |
|------|---------------|
| `HomogeneousOrderAllItemsPlaced` | Phase 1 packs bulk; total placed = total ordered (q=12) |
| `HeterogeneousOrderAllItemsPlaced` | Phase 1 skipped; GA places 3 unique items |
| `MixedOrderAllItemsPlaced` | Phase 1 packs type 0 layers; GA handles type 1 singleton |
| `NoAABBViolationsInSolution` | No two placed items share interior volume |
| `AllItemsWithinContainerBounds` | Every item fits within its container's walls |
| `SingleItemPlacedSuccessfully` | q=1 order: GA places the single item |
| `TinyOrderUsesOneContainer` | Two small items fit in exactly 1 container |
| `HomogeneousOrderSatisfiesConstraints` | Combines AABB + bounds checks for Phase 1 output |
| `SelectionTest.PrefersFewerContainers` | 1-container solution beats 2-container regardless of waste |
| `SelectionTest.TiebreaksOnWastedVolume` | Equal container counts → lower wasted volume wins |

The `runPipeline` helper replicates `main.cpp` logic exactly so tests share one code path.

### Step 6 — Diagnosed and Fixed `BlockBuilder::buildBlocks` Bug

**Root cause:** `buildBlocks` stacked layers from the sorted candidate list without checking `ItemType::q`. For an order with `q=12`, `generateFull` might produce a layer of 4 items; `buildBlocks` would stack 7 layers (filling 1400 mm / 200 mm height) → 28 items placed, but only 12 were ordered. `computeResiduals` returns 0 residuals (since `packed > q` → `max(0, q-packed) = 0`), so the GA is skipped and the solution overcounts items.

**Fix:** Added `remaining[]` vector and two lambdas in `buildBlocks`:

```cpp
std::vector<int> remaining(item_types.size());
for (int i = 0; i < (int)item_types.size(); ++i)
    remaining[i] = item_types[i].q;

auto canCommit = [&](const Layer& layer) -> bool {
    std::vector<int> needed(remaining.size(), 0);
    for (const PlacedItem& pi : layer.placed_items)
        ++needed[pi.item_type_index];
    for (int i = 0; i < (int)needed.size(); ++i)
        if (needed[i] > remaining[i]) return false;
    return true;
};

auto doCommit = [&](const Layer& layer) {
    for (const PlacedItem& pi : layer.placed_items)
        --remaining[pi.item_type_index];
};
```

Each pass (full, half, quarter) gains `if (!canCommit(layer)) continue;` at the top of its loop, and `doCommit(...)` after every successful `commitLayer` call (both the existing-pallet and new-pallet paths). Iterating `placed_items` (rather than using `layer.item_type_index` alone) handles merged layers that may contain two different item types.

**Side effect:** The existing `BlockBuilder.SingleItemOrder` unit test used `q=1` with a full-layer item that generates 4 items per layer. With the fix, that layer is correctly skipped (4 > 1). The test was updated to `q=10` so Phase 1 can commit the layer — consistent with the intended behavior of verifying that a single item type produces a non-empty block.

---

## 3. Final File Structure After Phase 7

```
GA_3DBPP/
├── include/
│   ├── Packer.h          ← NEW (Task 7.1)
│   ├── NSGA2.h           (Phase 6)
│   ├── GeneticOperators.h (Phase 6)
│   └── ...
├── src/
│   ├── Packer.cpp        ← NEW (Task 7.1)
│   ├── main.cpp          ← REWRITTEN (Tasks 7.2–7.4)
│   ├── BlockBuilder.cpp  ← MODIFIED (quantity-tracking bug fix)
│   ├── NSGA2.cpp         (Phase 6)
│   ├── GeneticOperators.cpp (Phase 6)
│   └── ...
├── test/
│   ├── test_integration.cpp ← NEW (Task 7.5)
│   ├── test_layers.cpp      ← MODIFIED (q=10 fix for SingleItemOrder)
│   ├── test_nsga2.cpp    (Phase 6)
│   └── ...
└── CMakeLists.txt        (updated for Packer + integration tests)
```

---

## 4. Test Results

**119 / 119 tests pass** (zero failures, zero warnings).

```
100% tests passed, 0 tests failed out of 119
Total Test time (real) = 16.17 sec
```

Build: GCC 13.3.0, C++20, `-Wall -Wextra -Wpedantic`, ASan + UBSan enabled (Debug preset). Zero compiler warnings.

---

## 5. What Changes in Phase 8

Phase 8 focuses on benchmarking and comprehensive validation:

- **BR1–BR7 benchmark runner**: process all 700 instances from the OR-Library datasets, collect per-instance volume utilization and elapsed time, compare against Table 7 of the paper.
- **Full constraint audit**: extend AABB and bounds checks with support (Constraint 4) and center-of-mass (Constraint 3) verification across all benchmark outputs.
- **Edge-case hardening**: fully heterogeneous large orders, overweight single items, empty orders, orders where every item exceeds pallet footprint.
- **`BenchmarkRunner` utility**: automated batch runner that writes a CSV summary (instance ID, utilization %, time ms, containers used, violation count).
