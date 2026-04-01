# Phase 1: Core Data Structures
**Environment:** Ubuntu 24.04, GCC 13.3.0, C++20, CMake 3.28.3 + Ninja, vcpkg (x64-linux)

---

## 1. Introduction

### 1.1 What This Phase Accomplishes

Phase 0 set up the toolchain, build system, libraries, directory structure, benchmark datasets, and verified that Debug and Release configurations both compile and pass tests. At this point the project compiles and runs but does nothing — `main.cpp` is a hello-world smoke test.

Phase 1 defines the **vocabulary of the entire program** in C++. Every later phase — file I/O, geometric constraints, the packing engine, the genetic algorithm, visualization — operates on the types defined here. Getting these types right now means the rest of the codebase has a stable foundation. Getting them wrong means refactoring every file that depends on them.

By the end of Phase 1 you will have two header files:

- **`include/Types.h`** — all structs and enums that represent the problem domain (items, containers, layers, blocks, extreme points, GA individuals, and the overall packing solution).
- **`include/Config.h`** — compile-time constants (`constexpr`) for algorithm parameters (pallet dimensions, GA population size, crossover/mutation rates, constraint thresholds).

You will also have unit tests in `test/test_types.cpp` that verify construction, default values, and basic invariants of every struct.

### 1.2 Why Data Structures Come First

The algorithm in the paper operates on a chain of increasingly complex objects:

1. An **order** arrives as a list of **item types**, each with dimensions, weight, and quantity.
2. Phase 1 of the algorithm tiles identical items into **layers** (full, half, or quarter pallet coverage).
3. Layers are stacked into **blocks** on individual **containers** (pallets).
4. Residual items that didn't fit into layers are packed by the GA in Phase 2, which generates **individuals** (permutations of item types) and evaluates them by placing items at **extreme points**.
5. Each placed box becomes a **placed item** with a position and orientation inside a container.
6. The complete result is a **packing solution**: a set of containers with fitness scores.

Every noun in bold above becomes a C++ struct. The relationships between them (an order *contains* item types, a container *contains* placed items, etc.) become member fields — usually `std::vector`s of other structs, or integer indices referencing into shared vectors.

If you write packing logic before these types exist, you end up passing around raw integers and ad-hoc tuples, which produces code that is hard to read, easy to misuse, and painful to refactor. Defining the types first forces you to think precisely about what data each part of the algorithm actually needs.

### 1.3 The Paper's Data Model

The paper (Section IV-A) defines an item as a tuple `[l, w, h, m, v, q]` — length, width, height, mass, volume, and quantity. A container (pallet) is defined by `[L, W, H]`. Item positions are integer tuples `[x, y, z]` in a coordinate system with the origin at the left-bottom corner of the pallet, X along the pallet length, Y along the pallet width, and Z pointing up (see Figure 4 in the paper).

Items may only be rotated 90° around the Z-axis. This means an item with dimensions `[l, w, h]` can be placed as `[l, w, h]` (original) or `[w, l, h]` (rotated) — the height is always preserved. This constraint dramatically reduces the search space compared to allowing all 6 orthogonal orientations.

The key distinction to internalize is **item type vs. placed item**. An item type is a product description ("108×76×30mm, 2.5kg, quantity 40"). A placed item is a single physical box from that type that has been assigned a specific position, orientation, and container. One item type with quantity 40 generates up to 40 placed items during the packing process.

### 1.4 C++ Design Principles Used in This Phase

**Rule of Zero.** Every struct in `Types.h` uses only value types (`int`, `double`, `bool`) and standard containers (`std::vector`, `std::string`) as members. Because these types all manage their own resources correctly, the compiler-generated default constructor, destructor, copy constructor, move constructor, copy assignment, and move assignment are all correct. We write none of them. This eliminates an entire class of bugs (double-free, use-after-move, shallow copy of raw pointers) and keeps the code minimal.

**Array of Structures (AoS).** We store `std::vector<PlacedItem>` rather than separate vectors for each field (x-positions, y-positions, z-positions, etc.). At the problem's scale of ~50–500 items per container, the access pattern is almost always "read all fields of one item" (to check collision, compute support, evaluate fitness). AoS keeps those fields adjacent in a single cache line, which is the efficient layout for this pattern.

