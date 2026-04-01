# Phase 3: Geometric Constraint Modules
**Environment:** Ubuntu 24.04, GCC 13.3.0, C++20, CMake 3.28.3 + Ninja, vcpkg (x64-linux)

---

## 1. Introduction

### 1.1 What This Phase Accomplishes

Phase 2 gave the program the ability to read problem instances and write results. Phase 3 gives it the ability to *reason about space*. Every constraint the packing engine must enforce — no two items occupy the same volume, items don't float in the air, the pallet won't tip, layers interlock for stability — is implemented here as a standalone, independently testable module.

By the end of Phase 3 the program has four geometric utilities:

- **`AABB`** — detects volume overlap between two placed items and checks that an item sits within container bounds. Used by the placement engine on every attempted placement.
- **`SupportChecker`** — verifies that an item has adequate physical support from below using a three-tier test combining vertex coverage and base area overlap.
- **`CenterOfMass`** — computes the mass-weighted centroid of all items in a container and checks whether it deviates too far from the pallet's geometric center.
- **`Hausdorff`** — measures how well two 2D point sets interlock, used in Phase 4 to select the best symmetry variant when stacking layers.

None of these modules depend on each other. Each can be tested in isolation, which is why Phase 3 introduces four separate test files rather than one combined `test_geometry.cpp`.

### 1.2 Design Decisions

**`AABB` is header-only.** Both functions (`overlaps` and `fitsInContainer`) are short enough to inline at every call site. Marking them `inline` in the header avoids ODR violations when multiple translation units include `AABB.h`. There is no state to encapsulate, so a class would add ceremony without value.

**Strict `<` for item-to-item overlap, `<=` for container bounds.** Two items sharing a face have no interior volume in common — `<` correctly returns `false`. An item whose far corner touches a container wall is fully inside the container — `<=` correctly returns `true`. The distinction is not a subtle edge case; it is the definition of valid packing.

**`SupportChecker` is a class with an `inset_` member.** The vertex inset is a configurable algorithm parameter that persists across multiple calls. Storing it in the object avoids passing it as an argument every time, which would make the call site noisier and easier to misuse. Tests use `inset = 0` to keep expected values exact; production code uses `Config::SUPPORT_VERTEX_INSET = 10mm`.

**`CenterOfMass` uses `static` methods.** The class has no member state — both methods are pure computations on their arguments. `static` methods belong to the class (not to any instance), so callers write `CenterOfMass::compute(...)` without constructing an object. This is the class-based equivalent of a free function, used here to group the two related utilities under one name.

**`Hausdorff` uses an anonymous namespace for its `directed` helper.** The helper function is implementation detail that no other translation unit should ever call. An anonymous namespace gives it internal linkage cleanly, without the per-symbol `static` keyword. It is the idiomatic modern C++ approach for file-local symbols.

### 1.3 Files Created in This Phase

| File | Purpose |
|---|---|
| `include/AABB.h` | Header-only: `AABB::overlaps()`, `AABB::fitsInContainer()` |
| `include/SupportChecker.h` | Three-tier support check class declaration |
| `src/SupportChecker.cpp` | Vertex inset, area overlap, tiered threshold logic |
| `include/CenterOfMass.h` | Static COM computation + stability check |
| `src/CenterOfMass.cpp` | Weighted centroid + ±60mm deviation test |
| `include/Hausdorff.h` | Symmetric Hausdorff distance declaration |
| `src/Hausdorff.cpp` | Directed + symmetric Hausdorff over 2D integer point sets |
| `test/test_aabb.cpp` | 8 AABB test cases |
| `test/test_support.cpp` | 6 SupportChecker test cases |
| `test/test_com.cpp` | 4 CenterOfMass test cases |
| `test/test_hausdorff.cpp` | 3 Hausdorff test cases |

### 1.4 `Config.h` Additions

The following constants were added to `namespace Config` to support Phase 3:

