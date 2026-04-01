# GA_3DBPP — Multi-Container 3D Bin Packing (Ananno & Ribeiro 2024)

## Project Overview

C++ implementation of a two-phase multi-container 3D bin packing algorithm from an IEEE Access paper. Phase 1 uses constructive heuristics (layer/block building) for homogeneous items. Phase 2 uses a genetic algorithm with Extreme Points placement for residual items. Targets Euro pallets (1200×800×1400mm) with 8 real-world constraints, though pallet sizes are flexible per GA_3DBPP/docs/misc/standard-pallet-sizes.md. DIRECTLY implement from the paper, which can be found at GA_3DBPP/docs/misc/A_Multi-Heuristic_Algorithm_for_Multi-Container_3-D_Bin_Packing_Problem_Optimization_Using_Real_World_Constraints.pdf.

## Build & Test

- **Toolchain**: GCC 13.3.0, C++20, CMake 3.28.3 + Ninja, vcpkg (x64-linux)
- **Debug build** (with ASan/UBSan): `cmake --preset debug && cmake --build --preset debug`
- **Release build** (optimized): `cmake --preset release && cmake --build --preset release`
- **Run tests**: `cd build/debug && ctest --output-on-failure`
- **vcpkg toolchain**: `~/vcpkg/scripts/buildsystems/vcpkg.cmake`
- Always build and run tests in Debug mode during development to catch memory errors early.

## Key Directories

- `src/` — application source files (.cpp)
- `include/` — header files (.h)
- `test/` — Google Test files
- `data/br_benchmark/` — BR1-7 thpack files (read-only test data)
- `docs/` — project documentation and phase guides
- `build/debug/`, `build/release/` — CMake build outputs (not committed)

## C++ Conventions (Strict)

- C++20 standard. No compiler extensions.
- **Rule of Zero**: no custom destructors, copy/move constructors, or assignment operators. All structs use only value types and standard containers.
- **AoS layout**: `std::vector<Struct>`, not parallel arrays of fields.
- **Default member initializers** on every numeric field (int = 0, double = 0.0, bool = false).
- **`enum class`** for all categorical values (Orientation, LayerType), never raw ints.
- **`constexpr`** for compile-time constants, never `#define`.
- **Index-based references** between structs (int index into a vector), never raw pointers.
- All dimensions and positions are `int` (millimeters). Ratios and metrics are `double`.
- Header guards use `#pragma once`.
- Warnings: `-Wall -Wextra -Wpedantic` must compile clean. Zero warnings policy.

## Teaching Mode (CRITICAL — ALWAYS FOLLOW)

The user is a C++ beginner. The primary goal is deep understanding, not just working code.

- **Before writing or modifying any code**, explain WHAT you are about to do and WHY in 2-4 sentences. Name the C++ concepts involved.
- **After writing code**, walk through it line by line. Explain any syntax, keyword, or pattern the user might not know.
- **Quiz the user** after each struct, file, or concept with 2-3 short questions. Wait for answers before proceeding. Do not give the answers — let the user try.
- **If the user's answer is wrong**, explain the correct answer and why the mistake is a common one. Do not just say "incorrect."
- **Never batch multiple files** into one response without discussion between them. One struct or one concept at a time.
- **Ask "Ready to continue?"** before moving to the next task.
- **When modifying CMakeLists.txt** or build files, explain what changed and why.
- **If the user asks to skip teaching**, respect that for the current task only. Resume teaching on the next task.
- Use precise technical vocabulary but always define terms on first use.

## Full Project Roadmap
 
Work through phases sequentially. Within each phase, tasks are ordered by dependency.
Each task: explain → write code → walk through → quiz → confirm understanding → next task.
 
---
 
### Phase 0: Project Scaffolding & Toolchain Setup ✅ COMPLETE
 