**Default member initializers.** Every numeric field gets an explicit default value (typically 0, -1, or 0.0) at the point of declaration. This guarantees that a default-constructed struct is always in a valid, predictable state. Without defaults, uninitialized fields contain garbage, and reading them is undefined behavior — which AddressSanitizer and UndefinedBehaviorSanitizer in your Debug build will flag immediately.

**`enum class` for categorical values.** Orientation (original vs. rotated) and layer type (full, half, quarter) are represented as scoped enumerations rather than raw integers. This provides type safety (you cannot accidentally pass an orientation where a layer type is expected) and self-documenting code (the name `Orientation::Rotated90` is unambiguous; the integer `1` is not).

**`constexpr` for algorithm parameters.** Fixed values like pallet dimensions and GA hyperparameters are declared `constexpr`, making them compile-time constants. Unlike `#define` macros, `constexpr` variables have a type, obey scoping rules, and are visible in the debugger. Unlike `const` global variables, they are guaranteed to be evaluated at compile time, not at program startup. When used in contexts that require compile-time values (template arguments, array sizes, `static_assert` conditions), `constexpr` is strictly required.

**Index-based cross-references.** When one struct needs to refer to another (e.g., a `PlacedItem` referencing which `ItemType` it came from), we store an integer index into the relevant `std::vector` rather than a pointer. Indices are trivially copyable, survive container reallocations, and serialize naturally to JSON for visualization output. The tradeoff is that you must have access to the source vector to resolve the index, but in practice the item type vector is always available in context.

### 1.5 Files Created in This Phase

| File | Purpose |
|---|---|
| `include/Types.h` | All structs and enums for the problem domain |
| `include/Config.h` | `constexpr` algorithm parameters and pallet dimensions |
| `test/test_types.cpp` | Unit tests for struct construction, defaults, and invariants |

### 1.6 Prerequisites

Before starting this phase, verify:

- Phase 0 is complete (Tasks 0.1–0.11)
- `cmake --preset debug` and `cmake --preset release` both build and pass tests
- The BR1–7 benchmark files exist in `data/br_benchmark/thpack1.txt` through `thpack7.txt`
- You have read Section IV-A ("Problem Description") of the Ananno & Ribeiro paper and understand Figure 4 (pallet coordinate system)

---

## Step 1: Create `include/Types.h` with `ItemType`

`Types.h` is the single file that will hold every struct in the project. We create it now with just the first struct — `ItemType` — and build it up step by step. Starting with `#pragma once` and the necessary standard library includes.

`ItemType` maps directly to the paper's item tuple `[l, w, h, m, v, q]`. Volume is not stored as a field because it can be derived from the dimensions — storing it would mean two sources of truth that could disagree. Instead, it is computed on demand by a helper method. The `[[nodiscard]]` attribute tells the compiler to warn if the return value is discarded, catching silent bugs where someone calls `volume()` but forgets to use the result.

```cpp
// include/Types.h
#pragma once

#include <array>
#include <vector>

#include "Config.h"

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
```

**Key points:**
- `#pragma once` — tells the compiler to include this file at most once per translation unit, preventing duplicate definition errors. It is the modern alternative to the older `#ifndef` guard pattern.
- `int l = 0` — the `= 0` is a **default member initializer**. A default-constructed `ItemType` will have all fields set to zero. Without this, the fields would contain garbage (undefined behavior).
- `const` after the method signature — means the method promises not to modify any fields of the struct. A method that only reads data should always be marked `const`.
- `[[nodiscard]]` — a C++17 attribute. If you write `item.volume();` on its own line and throw away the result, the compiler emits a warning. This catches accidental no-op calls.

---

## Step 2: Add `Orientation` enum and `PlacedItem` struct

`Orientation` is an `enum class` (scoped enumeration) with two values — `Original` and `Rotated90`. Using a named enum rather than a raw integer (0 or 1) makes code self-documenting and type-safe: the compiler will reject an integer where an `Orientation` is expected.

`PlacedItem` represents one physical box that has been placed inside a container. It records position (`x, y, z`), post-rotation dimensions (`dx, dy, dz`), which item type it came from (by index), and how it was oriented. The `extent()` and `center()` methods return the far corner and geometric center as `std::array<int, 3>` — a fixed-size array of exactly three integers, which is the right type for a 3D coordinate.

