# Phase 5: Packing Engine — Extreme Points Placement
**Environment:** Ubuntu 24.04, GCC 13.3.0, C++20, CMake 3.28.3 + Ninja, vcpkg (x64-linux)

---

## 1. Introduction

### 1.1 What This Phase Accomplishes

Phase 4 produced a set of fully-packed Container pallets (the Phase 1 blocks) and a list of residual item types — boxes that could not form a layer because too few of them exist or their dimensions do not divide the pallet footprint cleanly. Phase 5 implements the Extreme Points (EP) placement strategy that the genetic algorithm (Phase 6) will use to evaluate each chromosome: given a permutation of residual item types, the EP engine places them one by one onto existing blocks or empty pallets until all are packed or no valid position remains.

By the end of Phase 5 the program can:

- **Initialise an EP list** for any container — a single origin point `{0, 0, 0}` for empty pallets, or the full set of candidate positions derived from Phase 1 block surfaces for pallets that already hold blocks.
- **Generate 3 new EPs** each time an item is placed — right-face, back-face, and top-face positions — which accumulate into a growing pool of candidate positions.
- **Project EPs downward** to the nearest supporting surface (item top face or pallet floor), ensuring no EP ever floats in empty space.
- **Prune the EP list** after each placement: remove EPs that are inside existing items (interior), remove exact duplicates, and remove dominated points (those that are strictly farther from the origin in every axis than another surviving EP).
- **Sort EPs** by ascending z (lower positions tried first to discourage unstable columns), with distance to origin as the tiebreaker.
- **Execute the placement loop**: iterate the sorted EP list, try both item orientations, check bounds + AABB non-collision + support, commit on the first passing attempt, and return `false` (penalise the individual in the GA) if all EPs are exhausted.
- **Hand off Phase 1 blocks to Phase 2** by seeding the EP list from every placed item in the container, so residuals can be stacked on top of blocks or placed in any remaining empty pallet space.

### 1.2 Design Decisions

**`namespace ExtremePointEngine` instead of a class.** All six functions are stateless: they accept the EP list and Container by reference and mutate them directly. There is no persistent state between calls. This is the same idiom used by `LayerGenerator` and `BlockBuilder` — a namespace for a collection of related pure operations.

**Inclusive bounds in `projectZ`, half-open bounds in `isInterior`.** These two predicates use different bound conventions intentionally. `projectZ` asks "does item pi's top face support a point at (px, py)?". A point exactly on the far edge of an item (px == pi.x + pi.dx) is geometrically touching the item's corner and is a valid resting position — inclusive bounds are correct. `isInterior` asks "does the EP lie inside item pi's volume?". An EP on the boundary of the volume does not occupy the interior — half-open bounds `[pi.x, pi.x+pi.dx)` are consistent with the AABB convention used throughout the codebase.

**`generateFrom` creates raw EPs; `project` is always called immediately after.** The three canonical EP positions generated from a placed item (right, behind, top) are at the same z as the item's base or top. The right and behind EPs may be floating — nothing may be directly below them at that height. `project` snaps them to the correct resting surface. Separating generation from projection keeps each function to a single responsibility and makes the pipeline easy to reason about: generate → project → prune → sort.

**Dominance pruning after interior pruning.** The order matters. Interior pruning runs first, so every EP remaining in the list is at a valid (non-colliding) coordinate. When dominance is then checked — EP A dominates EP B if `A.x ≤ B.x && A.y ≤ B.y && A.z ≤ B.z` with at least one strict inequality — the dominating EP A is guaranteed to be a real candidate, not a phantom inside a placed item.

**COM stability is excluded from `placeItem`; support is included.** The paper classifies Constraint 3 (center-of-mass stability) as a soft constraint: the algorithm tries to satisfy it but does not reject solutions that violate it. It is evaluated by the GA's fitness functions, not used as a placement gate. Excluding it from `placeItem` avoids a subtle practical problem: `CenterOfMass::compute` returns `{0, 0, 0}` when all item masses are zero (the default for test items), which would make `isStable` fail for every placement — the COM of the origin is 600 mm from the Euro pallet's geometric centre, far outside the 60 mm deviation limit. Constraint 4 (support) is also classified as soft, but is included here because EP projection only guarantees that a surface exists below the new item; it does not verify that the surface covers a sufficient fraction of the item's base area. Without `SupportChecker`, large items placed at corners of small supporters would be accepted by the engine, producing physically unstable arrangements that the fitness function cannot correct.