| Task | Description | Status |
|------|-------------|--------|
| 0.1 | Install GCC 13.3.0 toolchain on Ubuntu 24.04 VM | ✅ Done |
| 0.2 | Install CMake 3.28.3 + Ninja 1.11.1 | ✅ Done |
| 0.3 | Bootstrap vcpkg at `~/vcpkg`, set x64-linux triplet | ✅ Done |
| 0.4 | Install nlohmann-json 3.12.0 via vcpkg | ✅ Done |
| 0.5 | Install Google Test 1.17.0 via vcpkg | ✅ Done |
| 0.6 | Install spdlog 1.17.0 via vcpkg | ✅ Done |
| 0.7 | Create `CMakeLists.txt` — C++20, `-Wall -Wextra -Wpedantic`, GTest integration, nlohmann-json, spdlog linking | ✅ Done |
| 0.8 | Create directory structure: `src/`, `include/`, `test/`, `data/`, `docs/`, `output/`, `visualization/`, `.vscode/` | ✅ Done |
| 0.9 | Create `sample_order.csv` test data file from paper's dataset format | ✅ Done |
| 0.10 | Download BR1–7 benchmark datasets into `data/br_benchmark/thpack1-7.txt` from OR-Library (Bischoff & Ratcliff 1995) | ✅ Done |
| 0.11 | Create `CMakePresets.json` with Debug (ASan/UBSan, `-g -O0`) and Release (`-O3 -DNDEBUG`) presets; verify both build and pass 2 smoke tests | ✅ Done |
 
---
 
### Phase 1: Core Data Structures ✅ COMPLETE
 
**Goal**: Define the C++ vocabulary for the entire program. Every later phase depends on these types.
**Files created**: `include/Types.h`, `include/Config.h`, `test/test_types.cpp`
**Design principles**: Rule of Zero, AoS layout, default member initializers, `enum class`, `constexpr`, index-based cross-references.
 
| Task | Description | Key Fields / Methods | Paper Reference | Status |
|------|-------------|---------------------|-----------------|--------|
| 1.1 | Implement `ItemType` struct in `include/Types.h` | `id`, `length`, `width`, `height`, `weight`, `quantity`; helpers: `volume()`, `baseArea()`, rotated dimensions | Item `[l,w,h,m,v,q]` — Section IV-A | ✅ Done |
| 1.2 | Implement `PlacedItem` struct | `item_type_index`, `x`, `y`, `z`, `orientation` (enum class), `container_index`; helpers: extent coords, center point | Position `[x_i,y_i,z_i]`, Z-rotation — Figure 4 | ✅ Done |
| 1.3 | Implement `Container` struct | `id`, `length`, `width`, `height`, `max_weight`, `vector<PlacedItem>`, `vector<ExtremePoint>`; helpers: `volumeUtilization()`, `totalWeight()`, `remainingHeight()` | Pallet `[L,W,H]` — Section IV-A | ✅ Done |
| 1.4 | Implement `PackingSolution` struct | `vector<Container>`, fitness values (container count, neg volume util, wasted space); helpers: `avgUtilization()`, `totalContainers()`, `allItemsPacked()` | Multi-objective targets — Section IV-B | ✅ Done |
| 1.5 | Implement `Individual` struct (GA chromosome) | `chromosome` (vector<int> — permutation of item type indices), `objectives` (vector<double>), `rank`, `crowding_distance` | GA encoding — Figure 12, NSGA-II — Section IV-C | ✅ Done |
| 1.6 | Implement `ExtremePoint` struct | `x`, `y`, `z`, `is_used`; comparison by z (height), then distance to origin | EP strategy — Section IV-B-3, Figure 11 | ✅ Done |
| 1.7 | Implement `Layer`, `Block` structs + `enum class LayerType` | `LayerType{Full, Half, Quarter}`; `Layer`: item_type_index, type, positions, height, fill_rate; `Block`: vector<Layer>, container_index, base_pos, total_height | Layer/block building — Section IV-B-2, Figures 6-9 | ✅ Done |
| 1.8 | Implement `include/Config.h` | `constexpr` pallet dims, GA params (pop, mu, lambda, Pc, Pm, ngen, stagnation), constraint thresholds (min support %, COM deviation, min fill rate) | Section IV-C parametrization | ✅ Done |
| 1.9 | Write `test/test_types.cpp` + update CMakeLists.txt | Default-construct all structs, verify defaults; construct with values, verify access; test `volume()`, `baseArea()`, extent in both orientations | — | ✅ Done |
 
---
 
### Phase 2: File I/O
 
**Goal**: Read problem instances (CSV + BR format), write solutions (JSON for visualization), add logging.
**Files created**: `src/CSVReader.h/.cpp`, `src/BRParser.h/.cpp`, `src/JSONWriter.h/.cpp`, `test/test_io.cpp`
**Key libraries**: nlohmann/json (serialization), spdlog (logging)
 