```cpp
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
```

**Key points:**
- `enum class` — unlike a plain `enum`, `enum class` values are scoped. You must write `Orientation::Original`, not just `Original`. This prevents naming collisions with other enums.
- `item_type_index = 0` — index-based cross-reference. This integer is a position into the shared `std::vector<ItemType>`. We store the index, not a copy of the `ItemType` or a pointer to it.
- `dx, dy, dz` — the post-rotation dimensions. When `orientation == Rotated90`, `dx = item.w` and `dy = item.l`; otherwise `dx = item.l` and `dy = item.w`. The height `dz` is always equal to `item.h` because only Z-axis rotation is allowed.
- `std::array<int, 3>` — a fixed-size stack-allocated array. Unlike `std::vector`, its size is known at compile time and it carries no heap allocation overhead.

---

## Step 3: Add `Container` struct

`Container` represents a single pallet. Its dimensions default to the Euro pallet standard from `Config.h` — the `Config::PALLET_L` etc. constants ensure there is one authoritative source of truth for these values. The dimensions are public fields, so any pallet size can be set at construction using C++20 designated initializers:

```cpp
Container custom{.L = 1000, .W = 600, .H = 1200};
```

`utilization()` computes the fraction of pallet volume occupied. `static_cast<double>(used)` is required before the division — without it, integer division would truncate the result to 0 for any partially-filled container. `totalWeight()` needs the shared `ItemType` vector as a parameter because mass lives on `ItemType`, not on `PlacedItem`.

```cpp
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
```

**Key points:**
- `const PlacedItem& p` in the range-based for loop — the `&` means we take a reference to each element (no copy), and `const` means we promise not to modify it. This is the correct idiom for read-only iteration over a vector.
- `static_cast<double>(used)` — explicit type conversion. This promotes the integer numerator to `double` before the division, ensuring floating-point arithmetic is used. Without it, `used / (L * W * H)` performs integer division and returns 0 for any fraction less than 1.
- `const std::vector<ItemType>& types` — passed by `const` reference. The `&` avoids copying the entire vector (potentially hundreds of elements) on every call. The `const` guarantees the function cannot modify the caller's data.

---

## Step 4: Add `PackingSolution` struct

`PackingSolution` is the top-level result object for a complete packing run. It holds all containers produced and exposes `avgUtilization()` as a single scalar fitness score. An early-return guard on `containers.empty()` prevents division by zero on a freshly constructed solution. `containers.size()` returns `size_t` (an unsigned integer type), so a `static_cast<double>` is needed to use it as a divisor without a compiler warning.

```cpp
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
```

**Key points:**
- `containers.empty()` — preferred over `containers.size() == 0`. The standard guarantees `empty()` is O(1) for all containers. It also reads as clear intent: "is this collection empty?"
- `double sum = 0.0` — initialized as `double` because `Container::utilization()` returns a `double`. Accumulating doubles into an `int` would silently truncate each value before addition.
- Nesting — `PackingSolution` owns a `std::vector<Container>`, each of which owns a `std::vector<PlacedItem>`. This entire hierarchy is managed by value types — no manual memory management anywhere.

---

## Step 5: Add `ExtremePoint` struct

An extreme point is a candidate 3D coordinate where the next item may be placed. The GA's Phase 2 placement engine maintains a live list of these points and filters it after every placement. It is the simplest struct in the project — three integer fields, no methods.

Every time a box is placed at `(x, y, z)` with dimensions `(dx, dy, dz)`, three new candidate points are generated:
- `(x + dx, y, z)` — to the right of the placed box
- `(x, y + dy, z)` — behind the placed box
- `(x, y, z + dz)` — on top of the placed box

After each placement the engine removes duplicates, out-of-bounds points, and any point now inside an existing box.

```cpp
// ExtremePoint is a candidate 3D position inside a container where the next item
// may be placed. The GA placement engine generates these at the projected corners
// of already-placed items, then tries each one when fitting the next box.
struct ExtremePoint {
    int x = 0;
    int y = 0;
    int z = 0;
};
```

**Key points:**
- No methods needed — `ExtremePoint` is pure data. The placement logic that consumes these points lives in a later phase.
- Default `(0, 0, 0)` is meaningful — it represents the bottom-left-front corner of an empty container, which is always the first valid candidate position.
- Named struct vs. `std::array<int, 3>` — both hold three integers, but `ExtremePoint` is a distinct type. The compiler will reject an `ExtremePoint` where a color or velocity triple is expected, preventing accidental misuse.