**`placeItem` uses iterator-based erasure and returns immediately on success.** After finding a valid EP, the function erases it from the vector using the live iterator (avoiding index arithmetic), commits the item, regenerates EPs from the new placement, and returns `true`. The immediately-following `generateFrom → project → prune → sortEPs` sequence keeps the EP list consistent for the next call. Returning after the first successful placement matches the paper's specification: the GA calls `placeItem` once per item type, cycling through the chromosome in sequence.

**Phase 1 → Phase 2 handoff uses `generateFrom` per placed item, not raw top-face corners.** The paper says "the EPs corresponding to the vertices of the top surfaces of the items in all blocks are immediately made available." Using `generateFrom` (which produces right, behind, and top EPs) rather than the four literal top-face corners is more complete: it also creates EPs adjacent to the block edges at floor level (for filling empty pallet space beside the block), which the pure top-face interpretation would miss. After projection, EPs between items within the block are pruned as interior; EPs on top of the block and beside the block both survive.

### 1.3 Files Created or Modified in This Phase

| File | Status | Purpose |
|------|--------|---------|
| `include/ExtremePointEngine.h` | Created | Declarations for `init`, `generateFrom`, `project`, `prune`, `sortEPs`, `placeItem` |
| `src/ExtremePointEngine.cpp` | Created | EP engine: projection, pruning, dominance, placement loop |
| `test/test_extreme_points.cpp` | Created | 16 tests covering all Phase 5 functionality |
| `CMakeLists.txt` | Modified | Added `ExtremePointEngine.cpp` and `test_extreme_points.cpp` to both targets |

### 1.4 `Config.h` Additions

No new constants were added to `Config.h` in this phase. The EP engine uses existing constants:

| Constant | Used by | Why |
|----------|---------|-----|
| `PALLET_L`, `PALLET_W`, `PALLET_H` | `Container` default fields | Out-of-bounds EP removal in `project` |
| `SUPPORT_VERTEX_INSET`, `SUPPORT_TIER*` | `SupportChecker` (called from `placeItem`) | Tiered area/vertex support thresholds |

### 1.5 Prerequisites

- Phase 1 complete: `Types.h`, `Config.h`, `ExtremePoint` struct in place.
- Phase 3 complete: `AABB.h`, `SupportChecker.h/.cpp`, `CenterOfMass.h/.cpp` available.
- Phase 4 complete: `BlockBuilder.h/.cpp` provides the containers with Phase 1 blocks that `init` consumes.
- All 73 Phase 1–4 tests passing before Phase 5 begins.

---

## Step 1: Implement `init` and the Phase 1 → Phase 2 handoff — Tasks 5.1 and 5.7

### Purpose

Before the GA placement loop can begin, the EP list for each container must be seeded. There are two cases:

1. **Empty container**: Phase 2 is placing residuals onto a fresh pallet. The only valid starting position is the origin `{0, 0, 0}`.
2. **Container with Phase 1 blocks**: The pallet already holds packed items. Residuals can be stacked on top of the blocks or placed in empty pallet space beside them. The EP list must reflect both opportunities.

### `init` implementation

```cpp
void init(std::vector<ExtremePoint>& eps, const Container& cont) {
    eps.clear();

    if (cont.items.empty()) {
        eps.push_back({0, 0, 0});
        return;
    }

    for (const PlacedItem& pi : cont.items) {
        generateFrom(pi, eps);
    }
    project(eps, cont);
    prune(eps, cont);
    sortEPs(eps);
}
```

For an empty container the function returns immediately after pushing the single origin EP — no projection or pruning is needed because the origin is always valid.