| Task | Description | Status |
|------|-------------|--------|
| 2.1 | Implement CSV reader — parse industrial dataset: order number, product id, quantity, dimensions (l,w,h), weight → vector of `ItemType` | ✅ Done  |
| 2.2 | Implement BR dataset parser — parse thpack1-7.txt: instance count, container dims, box type count, box dims + orientation flags + quantity → per-instance vector of `ItemType` + `Container` dims | ✅ Done |
| 2.3 | Implement JSON writer — serialize `PackingSolution` (containers, placed items with positions/orientations/dimensions, metadata) for Three.js | ✅ Done  |
| 2.4 | Implement spdlog-based logging utility — debug/info/warn/error levels, configurable verbosity | ✅ Done |
| 2.5 | Write `test/test_io.cpp` — round-trip: parse sample_order.csv → verify ItemType fields; parse thpack1.txt instance 1 → verify 3 box types with correct dims/quantities; JSON write → read back → verify | ✅ Done |
 
---
 
### Phase 3: Geometric Constraint Modules
 
**Goal**: Implement the math behind constraints 2 (AABB), 3 (COM stability), 4 (support), and 8 (Hausdorff interlocking).
**Files created**: `src/AABB.h`, `src/SupportChecker.h/.cpp`, `src/CenterOfMass.h/.cpp`, `src/Hausdorff.h/.cpp`, `test/test_geometry.cpp`
 