---

## Step 6: Add `Individual` struct

An `Individual` is one candidate solution in the GA population. Its `chromosome` is a `std::vector<int>` — an ordered permutation of item-type indices that the GA will evaluate and evolve. The `objectives` vector holds the multi-objective fitness scores (e.g. utilization and container count). `rank` and `crowding_distance` are NSGA-II bookkeeping values assigned by the selection algorithm.

```cpp
// Individual represents one candidate solution in the genetic algorithm population.
// chromosome is an ordered permutation of item-type indices — the GA evolves this sequence.
// objectives holds the multi-objective fitness scores evaluated from the chromosome.
// rank and crowding_distance are NSGA-II selection bookkeeping values, set by the GA engine.
struct Individual {
    std::vector<int>    chromosome;         // permutation of item-type indices
    std::vector<double> objectives;         // fitness values (e.g. utilization, # containers)
    int    rank               = 0;          // Pareto front rank (0 = best)
    double crowding_distance  = 0.0;        // NSGA-II crowding distance
};
```

**Key points:**
- `chromosome` stores indices, not copies of `ItemType` objects — one shared `ItemType` vector is the single source of truth; chromosomes just reference into it by position.
- `objectives` is a `std::vector<double>` rather than named fields — this keeps the struct agnostic about the number of objectives and allows the GA engine to populate them without knowing the struct's internals.
- `rank = 0` defaults to the best Pareto front. If the GA engine forgets to run the ranking step before selection, all individuals appear equally elite and selection degenerates to random — a subtle but critical bug. The ranking step must always run before selection.
- `crowding_distance = 0.0` — within a Pareto front, crowding distance measures how isolated an individual is from its neighbors. Higher distance = more isolated = preferred, to maintain population diversity.

---

## Step 7: Add `LayerType` enum, `Layer` and `Block` structs

These three definitions support Phase 1's constructive heuristic. A `Layer` is a horizontal slice of the pallet filled with one item type in a regular grid. `LayerType` describes how much of the pallet footprint it covers. A `Block` is a vertical stack of layers placed on a container at a known z height.

```cpp
// LayerType describes what fraction of the pallet footprint a layer covers.
// Full = entire L×W area; Half = one half; Quarter = one quarter.
enum class LayerType { Full, Half, Quarter };

// Layer represents a single horizontal slice of the pallet filled with one item type
// arranged in a regular grid. height is the item's h dimension (all items in a layer
// share the same height). item_count is how many individual boxes the grid contains.
struct Layer {
    int       item_type_index = 0;
    LayerType type            = LayerType::Full;
    int       item_count      = 0;  // number of boxes in this layer's grid
    int       height          = 0;  // layer thickness in mm (= item height)
};

// Block is a vertical stack of layers placed on a container.
// z_base is the mm height at which the bottom of this block sits.
// Layers are stacked in order: layers[0] is at z_base, layers[1] above it, and so on.
struct Block {
    std::vector<Layer> layers;
    int z_base = 0;  // bottom z position of this block on the container (mm)
};
```

**Key points:**
- `Layer` embeds no x/y position — the Phase 1 engine decides where on the footprint each layer sits at placement time, based on `LayerType` and available area. The layer just records what it contains and how thick it is.
- `Block` stores `std::vector<Layer> layers` by value (not by index) because layers have no existence outside their block. Contrast with `PlacedItem`, which references `ItemType` by index because many placed items across many containers share the same item type definitions.
- `z_base` on `Block` — the Phase 1 engine tracks a running height as it stacks blocks. A block with `z_base = 400` containing layers of height 120 and 80 has its top face at `400 + 120 + 80 = 600 mm`. The next block starts at `z_base = 600`.

---

## Step 8: Create `include/Config.h`

`Config.h` is a standalone header with no dependencies on the rest of the project. It declares all algorithm parameters as `constexpr` constants inside a `namespace Config`. The namespace prevents name collisions: you write `Config::PALLET_L`, not bare `PALLET_L`. `static_assert` statements below the namespace enforce logical constraints on the GA parameters at compile time — if you set `GA_MU` larger than `GA_POPULATION`, the build fails immediately with a clear error message before any code runs.