| Constant | Value | Meaning |
|---|---|---|
| `SUPPORT_VERTEX_INSET` | 10 mm | Inward offset applied to each base vertex before support testing |
| `SUPPORT_TIER1_VERTS` | 4 | Minimum supported vertices for Tier 1 |
| `SUPPORT_TIER1_AREA` | 0.40 | Minimum base area coverage for Tier 1 |
| `SUPPORT_TIER2_VERTS` | 3 | Minimum supported vertices for Tier 2 |
| `SUPPORT_TIER2_AREA` | 0.50 | Minimum base area coverage for Tier 2 |
| `SUPPORT_TIER3_VERTS` | 2 | Minimum supported vertices for Tier 3 |
| `SUPPORT_TIER3_AREA` | 0.75 | Minimum base area coverage for Tier 3 |
| `COM_MAX_DEVIATION` | 60 mm | Maximum XY displacement of COM from pallet geometric center |

### 1.5 Prerequisites

- Phase 1 complete: `Types.h`, `Config.h` in place.
- Phase 2 complete: build system, test infrastructure, and I/O modules working.
- All 36 Phase 1–2 tests passing before Phase 3 begins.

---

## Step 1: Implement `AABB` — header-only collision detection

`AABB.h` contains two `inline` free functions inside `namespace AABB`. Both are header-only because they are short, called frequently, and have no state to encapsulate.

### `overlaps` — strict interior volume test

Two axis-aligned boxes share interior volume if and only if they overlap simultaneously on all three axes. On a single axis, intervals `[a, a+da)` and `[b, b+db)` overlap strictly when `a < b + db` AND `b < a + da`. The `<` (not `<=`) is intentional: boxes sharing a face touch but do not penetrate. In a valid packing, every adjacent pair of items touches at a face, so using `<=` would flag every valid solution as a collision.

```cpp
[[nodiscard]] inline bool overlaps(const PlacedItem& a, const PlacedItem& b)
{
    return a.x < b.x + b.dx && b.x < a.x + a.dx &&
           a.y < b.y + b.dy && b.y < a.y + a.dy &&
           a.z < b.z + b.dz && b.z < a.z + a.dz;
}
```

### `fitsInContainer` — bounds check

An item fits if its near corner is non-negative on all axes and its far corner does not exceed the container's corresponding dimension. The far corner uses `<=` (not `<`) — an item flush with the container wall is valid.

```cpp
[[nodiscard]] inline bool fitsInContainer(const PlacedItem& p, const Container& c)
{
    return p.x >= 0       && p.y >= 0       && p.z >= 0      &&
           p.x + p.dx <= c.L &&
           p.y + p.dy <= c.W &&
           p.z + p.dz <= c.H;
}
```

**Key points:**
- `inline` is mandatory for any non-template function defined in a header. Without it, every `.cpp` that includes `AABB.h` produces a definition of `overlaps`, and the linker sees duplicate symbols — an ODR violation.
- `[[nodiscard]]` on both functions: silently discarding the result of a collision check is almost certainly a bug. The attribute promotes that mistake to a compiler warning.
- The six comparisons compile to six integer comparisons with short-circuit evaluation — the cheapest possible collision test for axis-aligned geometry.

### `test_aabb.cpp` test cases

| Case | Setup | Expected |
|---|---|---|
| `SeparateItemsDoNotOverlap` | Gap of 100 mm on X | `false` |
| `TouchingFacesDoNotOverlap` | Far corner of A = near corner of B | `false` |
| `OneMmOverlapDetected` | B starts 1 mm inside A | `true` |
| `SamePositionOverlaps` | Identical items | `true` |
| `ContainedItemOverlaps` | B entirely inside A | `true` |
| `OverlapOnTwoAxesButNotThirdIsNotOverlap` | XY overlap, touching Z faces | `false` |
| `ItemExactlyFitsInContainer` | Item fills entire pallet | `true` |
| `ItemExceedsContainerLength` | Item 1 mm too long | `false` |

---

## Step 2: Implement `SupportChecker` — three-tier support verification

Support checking answers: *is this item physically held up by what is below it?* The answer depends on two quantities — how many of the item's four base corners land on a supporting surface, and what fraction of the item's base area is covered from below.

### Finding supporters

A supporter is any already-placed item whose top face (`z + dz`) is exactly equal to the new item's bottom face (`z`). A gap of even 1 mm means no physical contact and no support. The `isSupported` caller passes all other items in the container; the method filters them internally.

### Vertex inset