For a container with blocks, `generateFrom` is called for every already-placed item. This produces three raw EPs per item (right, behind, top — detailed in Step 2). After `project`, EPs that were floating between block items snap to the correct surface; EPs at the right or back edge of the last item in a block snap to the floor at z = 0 if there is no item beneath them in that XY column. After `prune`, EPs between items in the same block layer are removed as interior (they would immediately collide with an adjacent item). The survivors are the EPs on top of the block and the EPs at the block's outer edges facing empty pallet space.

**Why use `generateFrom` for handoff rather than the literal top-face corners?** Consider a block that fills only the left half of the pallet (x: 0–600). The top-face-corner approach would add EPs only at z = block_height — it would miss the EP at `{600, 0, 0}` (floor level, right of the block) where a residual could be placed in the empty right half. `generateFrom` naturally produces `{pi.x + pi.dx, pi.y, pi.z}` for the rightmost items in the block, which projects to z = 0 in empty space, making that floor region reachable.

**Key points:**
- `eps.clear()` at the start prevents stale EPs from a previous call contaminating the new list.
- The empty-container fast path avoids unnecessary work and keeps the function's contract explicit.
- Calling the full `project → prune → sortEPs` pipeline after seeding from blocks ensures the list is in a consistent, ready-to-use state before the GA calls `placeItem` for the first time.

---

## Step 2: Implement `generateFrom` — Task 5.2

### Purpose

The EP strategy's core insight: every newly placed item creates three new positions where the next item may go. These positions are derived from three faces of the placed item.

### Paper specification (Section IV-B-3)

"If an item is positioned at `[x_i, y_i, z_i]`, the following three new EPs are made available for subsequent placements: `[l_i + x_i, y_i, z_i]`, `[x_i, w_i + y_i, z_i]`, and `[x_i, y_i, h_i + z_i]`."

### Implementation

```cpp
void generateFrom(const PlacedItem& pi, std::vector<ExtremePoint>& eps) {
    eps.push_back({pi.x + pi.dx, pi.y,        pi.z        });  // right face
    eps.push_back({pi.x,         pi.y + pi.dy, pi.z        });  // back face
    eps.push_back({pi.x,         pi.y,         pi.z + pi.dz}); // top face
}
```

The function uses `pi.dx`, `pi.dy`, `pi.dz` (post-rotation effective dimensions) rather than looking up the `ItemType`. This is correct: `PlacedItem` stores its actual dimensions after any rotation has been applied, so the EP positions are always computed from the item as it physically sits.

**Why three faces?** The right and back EPs allow packing more items at the same height level, building a dense horizontal layer. The top EP allows stacking — placing an item directly on top. Together they explore both horizontal and vertical packing directions.