```cpp
// include/Config.h
#pragma once

// Config.h — compile-time algorithm parameters for GA_3DBPP.
// All constants are constexpr: typed, scoped, debugger-visible, and guaranteed
// to be evaluated at compile time. Use Config::PALLET_L etc. throughout the codebase.
// Never use #define for these values.

namespace Config {

    // -------------------------------------------------------------------------
    // Pallet (container) dimensions — Euro pallet standard (mm)
    // -------------------------------------------------------------------------
    constexpr int PALLET_L = 1200;  // length (mm)
    constexpr int PALLET_W =  800;  // width  (mm)
    constexpr int PALLET_H = 1400;  // height (mm)

    // Maximum load mass per pallet (kg). Euro pallet static load limit.
    constexpr int PALLET_MAX_WEIGHT = 1000;

    // -------------------------------------------------------------------------
    // Genetic algorithm hyperparameters (Ananno & Ribeiro 2024, Section IV-B)
    // -------------------------------------------------------------------------
    constexpr int    GA_POPULATION     = 100;  // total individuals per generation
    constexpr int    GA_MU             =  15;  // number of parents selected each generation
    constexpr int    GA_LAMBDA         =  30;  // number of offspring produced each generation
    constexpr double GA_CROSSOVER_RATE = 0.5;  // probability of crossover between two parents
    constexpr double GA_MUTATION_RATE  = 0.2;  // probability of mutating an offspring
    constexpr int    GA_NGEN           =  30;  // maximum number of generations
    constexpr int    GA_MAX_STAGNATION =   5;  // stop early if best fitness unchanged this long

} // namespace Config

// Compile-time sanity checks on GA parameters.
// These fire at build time if the constants above are set to logically invalid values.
static_assert(Config::GA_MU >= 1,
    "GA_MU must be at least 1. The GA needs at least one parent.");
static_assert(Config::GA_LAMBDA >= 1,
    "GA_LAMBDA must be at least 1. The GA needs at least one offspring.");
static_assert(Config::GA_MU <= Config::GA_POPULATION,
    "GA_MU cannot exceed GA_POPULATION. Cannot select more parents than exist.");
static_assert(Config::GA_LAMBDA >= Config::GA_MU,
    "GA_LAMBDA must be >= GA_MU. Offspring count must meet or exceed parent count.");
```

**Key points:**
- `constexpr` vs `const` — `const` only prevents reassignment. `constexpr` guarantees the value is evaluated at compile time. For use in template arguments, array sizes, or other `constexpr` expressions, `constexpr` is strictly required.
- `namespace Config` — groups all constants under one name. Without a namespace, `PALLET_L` is a global symbol that could silently collide with a name in any library header you include.
- `static_assert` — evaluated at compile time because all operands are `constexpr`. A violated assertion halts the build and prints the string message. Zero runtime cost. The three required invariants are: `GA_MU >= 1`, `GA_LAMBDA >= GA_MU`, and `GA_MU <= GA_POPULATION`.
- Include order — `Types.h` includes `Config.h` (Types depends on constants). `Config.h` does not include `Types.h` (it has no dependency on any struct). The include arrow flows one direction only.

---

## Step 9: Register `test/test_types.cpp` in `CMakeLists.txt` and write the tests

CMake only compiles `.cpp` files that are explicitly listed in `add_executable()`. Add the new test file to the `GA_3DBPPTests` target:

```cmake
# CMakeLists.txt — updated GA_3DBPPTests target
add_executable(GA_3DBPPTests
    test/test_main.cpp
    test/test_types.cpp       # <-- added
)
```

Header-only files (`Types.h`, `Config.h`) never appear in `add_executable()` — they are pulled in automatically via `#include` when their dependent `.cpp` files are compiled.

Now write the test file. Each `TEST(Suite, Case)` is an independent test case. `EXPECT_EQ` compares integers. `EXPECT_DOUBLE_EQ` compares floating-point values with tolerance (use this instead of `EXPECT_EQ` for any `double`). `EXPECT_TRUE` checks a boolean condition.