| Task | Description | Status |
|------|-------------|--------|
| 3.1 | AABB collision detection — check two PlacedItems for volume overlap; check item vs. container bounds (0 ≤ x ≤ L, etc.) | ✅ Done |
| 3.2 | Support checking — tiered: (a) 40% base area + 4 base vertices, (b) 50% base area + 3 vertices, (c) 75% base area + 2 vertices | ✅ Done |
| 3.3 | Base vertex inward translation — configurable XY offset pushing test vertices toward pallet center (paper's practical adjustment) | ✅ Done |
| 3.4 | Center of mass calculation — weighted average of all placed item centers in a container | ✅ Done |
| 3.5 | COM stability check — tipping test (COM within base polygon) + XY deviation from pallet geometric center (configurable max deviation) | ✅ Done |
| 3.6 | Hausdorff distance — between top-layer vertices and bottom-layer vertices; higher = better interlocking | ✅ Done |
| 3.7 | Write `test/test_geometry.cpp` — AABB (8+ cases), support (6+ cases), COM (4+ cases), Hausdorff (known point sets) | ✅ Done |
 
---
 
### Phase 4: Packing Engine — Phase 1 (Layer & Block Building)
 
**Goal**: Implement the paper's constructive heuristics that pack the bulk of homogeneous items into layers and blocks.
**Files created**: `src/LayerGenerator.h/.cpp`, `src/BlockBuilder.h/.cpp`, `test/test_layers.cpp`
**Paper ref**: Section IV-B-2, Figures 6-10
 
| Task | Description | Status |
|------|-------------|--------|
| 4.1 | Single-item-type layer generation — test both orientations, pick pattern with more items; on tie, prefer default orientation | ✅ Done |
| 4.2 | Dynamic shifting — distribute unused area along center lines, push items to pallet extremities (improves stability + interlocking) | ✅ Done |
| 4.3 | Full/half/quarter layer classification by pallet area coverage | ✅ Done |
| 4.4 | Layer merging — combine quarter layers of same height → half layers; half layers of same height → full layers (up to 4 item types per merged layer) | ✅ Done |
| 4.5 | Minimum fill rate filter — reject layers below configurable threshold | ✅ Done |
| 4.6 | Block building — stack layers sorted by (occupied area, weight, item type); check max height; create new pallet on overflow; sort blocks by remaining height ascending before placing next layer | ✅ Done |
| 4.7 | Half-layer placement — first half-layer on top of last full layer's first half; subsequent in lowest half of block | ✅ Done |
| 4.8 | Quarter-layer placement — first in lowest quadrant (or first quadrant if starting new pallet); subsequent balance quadrant heights | ✅ Done |
| 4.9 | Hausdorff interlocking — for each stacked pair, test 4 symmetry variants (original, H-flip, V-flip, HV-flip), select variant with highest Hausdorff distance | ✅ Done |
| 4.10 | Identify residual items — items not assigned to any layer; pre-check if remaining volume on existing blocks fits residual volume; spawn new pallet if not | ✅ Done |
| 4.11 | Write `test/test_layers.cpp` — layer item counts, fill rates, block heights, merged layer composition, residual identification | ✅ Done |
 
Objectives for Phase 4: 
Implement full layer generation (try both orientations, pick best)
Implement half layer generation (4 candidates per item type)
Implement quarter layer generation (2 candidates per type)
Implement fill rate thresholds (Full=90%, Half=90%, Quarter=85%)
Implement dynamic shifting — distribute gaps to pallet edges
Implement block building — stack homogeneous layers vertically
Implement interlocking layer alternation using Hausdorff metric
Implement block catalog precomputation for all item types
Implement half/quarter layer merging into full layers (height match)
Implement block-to-pallet assignment (sort area→weight→type)
Write test_layers.cpp (5+ cases: perfect div, oversize, single item)

---
 
### Phase 5: Packing Engine — Phase 2 (Extreme Points)
 
**Goal**: Implement the EP-based placement strategy that the GA uses to evaluate chromosomes.
**Files created**: `src/ExtremePointEngine.h/.cpp`, `test/test_extreme_points.cpp`
**Paper ref**: Section IV-B-3, Figure 11
 
| Task | Description | Status |
|------|-------------|--------|
| 5.1 | EP initialization — `[0,0,0]` for empty pallet; block top-surface vertices for pallets with Phase 1 blocks | ✅ Done |
| 5.2 | EP generation — 3 new EPs per placed item: `[x+l, y, z]`, `[x, y+w, z]`, `[x, y, z+h]` | ✅ Done |
| 5.3 | EP projection — snap floating EPs down to nearest supporting surface or pallet floor | ✅ Done |
| 5.4 | EP maintenance — remove used EPs, remove interior/dominated EPs, deduplicate | ✅ Done |
| 5.5 | EP priority sorting — lowest z first; on tie, closest to origin `[0,0,0]` | ✅ Done |
| 5.6 | Placement procedure — iterate sorted EPs: try item in primary orientation → check all hard constraints → if fail, try rotated → if fail, next EP → if all EPs exhausted, penalize individual | ✅ Done |
| 5.7 | Phase 1→2 handoff — block top-surface vertices become initial EP set for Phase 2 | ✅ Done |
| 5.8 | Write `test/test_extreme_points.cpp` — EP generation (5+ cases), placement with constraints, EP exhaustion/penalty | ✅ Done |
 
Objectives for phase 5:
Implement EP initialization — origin (0,0,0) for empty container
Implement EP generation — 3 new EPs per placed item placement
Implement EP projection — snap EPs to nearest geometry/wall
Implement EP maintenance — remove interior + dominated EPs
Implement EP priority sorting (lowest Y → smallest Z → smallest X)
Implement placement procedure — first-fit over sorted EP list
Integrate Phase 1 block top-surfaces as initial Phase 2 EPs
Write test_extreme_points.cpp (5+ cases)
---
 
### Phase 6: NSGA-II Genetic Algorithm
 
**Goal**: Implement the multi-objective evolutionary optimizer that finds good residual packing sequences.
**Files created**: `src/NSGA2.h/.cpp`, `src/GeneticOperators.h/.cpp`, `test/test_nsga2.cpp`
**Paper ref**: Section IV-C, Figure 12
 
| Task | Description | Status |
|------|-------------|--------|
| 6.1 | Chromosome encoding — permutation of item type indices (not individual items); reduces search space from q! to m! | ⬜ |
| 6.2 | 10 seeded individuals — 5 sorting criteria (weight, quantity, base area, volume, volume×qty) × asc/desc (Table 5) | ⬜ |
| 6.3 | Random population fill — `std::shuffle` + `std::mt19937` to fill remaining slots up to population_size | ⬜ |
| 6.4 | Single-point crossover with repair — exchange segments, detect duplicates + missing genes, replace duplicates with missing | ⬜ |
| 6.5 | Swap mutation — randomly select two positions in chromosome, swap genes | ⬜ |
| 6.6 | Binary tournament selection — compare by rank first; on tie, prefer higher crowding distance | ⬜ |
| 6.7 | Fast non-dominated sorting — assign rank (Pareto front = rank 0, next front = rank 1, …) | ⬜ |
| 6.8 | Crowding distance — per-objective normalized spread; infinity for boundary solutions | ⬜ |
| 6.9 | Mu+Lambda selection — combine parent pop (mu) + offspring (lambda), non-dominated sort, select top mu individuals | ⬜ |
| 6.10 | 3-objective fitness evaluation — decode chromosome via EP placement engine → compute (container count, −volume utilization, wasted space) | ⬜ |
| 6.11 | Pareto front extraction — collect all rank-0 individuals from final generation | ⬜ |
| 6.12 | Stagnation detection — track best fitness over generations; stop if no improvement for max_stagnation consecutive generations | ⬜ |
| 6.13 | GA main loop — init pop → for each gen: select parents → crossover/mutate → evaluate → combine → non-dominated sort → select next gen → check stopping | ⬜ |
| 6.14 | Write `test/test_nsga2.cpp` — sorting correctness, crossover repair validity, tournament selection, convergence on trivial 3-type instance | ⬜ |

Objectives for Phase 6:
Implement chromosome — permutation of item type indices
Implement 10 seeded individuals (5 criteria × asc/desc order)
Implement random population fill (std::shuffle + mt19937)
Implement single-point crossover with duplicate repair
Implement swap mutation for permutation chromosomes
Implement binary tournament selection (rank → crowding)
Implement fast non-dominated sorting O(MN²)
Implement crowding distance calculation across 3 objectives
Implement Mu+Lambda selection (parents+offspring → next gen)
Implement 3-objective fitness evaluation via Packer decoder
Implement Pareto front extraction from final population
Implement early stopping — stagnation detection over N gens
Implement GA main loop (init → evolve → select → stop)
Write test_nsga2.cpp (sorting, crossover repair, tournament, etc.)
 
---
 
### Phase 7: Main Entry Point & Integration
 
**Goal**: Wire everything together into a single executable pipeline.
**Files created**: `src/Packer.h/.cpp`, update `src/main.cpp`

When we get to Phase 7, main.cpp will call LayerGenerator::generateAll(itemTypes) first, check if the returned layer list is empty, and branch accordingly. No new task is needed — it's a conditional if statement inside Task 7.2. But it's worth being aware of now because it affects how you think about the data flow: the GA's chromosome only permutes residual item types, not the full order. If no item type in the entire order can form even a single layer, Phase 1 is skipped entirely and every item goes to Phase 2 as a residual. The paper states this directly: a fully heterogeneous order where each item is unique invalidates Phase 1, and the algorithm proceeds with only Phase 2.
 
| Task | Description | Status |
|------|-------------|--------|
| 7.1 | Implement `Packer::decode()` — chromosome + item types + containers → run EP placement → return `PackingSolution` | ⬜ |
| 7.2 | Implement `main.cpp` — CLI arg parsing (input file, config overrides) → read input → Phase 1 → Phase 2 (GA) → select solution → write JSON | ⬜ |
| 7.3 | Solution selection from Pareto front — prioritize compactness over heterogeneity (per paper) | ⬜ |
| 7.4 | Progress reporting — log generation #, best fitness values, elapsed time via spdlog | ⬜ |
| 7.5 | End-to-end test — sample_order.csv → full algorithm → JSON output, verify all 8 constraints satisfied | ⬜ |
| 7.6 | Profile with `perf`/`gprof`, fix crashes/edge cases, ensure stability on all test inputs | ⬜ |
 
Objectives for Phase 7: 
Implement Packer::decode() — chromosome → PackingSolution
Implement main.cpp — CLI parsing + CSV→GA→JSON pipeline
Implement solution selection heuristic from Pareto front
Add progress reporting (gen #, best fitness, elapsed time)
End-to-end test: sample CSV → NSGA-II → JSON output
Profile with gprof (-pg flag), fix crashes / edge cases

---
 
### Phase 8: Comprehensive Testing & Benchmarks
 
**Goal**: Validate correctness and compare performance against published results.
**Files created**: `src/BenchmarkRunner.h/.cpp`, `test/test_integration.cpp`
 
| Task | Description | Status |
|------|-------------|--------|
| 8.1 | Write `test/test_integration.cpp` — full pipeline constraint audit (every placed item checked for AABB, support, COM, bounds) | ⬜ |
| 8.2 | Run algorithm on BR1–BR7 (700 instances total), collect per-instance volume utilization + execution time | ⬜ |
| 8.3 | Compare results vs. literature Table 7 — strict support: ~50% util (all items packed → 2 containers), relaxed: 62-95% | ⬜ |
| 8.4 | Edge case testing — single-type orders, fully heterogeneous orders, overweight single items, 1-item orders, empty orders | ⬜ |
| 8.5 | Constraint violation audit — verify zero violations across all BR instances and sample orders | ⬜ |
| 8.6 | Create `BenchmarkRunner` — automated batch runner, produces CSV summary (instance, util%, time_ms, containers, violations) | ⬜ |

Objectives for Phase 8:
Write test_integration.cpp — full pipeline constraint audit
Implement BR dataset parser (thpack1-7 text format)
Run algorithm on BR1-BR7 (700 instances), collect metrics
Compare volume utilization vs literature (target 85-92%)
Test edge cases: single-type, all-unique, overweight, tiny orders
Constraint violation audit on all outputs (AABB, support, COM)
Create BenchmarkRunner — automated test + CSV report
 
---
 
### Phase 9: Three.js Visualization
 
**Goal**: Interactive 3D web viewer for inspecting packing solutions.
**Files created**: `visualization/index.html`, `visualization/js/` modules
**Key library**: Three.js r170 via ES6 import map
 
| Task | Description | Status |
|------|-------------|--------|
| 9.1 | Create `index.html` — dark sidebar (280px) + Three.js canvas, import map for r170 modules | ⬜ |
| 9.2 | Scene setup — PerspectiveCamera, ambient + directional lights, grid helper, OrbitControls | ⬜ |
| 9.3 | Euro pallet rendering — wooden base box + translucent max-height wireframe zone | ⬜ |
| 9.4 | Item rendering — color-coded by item type, black edge lines via `EdgesGeometry`, opacity support | ⬜ |
| 9.5 | JSON loader — `<input type="file">` + drag-and-drop on canvas, parse and populate scene | ⬜ |
| 9.6 | Step-by-step animation — play/pause/reset buttons, configurable speed, items appear one at a time in placement order | ⬜ |
| 9.7 | Step slider — scrub to any placement step, bidirectional | ⬜ |
| 9.8 | Layer visibility toggle — slider to show/hide items by Z-layer | ⬜ |
| 9.9 | Multi-pallet view — side-by-side for ≤4 pallets, dropdown selector for >4 | ⬜ |
| 9.10 | Item tooltip on hover — raycaster → display type, dims, weight, position in overlay | ⬜ |
| 9.11 | Opacity slider — X-ray mode for inspecting internal packing | ⬜ |
| 9.12 | Statistics panel — utilization %, item count, total weight, COM position, compactness score | ⬜ |

### Task Order

Work through tasks sequentially and directly reference the paper found in GA_3DBPP/docs/misc/A_Multi-Heuristic_Algorithm_for_Multi-Container_3-D_Bin_Packing_Problem_Optimization_Using_Real_World_Constraints.pdf to ensure the approach in C++ matches the algorithm and mathematical precision used in the paper. Each task: explain → write code → walk through → quiz → confirm understanding → next task. For each task, you are to take a skeptical approach and rigorously evaluate the approach which best guarantees mathematical accuracy and low latency/overhead.

## Paper Reference

- Ananno & Ribeiro (2024), "A Multi-Heuristic Algorithm for Multi-Container 3-D Bin Packing Problem Optimization Using Real World Constraints", IEEE Access Vol. 12
- Item: `[l, w, h, m, v, q]` — length, width, height, mass, volume, quantity
- Pallet: `[L, W, H]` = `[1200, 800, 1400]` mm (Euro pallet)
- Coordinate system: origin at left-bottom corner, X along length, Y along width, Z up
- Rotation: Z-axis 90° only (2 orientations per item)
- GA parameters (paper): population=100, mu=15, lambda=30, crossover=0.5, mutation=0.2, ngen=30, max_stagnation=5