**Key points:**
- These are raw, unvalidated EPs. They are not projected, pruned, or sorted here. The caller always follows `generateFrom` with `project → prune → sortEPs`.
- The right and back EPs share z with the placed item's base. They may be floating (nothing may be below them at that z). `project` will snap them to the correct surface in the next step.
- The top EP is at `pi.z + pi.dz` which is always on a real surface (the item's own top face). Projection will confirm this and leave it unchanged.

---

## Step 3: Implement `project` — Task 5.3

### Purpose

Raw EPs from `generateFrom` may be floating in mid-air. An EP at `{x+dx, y, z}` is at the item's base height, but unless there is another item or the floor at exactly that height below `{x+dx, y}`, nothing is there to support the next item. `project` snaps each EP downward to the nearest real surface.

### Internal helper `projectZ`

```cpp
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
```

`max_z` is the raw EP height. Only surfaces at or below it are candidates — this prevents an EP from being pushed upward. Inclusive bounds `[pi.x, pi.x+pi.dx]` are used because a point exactly on the far edge of an item (e.g. `px == pi.x + pi.dx`) is geometrically touching the corner of the item's top face and is a valid resting surface.

### Out-of-bounds removal

After projecting, EPs whose `x ≥ cont.L`, `y ≥ cont.W`, or `z ≥ cont.H` are removed. An item placed starting at `x = L` would immediately exceed the container wall. The erase–remove idiom handles this in a single O(n) pass.

**Key points:**
- `best = 0` initialises to the floor, so the function always returns a valid z even when no item covers the point.
- The loop is O(n) in the number of placed items. For the EP list sizes and item counts encountered in Phase 2, this is fast. A spatial index (e.g. height map grid) would be the natural optimisation if profiling identified this as a bottleneck.
- Projection is idempotent: calling it twice on the same list with the same container produces the same result. This means the `project → prune → sort` pipeline can safely re-project previously-valid EPs when new items are added.

---

## Step 4: Implement `prune` — Task 5.4

### Purpose

After projection the EP list may contain points that are useless (interior to a placed item, exact copies of another EP, or strictly dominated by another EP). Pruning keeps the list compact and avoids redundant constraint checks in the placement loop.

### Three pruning steps

**Step 1 — Interior removal.** An EP is interior if it lies inside a placed item's half-open volume `[pi.x, pi.x+pi.dx) × [pi.y, pi.y+pi.dy) × [pi.z, pi.z+pi.dz)`. Any item placed at an interior EP would immediately register an AABB collision. Removing these avoids wasted constraint checks.

Half-open bounds are used here (consistent with the AABB convention). An EP at `px == pi.x + pi.dx` (right face) is NOT interior — items can be placed touching that face.

**Step 2 — Deduplication.** After `std::sort` (needed for `std::unique` to find adjacent equal triples), `std::unique` with a custom comparator removes exact `{x, y, z}` duplicates. Multiple `generateFrom` calls (e.g. two items that share a face) often produce the same EP.

**Step 3 — Dominated EP removal.** EP B is dominated by EP A if:
```
A.x ≤ B.x  AND  A.y ≤ B.y  AND  A.z ≤ B.z  AND  A ≠ B
```
A is at least as close to the origin in every axis. Since `sortEPs` always tries lower-z and closer-to-origin EPs first, A will always be attempted before B. If A succeeds, B was never needed. If A fails (collision), then B might succeed but the heuristic accepts this loss in exchange for a shorter EP list. This is standard practice in EP-based bin packing literature.

Dominance is checked after interior removal so that every surviving "dominator" A is itself a valid (non-interior) candidate.

```cpp
std::vector<bool> dominated(n, false);
for (int i = 0; i < n; ++i) {
    if (dominated[i]) continue;
    for (int j = 0; j < n; ++j) {
        if (i == j || dominated[j]) continue;
        if (eps[i].x <= eps[j].x && eps[i].y <= eps[j].y && eps[i].z <= eps[j].z &&
            (eps[i].x < eps[j].x || eps[i].y < eps[j].y || eps[i].z < eps[j].z)) {
            dominated[j] = true;
        }
    }
}
```

The O(n²) loop is acceptable because the EP list is short in practice (typically 10–50 entries at any point during Phase 2 placement).

**Key points:**
- The three steps run in order: interior → dedup → dominated. Running dedup before dominance avoids comparing an EP against its own duplicates.
- `std::remove_if` + `erase` (erase–remove idiom) is used for interior removal and out-of-bounds removal in `project`. This is O(n) and avoids iterator invalidation.

---

## Step 5: Implement `sortEPs` — Task 5.5

### Paper specification

"EPs are ranked based on their height. Lower EPs score higher to discourage the GA from building columns of items. If two EPs are at the same height then the one closer to the origin of the pallet is ranked first."

### Implementation

```cpp
void sortEPs(std::vector<ExtremePoint>& eps) {
    std::sort(eps.begin(), eps.end(),
        [](const ExtremePoint& a, const ExtremePoint& b) {
            if (a.z != b.z) return a.z < b.z;
            const int da = a.x * a.x + a.y * a.y;
            const int db = b.x * b.x + b.y * b.y;
            return da < db;
        });
}
```

**Why `x²+y²` rather than `sqrt(x²+y²)`?** The squared distance preserves the same ordering as the Euclidean distance (both are monotonically increasing for non-negative values), but uses only integer arithmetic — no floating-point computation, no rounding, no `<cmath>` dependency. The values fit comfortably in `int` for Euro pallet coordinates (max 1200²+800²=2,080,000, well within `INT_MAX`).

**Key points:**
- Sorting by z ascending prevents tall column-building which would produce poor stability and COM scores.
- The origin-proximity tiebreaker pushes items toward the back-left corner of the pallet, which is the conventional packing direction and aligns with the paper's coordinate system (origin at left-bottom).

---

## Step 6: Implement `placeItem` — Task 5.6

### Overview

`placeItem` is the function the GA calls once per residual item type in chromosome order. It performs one placement attempt: iterate the sorted EP list, try both orientations, check hard constraints, commit on the first passing attempt. Return `false` if all EPs are exhausted.

### Constraint checks

| Constraint | Check | Rationale |
|------------|-------|-----------|
| Bounds | `AABB::fitsInContainer` | Item must not extend beyond any container wall |
| Non-collision (hard, C2) | `AABB::overlaps` against all items | No volume sharing |
| Support (soft, C4) | `SupportChecker::isSupported` | EP projection guarantees a surface exists below; `SupportChecker` verifies the surface covers sufficient base area |
| COM stability (soft, C3) | **Not checked** | Soft constraint evaluated by GA fitness; zero-mass test items cause `CenterOfMass::compute` to return `{0,0,0}`, making `isStable` always fail (COM distance to pallet center ≈ 720 mm >> 60 mm limit) |

### Placement loop

```cpp
for (auto it_ep = eps.begin(); it_ep != eps.end(); ++it_ep) {
    const int ex = it_ep->x, ey = it_ep->y, ez = it_ep->z;

    for (const Orientation ori : {Orientation::Original, Orientation::Rotated90}) {
        // Build candidate PlacedItem
        PlacedItem pi;
        pi.item_type_index = type_idx;
        pi.orientation = ori;
        pi.x = ex; pi.y = ey; pi.z = ez;
        pi.dx = (ori == Orientation::Original) ? it.l : it.w;
        pi.dy = (ori == Orientation::Original) ? it.w : it.l;
        pi.dz = it.h;

        if (!AABB::fitsInContainer(pi, cont)) continue;
        // ... collision and support checks ...

        // Commit
        cont.items.push_back(pi);
        it_ep = eps.erase(it_ep);          // remove used EP
        generateFrom(pi, eps);             // 3 new raw EPs
        project(eps, cont);
        prune(eps, cont);
        sortEPs(eps);
        return true;
    }
}
return false;
```

**Why copy `ex, ey, ez` from the iterator before erasing?** After `eps.erase(it_ep)`, the iterator (and the reference `*it_ep`) is invalidated. The values are copied to local variables before any mutation of `eps` occurs, so the `generateFrom` and subsequent calls use the correct committed position.

**Why erase the used EP before `generateFrom`?** The paper specifies "each EP may only support one placement." Erasing before regenerating also ensures the old EP cannot appear as a duplicate in the freshly generated set — `prune`'s deduplication step would catch it anyway, but the explicit erase is cleaner.

**Key points:**
- Iterating over orientations in a fixed order (`Original` first) matches the paper: "attempt to place an item in the primary orientation → if fail, try secondary orientation."
- The full `project → prune → sortEPs` pipeline runs after every successful placement to keep the list consistent. This is O(n²) in the EP list size per placement, which is acceptable for the expected list sizes.
- `return false` signals the GA to penalise the individual with maximum fitness scores on both objectives.

---

## Step 7: Write `test/test_extreme_points.cpp` — Task 5.8

16 test cases across the full `ExtremePointEngine` namespace:

### Initialisation (2 cases)

| Test | What it checks |
|------|----------------|
| `InitEmptyContainerProducesSingleOriginEP` | Empty container → exactly 1 EP at `{0, 0, 0}` |
| `InitWithBlockProducesTopSurfaceEP` | Container with one placed item → EP list contains at least one EP at z = item height |

### EP Generation (1 case)

| Test | What it checks |
|------|----------------|
| `GenerateFromProducesThreeEPs` | Item at `{100, 50, 0}` with dx=300,dy=200,dz=100 → exactly 3 EPs at `{400,50,0}`, `{100,250,0}`, `{100,50,100}` |

### Projection (3 cases)

| Test | What it checks |
|------|----------------|
| `ProjectionSnapsToFloorWhenNothingBelow` | EP at `{500, 0, 300}` with no items → snaps to z=0 |
| `ProjectionSnapsToItemTopFace` | EP at `{0, 0, 200}` with item top at z=100 → snaps to z=100 |
| `ProjectionRemovesOutOfBoundsEP` | EPs at `x=L` and `y=W` are removed; EP at `{0,0,0}` survives |

### Pruning (4 cases)

| Test | What it checks |
|------|----------------|
| `PruneRemovesInteriorEP` | EP inside `[0,600)×[0,400)×[0,200)` → removed |
| `PruneKeepsEPOnItemFace` | EP at `x = pi.x + pi.dx` (right face) → survives (not interior) |
| `PruneDeduplicates` | Three copies of `{0,0,0}` → one survives |
| `PruneRemovesDominatedEP` | `{100,50,0}` dominated by `{0,0,0}` → only `{0,0,0}` survives |

### Sorting (1 case)

| Test | What it checks |
|------|----------------|
| `SortByZThenByDistanceToOrigin` | Mixed z and xy EPs → sorted ascending z, ascending `x²+y²` within same z |

### Placement procedure (3 cases)

| Test | What it checks |
|------|----------------|
| `PlaceItemOnEmptyPalletSucceeds` | Single item onto empty pallet → returns true; item at `{0,0,0}` |
| `PlaceMultipleItemsNoOverlap` | 4 sequential placements → all succeed; O(N²) AABB check confirms no overlap |
| `PlaceItemReturnsFalseWhenNoEPFits` | Item larger than container on every axis → returns false; container unchanged |

### Phase 1 → 2 handoff (2 cases)

| Test | What it checks |
|------|----------------|
| `HandoffFindsAdjacentFloorEP` | Block covers left half of pallet → EP at `{600, 0, 0}` exists for placing in empty right half |
| `HandoffPlaceOnTopOfBlock` | Full-base block at height 100 → placement succeeds; placed item has z = 100 |

---

## Step 8: Update `CMakeLists.txt`

Two new files were added to both the main executable and the test target:

```cmake
add_executable(GA_3DBPP
    ...
    src/ExtremePointEngine.cpp    # added
)

add_executable(GA_3DBPPTests
    ...
    test/test_extreme_points.cpp  # added
    src/ExtremePointEngine.cpp    # added
)
```

`ExtremePointEngine.cpp` includes `AABB.h` and `SupportChecker.h` — the corresponding `.cpp` files (`SupportChecker.cpp`) were already present in both targets from Phase 3. No new `target_link_libraries` calls were needed.

---

## Final file structure added in Phase 5

```
include/
└── ExtremePointEngine.h   # init(), generateFrom(), project(), prune(),
                           # sortEPs(), placeItem()

src/
└── ExtremePointEngine.cpp # projectZ() helper, isInterior() helper,
                           # full EP pipeline implementation

test/
└── test_extreme_points.cpp  # 16 cases: init, generation, projection,
                             #  pruning, sorting, placement, handoff
```

Full test suite after Phase 5: **89 tests, 100% passing** under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## What changes in Phase 6

Phase 6 implements the NSGA-II genetic algorithm that drives Phase 2. The GA evolves permutations of residual item type indices (chromosomes). For each chromosome, `placeItem` is called once per item type in the chromosome's order — this is the `Packer::decode` step. The GA evaluates two fitness objectives: minimise item type heterogeneity per pallet (Fitness 1) and maximise compactness of residual item placements (Fitness 2). The initial population includes 10 seeded individuals encoding placement sequences ordered by weight, quantity, base area, volume, and volume×quantity (ascending and descending), plus random permutations to fill the remaining 90 slots.