```cpp
// test/test_types.cpp
#include <gtest/gtest.h>
#include "Types.h"
#include "Config.h"

// -------------------------------------------------------------------------
// ItemType
// -------------------------------------------------------------------------

TEST(ItemType, DefaultFields) {
    ItemType item;
    EXPECT_EQ(item.l, 0);
    EXPECT_EQ(item.w, 0);
    EXPECT_EQ(item.h, 0);
    EXPECT_EQ(item.m, 0);
    EXPECT_EQ(item.q, 0);
}

TEST(ItemType, VolumeAndBaseArea) {
    ItemType item;
    item.l = 100; item.w = 80; item.h = 50;
    EXPECT_EQ(item.volume(),   100 * 80 * 50);
    EXPECT_EQ(item.baseArea(), 100 * 80);
}

// -------------------------------------------------------------------------
// PlacedItem
// -------------------------------------------------------------------------

TEST(PlacedItem, DefaultFields) {
    PlacedItem p;
    EXPECT_EQ(p.item_type_index, 0);
    EXPECT_EQ(p.orientation, Orientation::Original);
    EXPECT_EQ(p.x, 0); EXPECT_EQ(p.y, 0); EXPECT_EQ(p.z, 0);
    EXPECT_EQ(p.dx, 0); EXPECT_EQ(p.dy, 0); EXPECT_EQ(p.dz, 0);
}

TEST(PlacedItem, ExtentAndCenter) {
    PlacedItem p;
    p.x = 10; p.y = 20; p.z = 30;
    p.dx = 100; p.dy = 80; p.dz = 50;
    EXPECT_EQ(p.extent(), (std::array<int,3>{110, 100, 80}));
    EXPECT_EQ(p.center(), (std::array<int,3>{ 60,  60, 55}));
}

// -------------------------------------------------------------------------
// Container
// -------------------------------------------------------------------------

TEST(Container, DefaultDimensions) {
    Container c;
    EXPECT_EQ(c.L, Config::PALLET_L);
    EXPECT_EQ(c.W, Config::PALLET_W);
    EXPECT_EQ(c.H, Config::PALLET_H);
    EXPECT_TRUE(c.items.empty());
}

TEST(Container, UtilizationEmpty) {
    Container c;
    EXPECT_DOUBLE_EQ(c.utilization(), 0.0);
}

TEST(Container, UtilizationAndTotalWeight) {
    // One item type: 600x400x700mm, mass 10
    std::vector<ItemType> types(1);
    types[0] = {600, 400, 700, 10, 1};

    // Place one box that fills exactly 1/8 of the container volume
    PlacedItem p;
    p.item_type_index = 0;
    p.dx = 600; p.dy = 400; p.dz = 700;

    Container c;
    c.items.push_back(p);

    // Placed volume = 600*400*700 = 168,000,000
    // Container volume = 1200*800*1400 = 1,344,000,000
    // Utilization = 168,000,000 / 1,344,000,000 = 0.125
    EXPECT_DOUBLE_EQ(c.utilization(), 0.125);
    EXPECT_EQ(c.totalWeight(types), 10);
}

// -------------------------------------------------------------------------
// PackingSolution
// -------------------------------------------------------------------------

TEST(PackingSolution, EmptyReturnsZero) {
    PackingSolution sol;
    EXPECT_TRUE(sol.containers.empty());
    EXPECT_DOUBLE_EQ(sol.avgUtilization(), 0.0);
}

// -------------------------------------------------------------------------
// ExtremePoint
// -------------------------------------------------------------------------

TEST(ExtremePoint, DefaultFields) {
    ExtremePoint ep;
    EXPECT_EQ(ep.x, 0);
    EXPECT_EQ(ep.y, 0);
    EXPECT_EQ(ep.z, 0);
}

// -------------------------------------------------------------------------
// Layer and Block
// -------------------------------------------------------------------------

TEST(Layer, DefaultFields) {
    Layer layer;
    EXPECT_EQ(layer.item_type_index, 0);
    EXPECT_EQ(layer.type, LayerType::Full);
    EXPECT_EQ(layer.item_count, 0);
    EXPECT_EQ(layer.height, 0);
}

TEST(Block, DefaultFields) {
    Block block;
    EXPECT_TRUE(block.layers.empty());
    EXPECT_EQ(block.z_base, 0);
}

// -------------------------------------------------------------------------
// Individual
// -------------------------------------------------------------------------

TEST(Individual, DefaultFields) {
    Individual ind;
    EXPECT_TRUE(ind.chromosome.empty());
    EXPECT_TRUE(ind.objectives.empty());
    EXPECT_EQ(ind.rank, 0);
    EXPECT_DOUBLE_EQ(ind.crowding_distance, 0.0);
}

// -------------------------------------------------------------------------
// Config constants
// -------------------------------------------------------------------------

TEST(Config, PalletDimensions) {
    EXPECT_EQ(Config::PALLET_L, 1200);
    EXPECT_EQ(Config::PALLET_W,  800);
    EXPECT_EQ(Config::PALLET_H, 1400);
}

TEST(Config, GAParameters) {
    EXPECT_EQ(Config::GA_POPULATION, 100);
    EXPECT_EQ(Config::GA_MU,          15);
    EXPECT_EQ(Config::GA_LAMBDA,       30);
    EXPECT_DOUBLE_EQ(Config::GA_CROSSOVER_RATE, 0.5);
    EXPECT_DOUBLE_EQ(Config::GA_MUTATION_RATE,  0.2);
    EXPECT_EQ(Config::GA_NGEN,          30);
    EXPECT_EQ(Config::GA_MAX_STAGNATION, 5);
}
```