Before testing whether a vertex is supported, each of the four base corners is translated inward by `inset_` mm:

```
Near-left:  (x + inset,      y + inset)
Near-right: (x + dx - inset, y + inset)
Far-left:   (x + inset,      y + dy - inset)
Far-right:  (x + dx - inset, y + dy - inset)
```

This "inward translation" is the paper's practical adjustment: it prevents items that barely touch at an edge from registering as vertex-supported. The default inset of 10 mm is a configurable tolerance.

### Base area coverage

For each supporter, compute the 2D overlap between the item's base footprint and the supporter's footprint using the standard interval-overlap formula on each axis, clamped to zero:

```
overlap_x = max(0, min(item.x + item.dx, supp.x + supp.dx) - max(item.x, supp.x))
overlap_y = max(0, min(item.y + item.dy, supp.y + supp.dy) - max(item.y, supp.y))
covered  += overlap_x * overlap_y
```

Coverage = total covered area / item base area. Overlapping supporters (which would violate AABB) are not deducted for — in a valid packing this cannot occur.

### Tiered logic

```cpp
if (item.z == 0)                                              return true;  // floor
if (supporters.empty())                                       return false; // floating
if (verts >= TIER1_VERTS && coverage >= TIER1_AREA)          return true;
if (verts >= TIER2_VERTS && coverage >= TIER2_AREA)          return true;
if (verts >= TIER3_VERTS && coverage >= TIER3_AREA)          return true;
return false;
```

The three tiers allow partial overhangs: a long item can be adequately supported even if one corner has nothing below it, as long as enough area and vertices are covered.

### `test_support.cpp` test cases

All tests use `inset = 0` so vertex positions coincide exactly with footprint corners and expected values require no offset arithmetic.

| Case | Setup | Expected |
|---|---|---|
| `FloorIsAlwaysSupported` | Item at z=0, no other items | `true` |
| `FullSupportTier1` | Supporter larger than item in XY | `true` (4 verts, 100% area) |
| `ThreeVerticesTier2` | Two non-overlapping supporters covering 3 of 4 corners, 83% area | `true` (Tier 2) |
| `TwoVerticesTier3` | Supporter covers left 75% of item base | `true` (Tier 3) |
| `FloatingNotSupported` | Supporter top face 100 mm below item bottom | `false` |
| `BelowThresholdNotSupported` | Supporter covers only 25% of base | `false` |

Note: getting exactly 3 vertices requires two non-overlapping supporters that together cover three corners. A single rectangular supporter always supports 0, 2, or 4 corners of a rectangular item — because corners come in symmetric pairs on each axis.

---

## Step 3: Implement `CenterOfMass` — weighted centroid and stability

### `compute`

The COM is the mass-weighted average position of all placed items:

```
COM.x = Σ(mass_i × (x_i + dx_i/2)) / Σ(mass_i)
COM.y = Σ(mass_i × (y_i + dy_i/2)) / Σ(mass_i)
COM.z = Σ(mass_i × (z_i + dz_i/2)) / Σ(mass_i)
```

Each item's geometric centre on one axis is `pos + dim/2.0` — dividing by `2.0` (not `2`) ensures floating-point arithmetic rather than integer truncation. Mass is cast to `double` before multiplication to prevent integer overflow on large values. Returns `{0, 0, 0}` when the container is empty.

### `isStable`

Stability is a 2D (horizontal) question. The pallet's geometric center is `(L/2, W/2)`. The COM is stable if:

```
|COM.x - L/2| <= Config::COM_MAX_DEVIATION   (60 mm)
|COM.y - W/2| <= Config::COM_MAX_DEVIATION   (60 mm)
```

`std::abs` from `<cmath>` must be used — the integer overload from `<cstdlib>` would truncate the `double` argument before computing the absolute value.

### `test_com.cpp` test cases

| Case | Setup | Expected |
|---|---|---|
| `EmptyReturnsZero` | No items | COM = `{0,0,0}` |
| `SingleCenteredItemIsStable` | 400×400×400 item centered on 1200×800 pallet | COM = (600, 400), stable |
| `SymmetricItemsAreStable` | Two equal-mass items mirrored about pallet centre | COM = (600, 400), stable |
| `AsymmetricHeavyItemIsUnstable` | Mass-100 item at far corner | COM = (1100, 700), unstable (Δ=500mm, 300mm) |

