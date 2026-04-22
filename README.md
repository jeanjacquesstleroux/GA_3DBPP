# GA_3DBPP — 3D Bin Packing with NSGA-II

An implementation of a two-phase 3D bin packing algorithm combining a constructive layer/block heuristic (Phase 1) with a multi-objective genetic algorithm (NSGA-II, Phase 2). Results are visualised in an interactive Three.js web app served by a local Flask server.

Based on: *Ananno & Ribeiro (2024), IEEE Access Vol. 12.*

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [File Tree](#file-tree)
3. [Building the C++ Binary](#building-the-c-binary)
4. [Running the App](#running-the-app)
5. [Using the UI](#using-the-ui)
6. [Algorithm Overview](#algorithm-overview)
7. [Data Formats](#data-formats)
8. [File Reference](#file-reference)
   - [C++ Source & Headers](#c-source--headers)
   - [Server](#server)
   - [Frontend](#frontend)
9. [Output Format](#output-format)
10. [Dependencies](#dependencies)
11. [Configuration](#configuration)

---

## Quick Start

```bash
# 1. Build the release binary (first time only, or after source changes)
cmake --preset release
cmake --build --preset release

# 2. Start the web server
cd server
python3 server.py

# 3. Open in browser
# http://localhost:5000
```

---

## File Tree

```
GA_3DBPP/
├── CMakeLists.txt                  Build configuration
├── CMakePresets.json               Debug / release CMake presets
│
├── src/                            C++ implementation
│   ├── main.cpp                    Entry point — two-phase algorithm orchestrator
│   ├── BenchmarkMain.cpp           BR benchmark batch runner
│   ├── LayerGenerator.cpp          Phase 1 layer construction
│   ├── BlockBuilder.cpp            Phase 1 block stacking & Hausdorff interlocking
│   ├── NSGA2.cpp                   Phase 2 genetic algorithm (NSGA-II)
│   ├── GeneticOperators.cpp        Crossover, mutation, tournament selection
│   ├── Packer.cpp                  GA chromosome decoder → packed containers
│   ├── ExtremePointEngine.cpp      3D extreme-point placement engine
│   ├── SupportChecker.cpp          Three-tier item support validation
│   ├── CenterOfMass.cpp            Centre-of-mass stability check
│   ├── Hausdorff.cpp               Hausdorff distance for layer interlocking
│   ├── CSVReader.cpp               CSV dataset parser
│   ├── BRReader.cpp                BR benchmark file parser
│   ├── JSONWriter.cpp              JSON output writer (standard + animated)
│   ├── Logger.cpp                  spdlog wrapper (console + file)
│   ├── PalletRegistry.cpp          75+ pallet dimension registry
│   └── BenchmarkRunner.cpp         BR benchmark test framework
│
├── include/                        C++ headers
│   ├── Types.h                     Core data structures
│   ├── Config.h                    Compile-time parameters (constexpr)
│   ├── AnimatedSolution.h          Animated output data structures
│   ├── AABB.h                      Axis-aligned bounding box utilities
│   ├── LayerGenerator.h
│   ├── BlockBuilder.h
│   ├── NSGA2.h
│   ├── GeneticOperators.h
│   ├── Packer.h
│   ├── ExtremePointEngine.h
│   ├── SupportChecker.h
│   ├── CenterOfMass.h
│   ├── Hausdorff.h
│   ├── CSVReader.h
│   ├── BRReader.h
│   ├── JSONWriter.h
│   ├── Logger.h
│   ├── PalletRegistry.h
│   └── BenchmarkRunner.h
│
├── server/
│   ├── server.py                   Flask web server
│   └── requirements.txt            Python dependencies (Flask ≥3.0.0)
│
├── visualization/                  Web app
│   ├── index.html                  Single-page app layout
│   ├── css/
│   │   └── styles.css              Dark-theme stylesheet
│   └── js/
│       ├── app.js                  Top-level orchestrator
│       ├── worker.js               Web Worker — frame pre-computation
│       ├── phase1Animator.js       Animation state machine
│       ├── controls.js             Transport & navigation controls
│       ├── renderer.js             Three.js 3D scene
│       ├── layerPanel.js           Layer manifest side panel
│       └── lib/                    Bundled Three.js r170 + OrbitControls
│
├── test/
│   └── *.cpp                       C++ unit tests (Google Test)
│
├── data/
│   ├── Dataset1000.csv             1 000-item author dataset
│   ├── Dataset10000.csv            10 000-item author dataset
│   ├── br_benchmark/
│   │   └── thpack1–7.txt           BR benchmark instances
│   └── test_fixtures/              Small CSV files used by unit tests
│
├── output/
│   ├── solution.json               Last standard solver output
│   └── solution_animated.json      Last animated solver output (used by web app)
│
├── docs/                           Architecture docs, phase notes, research paper
└── build/                          CMake build artifacts (generated)
```

---

## Building the C++ Binary

### Prerequisites

- CMake ≥ 3.20
- Ninja build system
- C++20-capable compiler (GCC 12+ or Clang 15+ recommended)
- [vcpkg](https://github.com/microsoft/vcpkg) with `nlohmann-json` and `spdlog` installed

### Commands

```bash
# Release build (used by the web server)
cmake --preset release
cmake --build --preset release
# Binary: build/release/GA_3DBPP

# Debug build (AddressSanitizer + UBSan enabled)
cmake --preset debug
cmake --build --preset debug
# Binary: build/debug/GA_3DBPP

# Run unit tests
cmake --build --preset debug --target GA_3DBPPTests
./build/debug/GA_3DBPPTests
```

### Three build targets

| Target | Binary | Purpose |
|--------|--------|---------|
| `GA_3DBPP` | `build/<preset>/GA_3DBPP` | Main solver — used by the web server |
| `GA_3DBPPBenchmark` | `build/<preset>/GA_3DBPPBenchmark` | Batch BR benchmark runner |
| `GA_3DBPPTests` | `build/<preset>/GA_3DBPPTests` | Google Test unit tests |

---

## Running the App

### 1. Start the server

```bash
cd server
python3 server.py
# Listening on http://localhost:5000
```

### 2. Open the browser

Navigate to `http://localhost:5000`.

### Running the solver directly (without the web app)

```bash
# Standard output (solution.json)
./build/release/GA_3DBPP data/Dataset1000.csv output/solution.json

# Animated output (solution_animated.json — required for the web app)
./build/release/GA_3DBPP data/Dataset1000.csv output/solution_animated.json --animated-output

# BR benchmark problem
./build/release/GA_3DBPP data/br_benchmark/thpack1.txt output/solution.json
```

---

## Using the UI

### Overview

The interface has three areas: a left sidebar (controls + stats), a centre 3D viewport, and an overlay nav bar at the bottom of the viewport.

### Step-by-step

1. **Select a dataset** from the dropdown at the top of the sidebar. Datasets are grouped:
   - *Author* — the included CSV files (`Dataset1000.csv`, `Dataset10000.csv`)
   - *BR Benchmark* — the `thpack1–7.txt` benchmark instances

2. **Click ▶ Run**. The server invokes the C++ binary, which runs both algorithm phases. A spinner appears while the algorithm executes (typically a few seconds).

3. **Watch the animation**. Items appear on the 3D pallet one-by-one (or one layer at a time for large datasets). Each pallet is animated in sequence with a brief pause between containers.

4. **Interact with the 3D view** at any time:
   - **Orbit** — click and drag
   - **Zoom** — scroll wheel
   - **Pan** — right-click and drag (or two-finger drag)

### Transport controls (sidebar)

| Button | Action |
|--------|--------|
| ⏮ | Jump to the start of the previous container |
| ⏸ / ▶ | Pause or resume the animation |
| ⏭ | Jump to the start of the next container |
| ↺ | Restart the entire animation from the beginning |

**Speed slider** — sets the playback rate: `0.25×`, `0.5×`, `1.0×`, `2.0×`, or `5.0×`. The selected speed takes effect immediately and is also applied when you click Run.

### Container dots

A row of small dots appears below the speed slider, one per pallet. Their state:
- **Grey** — not yet reached in the animation
- **Yellow** — currently animating
- **Red** — fully packed and complete

**Click any dot** to jump directly to that container's fully-packed view and pause. Use this to inspect any container without waiting for the animation.

### Container navigation bar

A `← Container X / N →` bar appears at the bottom of the viewport whenever the solution uses more than one container.

- **← / →** step through containers one at a time, showing the final fully-packed state of each.
- The prev/next buttons are disabled at the boundaries (first and last container).
- Clicking a container dot or using the nav bar both pause the animation and show the complete packed state of the selected container. Press ▶ to resume.

### Layer breakdown panel

Below the transport controls, the *Layer Breakdown* panel lists every layer in the current container (Phase 1 items only). Each entry shows:
- Layer index
- Z range in mm (bottom to top)
- Item count
- Expandable item-type breakdown

Hovering over a layer row highlights those items in the 3D view.

### Stats panel

At the bottom of the sidebar:

| Stat | Meaning |
|------|---------|
| Dataset | Filename of the loaded dataset |
| Containers | Total pallets used |
| Items | Total items packed across both algorithm phases |
| Avg Util | Average volumetric utilisation across all containers |
| GA Gens | Number of GA generations recorded |
| Pallet Util | Utilisation of the currently displayed container |

---

## Algorithm Overview

The solver runs in two sequential phases.

### Phase 1 — Layer & Block Heuristic

Constructs homogeneous layers for each item type and stacks them into blocks.

1. **Layer generation** — for each item type, candidate layers are built across three footprint sizes (Full `L×W`, Half `L/2×W` or `L×W/2`, Quarter `L/2×W/2`). Both Z-axis rotations are tried; the orientation maximising item count is kept. Leftover space is pushed to pallet edges (*dynamic shifting*).
2. **Fill-rate filtering** — Full/Half layers require ≥ 90% fill; Quarter layers ≥ 85%.
3. **Layer merging** — same-height Quarter layers merge into Halves; Halves merge into Fulls, reducing container count.
4. **Block building** — merged layers are stacked onto pallets, with Hausdorff-distance interlocking applied between adjacent layers to maximise stability. Half-layers occupy designated half-zones; Quarter-layers occupy the lowest quadrants.
5. **Residual computation** — items not placed in any layer become input to Phase 2.

### Phase 2 — NSGA-II Genetic Algorithm

Optimises placement of residual items using multi-objective evolution.

**Objectives (all minimised):**
1. Container count
2. Negative average utilisation (i.e. maximise utilisation)
3. Total wasted volume (mm³)

**Process:**
1. Population initialised with 10 seeded individuals (sorted by weight, quantity, area, volume) plus random shuffles.
2. Each individual is a permutation of residual item types. Fitness is evaluated by decoding the permutation through the **Extreme Point Engine**, which places items at geometrically-derived candidate points (right face, back face, top face of each placed item).
3. NSGA-II selection: non-dominated sort assigns rank; crowding distance preserves diversity within each rank.
4. Runs for up to 30 generations; terminates early after 5 consecutive generations with no improvement in best container count.
5. The Pareto front is extracted and the best solution is chosen: fewest containers → least wasted volume → highest single-container utilisation.

**Placement constraints:**
- AABB half-open collision detection (no overlapping items)
- Three-tier support checking (vertex coverage + area overlap thresholds)
- Centre-of-mass stability (COM must remain within 60 mm of the pallet's XY centre)
- Hard boundary check (item must fit within `L × W × H`)

---

## Data Formats

### CSV (Author datasets)

```
Order, Product, Quantity, Length, Width, Height, Weight
1001, PROD_A, 2, 400, 300, 200, 12.5
1001, PROD_B, 1, 600, 400, 300, 25.0
```

- Dimensions in **millimetres**, weight in **kg**.
- Rows with non-positive dimensions, zero quantity, or unparseable fields are skipped with a warning.
- Items sharing an `Order` value are grouped and packed together.

### BR Benchmark (thpack*.txt)

```
<number of problems>
<problem_id>
<container_L> <container_W> <container_H>
<number of item types>
<type_idx> <L> <rot> <W> <rot> <H> <rot> <quantity>
...
```

- Rotation flags: `0` = fixed axis, `1` = axis may rotate.
- Container dimensions represent a truck/container (not a Euro pallet).
- Multiple problem instances can appear in one file.

---

## File Reference

### C++ Source & Headers

#### `src/main.cpp` / (entry point)
Reads input, runs Phase 1 then Phase 2, writes output. Controls the `--animated-output` flag that adds per-item animation metadata to the JSON. Also contains `buildAnimatedPhase1Container()` which assigns `phase`, `placement_order`, and `layer_index` to every Phase 1 item.

#### `include/Types.h`
All core data structures: `ItemType`, `PlacedItem`, `Container`, `Orientation`, `ExtremePoint`, `Individual`, `PackingSolution`, `Layer`, `Block`, `PalletID`.

#### `include/Config.h`
All tunable parameters as `constexpr` values — pallet dimensions (default: 1200 × 800 × 1400 mm Euro pallet), support thresholds, fill-rate limits, GA hyperparameters (population 100, parents 15, offspring 30, generations 30, stagnation limit 5). Compile-time assertions guard against invalid combinations.

#### `include/AnimatedSolution.h`
Defines `AnimatedPlacedItem` (adds `phase`, `placement_order`, `layer_index` to `PlacedItem`), `AnimatedContainer`, `GAGenerationSnapshot`, and `AnimatedSolution`. Used exclusively for `--animated-output` mode.

#### `include/AABB.h`
Inline helpers: `overlaps(a, b)` (half-open 3D collision check) and `fitsInContainer(item, container)` (boundary check).

#### `src/LayerGenerator.cpp` / `include/LayerGenerator.h`
Builds candidate homogeneous layers. Tries Full, Half, and Quarter footprints; selects the rotation that maximises items placed; applies dynamic shifting to distribute leftover space.

#### `src/BlockBuilder.cpp` / `include/BlockBuilder.h`
`filterByFillRate()` → `mergeLayers()` → `buildBlocks()`. Stacks layers onto pallets, applies 4-symmetry Hausdorff interlocking between adjacent layers, manages zone placement for Half/Quarter layers. `computeResiduals()` identifies unpacked items.

#### `src/NSGA2.cpp` / `include/NSGA2.h`
Non-dominated sorting GA. `run()` is the main loop. `fastNonDominatedSort()` assigns Pareto ranks; `crowdingDistance()` computes diversity scores; `muLambdaSelect()` merges parent + offspring pools; `extractParetoFront()` returns rank-0 individuals.

#### `src/GeneticOperators.cpp` / `include/GeneticOperators.h`
`makeSeededIndividuals()` — deterministic seeds. `crossover()` — single-point cut with permutation repair. `mutate()` — two-position swap. `tournamentSelect()` — binary tournament by rank then crowding distance.

#### `src/Packer.cpp` / `include/Packer.h`
Decodes a chromosome (item-type permutation) into a `PackingSolution` by calling the Extreme Point Engine for each item in sequence.

#### `src/ExtremePointEngine.cpp` / `include/ExtremePointEngine.h`
Maintains the list of 3D candidate placement points. `init()` seeds points from existing Phase 1 geometry. `generateFrom()` emits three new extreme points per placed item (right, behind, top). `project()` snaps points down to the nearest surface. `prune()` removes interior, dominated, and duplicate points. `placeItem()` tests both orientations at each sorted extreme point.

#### `src/SupportChecker.cpp` / `include/SupportChecker.h`
Three-tier validation: Tier 1 requires all 4 base vertices supported + ≥ 40% area overlap; Tier 2 requires 3 vertices + ≥ 50%; Tier 3 requires 2 vertices + ≥ 75%. Base vertices are inset by 10 mm per axis for practical tolerance.

#### `src/CenterOfMass.cpp` / `include/CenterOfMass.h`
Computes the mass-weighted 3D centroid of all placed items. Checks that the XY projection stays within 60 mm of the pallet's geometric centre.

#### `src/Hausdorff.cpp` / `include/Hausdorff.h`
Symmetric Hausdorff distance on 2D point sets. Applied to layer corner vertices to select the rotation that maximises interlocking between adjacent layers.

#### `src/CSVReader.cpp` / `include/CSVReader.h`
Parses the CSV format described above. Groups rows by `Order`. Logs a warning and skips any row with invalid data.

#### `src/BRReader.cpp` / `include/BRReader.h`
Parses BR benchmark `.txt` files. Returns a `vector<BRProblem>` for batch processing.

#### `src/JSONWriter.cpp` / `include/JSONWriter.h`
`writeJSON()` writes `solution.json`. `writeAnimatedJSON()` writes `solution_animated.json`, adding animation metadata and the `ga_history` array of generation snapshots.

#### `src/Logger.cpp` / `include/Logger.h`
Thin wrapper around spdlog. Initialises a coloured stderr sink and an append-mode file sink (`ga_3dbpp.log`). Auto-initialises on first use.

#### `src/PalletRegistry.cpp` / `include/PalletRegistry.h`
Catalogue of 75+ pallet standards (Euro, North American, Asia-Pacific, South America / Africa / Middle East). All dimensions stored in mm; inch-based specs converted via `× 25.4` at registration time. Enum-based lookup prevents silent typos.

#### `src/BenchmarkMain.cpp` / `src/BenchmarkRunner.cpp`
Batch runner for the BR benchmark suite. Reads all problems in a `.txt` file, solves each, and prints aggregated statistics (container count, utilisation, runtime).

---

### Server

#### `server/server.py`

Flask server with four endpoints:

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/` | Serves `visualization/index.html` |
| `GET` | `/static/<path>` | Serves assets from `visualization/` |
| `GET` | `/datasets` | Returns JSON list of datasets grouped by type |
| `POST` | `/run` | Runs the binary and returns `solution_animated.json` as JSON |

**POST /run** body: `{ "dataset": "<path relative to project root>" }`

The server validates that the requested dataset path is inside the `data/` directory (rejects path traversal), invokes `build/release/GA_3DBPP` with `--animated-output`, waits up to 300 seconds, and returns the parsed JSON. On error it returns a descriptive JSON error object.

#### `server/requirements.txt`

```
flask>=3.0.0
```

Install with: `pip install -r server/requirements.txt`

---

### Frontend

All files live under `visualization/`.

#### `index.html`
Single-page app scaffold. Defines the three-column layout (sidebar / viewport / nothing), all DOM element IDs referenced by the JavaScript modules, and a global `window.SPEED_STEPS` array consumed by `controls.js`. Loads `app.js` as the ES module entry point.

#### `js/app.js`
Top-level orchestrator.
- Fetches the dataset list on load and populates the selector.
- On Run: POSTs to `/run`, spins up the Web Worker, receives the `READY` message, creates the animator, and wires everything together.
- Owns the container navigation state (`_activeContainerIdx`, `_containerCount`).
- Routes all control events (play, pause, restart, speed, container select/prev/next) to the animator.

#### `js/worker.js`
Web Worker that pre-computes animation frames off the main thread.
- Receives a `PREPARE` message containing the full solution JSON.
- Decides animation mode: **box** (one frame per item, for < 1000 total items) or **layer** (one frame per layer group, for ≥ 1000 items).
- In layer mode, Phase 1 items are grouped by `layer_index`; Phase 2 items are appended individually after the layers.
- Posts back a `READY` message with `allFrames` and `layerManifests`.

#### `js/phase1Animator.js`
State machine that drives the animation loop.
- `prepare()` — stores the solution, frame list, animation mode, and layer manifests.
- `start(onComplete)` — begins `requestAnimationFrame` loop; calls `onComplete` when the last frame is processed.
- `pause()` / `resume()` — suspend and continue the loop.
- `restart()` — resets to frame 0 and replays.
- `stepBack()` / `stepFwd()` — jump to the previous/next container's animation start.
- `viewFinalContainer(ci)` — shows a container's complete packed state immediately and pauses; used by dot clicks and the prev/next nav bar.
- `setSpeed(s)` — updates the budget multiplier; takes effect on the next tick.
- Budget-based pacing: each RAF tick adds `speed` to a budget; frames are consumed while `budget ≥ 1.0`, up to 32 per tick.

#### `js/controls.js`
Binds DOM events to callbacks.
- Manages the play/pause button text state.
- Speed slider maps its integer value (0–4) to `SPEED_STEPS` and fires `onSpeed`.
- Container dots are built dynamically by `buildContainerDots(n)`.
- `showContainerNav(visible)` / `updateContainerNav(idx, total)` control the viewport overlay bar.
- All user actions are routed through a callback object set by `setHandlers()`.

#### `js/renderer.js`
Three.js scene wrapper.
- Uses `InstancedMesh` (one per item type) for efficient rendering of hundreds of boxes.
- Coordinate mapping: algorithm uses Z-up (X = length, Y = width, Z = height); Three.js uses Y-up. Mapping: `BoxGeometry(dx, dz, dy)`, position `(x + dx/2, z + dz/2, y + dy/2)`.
- `setupContainer(dims, items)` — draws the pallet wireframe, allocates InstancedMeshes sized to the item count per type, and focuses the camera.
- `showItem(item)` — makes one instance visible by writing its transform matrix.
- `showLayer(items)` — calls `showItem` for each item in the array.
- `highlightLayer(layerIndex)` / `resetHighlight()` — per-instance colour changes for the layer panel hover effect.
- `focusCameraOnPallet(dims)` — repositions the camera to frame the pallet.
- `ResizeObserver` keeps the canvas and camera aspect ratio in sync with the viewport.

#### `js/layerPanel.js`
Renders the layer manifest as a collapsible list in the sidebar.
- `render(ci, manifest, items)` — builds a `<details>`/`<summary>` tree; each entry shows the layer index, Z range, item count, and per-type breakdown.
- `setActiveLayer(li)` — highlights the active layer row and calls `renderer.highlightLayer`.

#### `css/styles.css`
Dark-theme stylesheet.
- CSS custom properties for all colours (`--bg-deep`, `--bg-panel`, `--accent`, etc.) and layout dimensions (`--header-h: 48px`, `--sidebar-w: 240px`).
- Flexbox layout: `#app` → vertical stack; `#main` → horizontal row (sidebar + viewport).
- Container dot states: grey (default), yellow (`active`), red (`done`).
- `#container-nav`: absolute-positioned overlay at the bottom-centre of the viewport.

---

## Output Format

### `solution.json` (standard)

```json
{
  "metadata": {
    "container_count": 2,
    "avg_utilization": 0.87,
    "phase1_item_count": 48,
    "phase2_item_count": 12
  },
  "containers": [
    {
      "id": 0,
      "dims": { "L": 1200, "W": 800, "H": 1400 },
      "utilization": 0.89,
      "items": [
        {
          "item_type_index": 5,
          "orientation": "Original",
          "x": 0, "y": 0, "z": 0,
          "dx": 400, "dy": 300, "dz": 200,
          "orig_l": 400, "orig_w": 300, "orig_h": 200
        }
      ]
    }
  ]
}
```

### `solution_animated.json` (with `--animated-output`)

Extends every item with:

| Field | Type | Description |
|-------|------|-------------|
| `phase` | int | `1` = placed by heuristic, `2` = placed by GA |
| `placement_order` | int | 0-based index within the container (animation order) |
| `layer_index` | int | Index into `layer_manifest`; `-1` for Phase 2 items |

Extends every container with:

| Field | Description |
|-------|-------------|
| `layer_manifest` | Array of layer entries: `{ layer_index, z_min, z_max, item_count, item_type_summary }` |

Adds to the top-level object:

| Field | Description |
|-------|-------------|
| `ga_history` | Array of generation snapshots: `{ generation, best_container_count, best_avg_utilization, containers }` |
| `phase1_item_count` | Count of items placed in Phase 1 |
| `phase2_item_count` | Count of items placed in Phase 2 |

---

## Dependencies

### C++

| Library | Required | Purpose |
|---------|----------|---------|
| nlohmann/json | Yes | JSON serialisation |
| spdlog | Optional | Structured coloured logging |
| Google Test | Build-time (tests only) | Unit test framework |
| C++20 STL | Yes | Containers, `<random>`, `<algorithm>`, `<filesystem>` |

Install via vcpkg:
```bash
vcpkg install nlohmann-json spdlog gtest
```

### Python

| Package | Version | Purpose |
|---------|---------|---------|
| Flask | ≥ 3.0.0 | Web server |

```bash
pip install -r server/requirements.txt
```

### JavaScript (bundled locally, no CDN)

| Library | Version | Location |
|---------|---------|----------|
| Three.js | r170 | `visualization/js/lib/three.module.min.js` |
| OrbitControls | r170 addon | `visualization/js/lib/addons/` |

---

## Configuration

All algorithm parameters live in `include/Config.h` as `constexpr` values. Changing them requires a recompile.

| Parameter | Default | Description |
|-----------|---------|-------------|
| `PALLET_L` | 1200 mm | Default pallet length |
| `PALLET_W` | 800 mm | Default pallet width |
| `PALLET_H` | 1400 mm | Default pallet height (stack limit) |
| `GA_POPULATION` | 100 | Total population size |
| `GA_NPARENTS` | 15 | Parents selected per generation |
| `GA_NOFFSPRING` | 30 | Offspring produced per generation |
| `GA_NGEN` | 30 | Maximum generations |
| `GA_STAGNATION` | 5 | Early-stop after N gens without improvement |
| `SUPPORT_AREA_T1` | 0.40 | Tier 1 minimum area overlap ratio |
| `SUPPORT_AREA_T2` | 0.50 | Tier 2 minimum area overlap ratio |
| `SUPPORT_AREA_T3` | 0.75 | Tier 3 minimum area overlap ratio |
| `COM_MAX_OFFSET` | 60 mm | Max XY offset of centre of mass from pallet centre |
| `FILL_RATE_FULL` | 0.90 | Minimum fill rate for Full/Half layers |
| `FILL_RATE_QUARTER` | 0.85 | Minimum fill rate for Quarter layers |