**Key points:**
- One `TEST` per logical concern — `Container` has three separate cases (defaults, empty utilization, and the full calculation). Separate cases give precise failure messages.
- `EXPECT_DOUBLE_EQ` for all `double` results — floating-point arithmetic can produce results like `0.12500000000000003` instead of exactly `0.125`. `EXPECT_DOUBLE_EQ` accounts for this with a small tolerance. `EXPECT_EQ` on doubles would make tests brittle.
- `Config` tests pin the paper's values — if a constant is accidentally changed, the corresponding test fails immediately, making the regression visible before any algorithm code runs.

---

## Step 10: Build and run all tests

Reconfigure (to pick up the new source file in `CMakeLists.txt`), build, and run:

```bash
cmake --preset debug
cmake --build --preset debug
cd build/debug && ctest --output-on-failure
```

Expected output:

```
Test project /home/ubuntu/repos/GA_3DBPP/build/debug
      Start  1: SmokeTest.TrueIsTrue ..................   Passed
      Start  2: SmokeTest.OnePlusOneIsTwo .............   Passed
      Start  3: ItemType.DefaultFields ................   Passed
      Start  4: ItemType.VolumeAndBaseArea ............   Passed
      Start  5: PlacedItem.DefaultFields ..............   Passed
      Start  6: PlacedItem.ExtentAndCenter ............   Passed
      Start  7: Container.DefaultDimensions ...........   Passed
      Start  8: Container.UtilizationEmpty ............   Passed
      Start  9: Container.UtilizationAndTotalWeight ...   Passed
      Start 10: PackingSolution.EmptyReturnsZero ......   Passed
      Start 11: ExtremePoint.DefaultFields ............   Passed
      Start 12: Layer.DefaultFields ...................   Passed
      Start 13: Block.DefaultFields ...................   Passed
      Start 14: Individual.DefaultFields ..............   Passed
      Start 15: Config.PalletDimensions ...............   Passed
      Start 16: Config.GAParameters ...................   Passed

100% tests passed, 0 tests failed out of 16
```

All 16 tests pass under AddressSanitizer and UndefinedBehaviorSanitizer (enabled automatically in the debug preset). Zero compiler warnings.

---

## Final file structure added in Phase 1

```
include/
├── Types.h        # All structs and enums (ItemType, PlacedItem, Container,
│                  # PackingSolution, ExtremePoint, Individual, Layer, Block,
│                  # Orientation enum, LayerType enum)
└── Config.h       # constexpr algorithm parameters + static_assert guards

test/
├── test_main.cpp  # Original smoke tests (unchanged)
└── test_types.cpp # Phase 1 unit tests (16 cases)
```

---

## What changes in Phase 2

Phase 2 adds the file I/O layer: a CSV reader that parses order files into `std::vector<ItemType>` and a benchmark reader that loads the BR1–7 thpack files. It also adds the geometric constraint checkers (weight limit, overhang, stability) that operate on `Container` and `PlacedItem`. All of that code operates on the types defined here — Phase 1's structs are the stable foundation every subsequent phase builds on.