---

## Step 4: Implement `Hausdorff` — layer interlocking distance

### Definition

The symmetric Hausdorff distance between point sets A and B:

```
H(A, B) = max( directed(A→B),  directed(B→A) )

directed(A→B) = max over a∈A  of  min over b∈B  of  dist(a, b)
```

The directed distance answers: "for the point in A that is farthest from its nearest neighbour in B, how far away is that nearest neighbour?" Taking the max of both directions gives a symmetric metric.

### Implementation

`directed` lives in an anonymous namespace — internal to `Hausdorff.cpp`, invisible to all other translation units. It initialises each running minimum with `std::numeric_limits<double>::infinity()` so the first real distance always replaces it. Integer coordinates are cast to `double` before squaring to prevent overflow (`1000² = 1,000,000`, which fits in an int, but `3000² = 9,000,000` does too — however making it a habit with `double` avoids the class of bugs entirely).

### Packing application

In Phase 4, this function is applied to the XY corner vertices of items in adjacent layers. The algorithm tests four symmetry variants of each layer (original, H-flip, V-flip, HV-flip) and selects the variant with the highest Hausdorff distance — meaning the corners of one layer reach deepest into the gaps of the layer below, improving interlocking under transport loads.

### `test_hausdorff.cpp` test cases

| Case | Setup | Expected |
|---|---|---|
| `IdenticalSetsDistanceIsZero` | A = B (4 corners of a 100×100 square) | 0.0 |
| `ShiftedSetsDistanceEqualsShift` | B = A translated 100 mm on X | 100.0 |
| `DisjointSetsDistanceIsGap` | A at y=0, B at y=300, same X coords | 300.0 |

---

## Step 5: `CMakeLists.txt` updates

All Phase 3 `.cpp` files were added to both the main executable and the test target. All four test files were added to the test target. Header-only `AABB.h` needs no entry.

```cmake
add_executable(GA_3DBPP
    src/main.cpp
    src/BRReader.cpp
    src/CSVReader.cpp
    src/JSONWriter.cpp
    src/Logger.cpp
    src/SupportChecker.cpp   # added
    src/CenterOfMass.cpp     # added
    src/Hausdorff.cpp        # added
)

add_executable(GA_3DBPPTests
    ...
    test/test_aabb.cpp       # added
    test/test_support.cpp    # added
    test/test_com.cpp        # added
    test/test_hausdorff.cpp  # added
    src/SupportChecker.cpp   # added
    src/CenterOfMass.cpp     # added
    src/Hausdorff.cpp        # added
)
```

---

## Final file structure added in Phase 3

```
include/
├── AABB.h             # Header-only: overlaps() + fitsInContainer()
├── SupportChecker.h   # Three-tier support class (inset, vertex count, area)
├── CenterOfMass.h     # Static compute() + isStable()
└── Hausdorff.h        # Symmetric Hausdorff distance declaration

src/
├── SupportChecker.cpp # Supporter filtering, vertex inset, interval area overlap
├── CenterOfMass.cpp   # Weighted centroid, std::abs deviation check
└── Hausdorff.cpp      # Anonymous-namespace directed(), symmetric distance()

test/
├── test_aabb.cpp      # 8 cases: overlap, touching, contained, same pos, bounds
├── test_support.cpp   # 6 cases: floor, Tier 1/2/3, floating, below threshold
├── test_com.cpp       # 4 cases: empty, centered, symmetric, asymmetric
└── test_hausdorff.cpp # 3 cases: identical, shifted, disjoint
```

Full test suite after Phase 3: **57 tests, 100% passing** under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## What changes in Phase 4

Phase 4 implements the paper's constructive Phase 1 heuristic: single-item-type layer generation, dynamic shifting, layer classification (full/half/quarter), layer merging, and block building. It is the first phase that uses all four Phase 3 modules together — AABB prevents overlapping placements, SupportChecker validates each layer's support, CenterOfMass monitors stability as blocks grow, and Hausdorff selects the best interlocking symmetry variant when stacking layers.
