# Phase 9 Summary — Interactive 3-D Visualization UI

## 1. Overview

### 1.1 What Was Accomplished

Phase 9 built a browser-based interactive viewer for GA-3DBPP solutions. The
deliverables are a Flask HTTP server (`server/server.py`) and a Six-module
Three.js front-end (`visualization/`) that together let a user select any dataset,
run the C++ algorithm, and watch boxes being placed one-by-one (or one layer at a
time) inside a 3-D pallet wireframe.

Key deliverables:

- **`include/AnimatedSolution.h`** — new structs (`AnimatedPlacedItem`,
  `AnimatedContainer`, `GAGenerationSnapshot`, `AnimatedSolution`) that extend
  the core types with per-item animation metadata (phase, placement order, layer
  index) and a per-container layer manifest.
- **`--animated-output` flag in `src/main.cpp`** — when present, `main.cpp`
  builds an `AnimatedSolution` alongside the normal `PackingSolution` and writes
  `output/solution_animated.json` via `writeAnimatedJSON`.
- **`src/JSONWriter.cpp` — `writeAnimatedJSON`** — serializes the animated
  solution including the layer manifest and `ga_history` snapshots.
- **`server/server.py`** — minimal Flask application (three endpoints) that
  serves the front-end assets, discovers available datasets, and invokes the
  release binary with `--animated-output`, returning the resulting JSON to the
  browser.
- **`visualization/index.html`** — single-page application shell: header with
  Run button, left sidebar (dataset selector, transport controls, layer panel,
  stats table), and a center Three.js canvas.
- **`visualization/js/renderer.js`** — Three.js scene manager using
  `InstancedMesh` for all item geometry; handles pallet wireframe, coordinate
  mapping, per-instance color highlighting, and responsive resize.
- **`visualization/js/worker.js`** — Web Worker that pre-computes the animation
  frame list from the solution JSON off the main thread, choosing between
  box-by-box and layer-by-layer mode based on item count.
- **`visualization/js/phase1Animator.js`** — animation state machine: play /
  pause / restart / step-back / step-forward / speed control / container
  navigation / jump-to-final-state.
- **`visualization/js/controls.js`** — DOM binding layer for transport buttons,
  speed slider, container dots, and container prev/next navigation.
- **`visualization/js/layerPanel.js`** — left-sidebar layer manifest panel with
  expandable rows and mouse-over layer highlighting wired back to the renderer.
- **`visualization/js/app.js`** — top-level entry point: dataset fetch, Run
  button handler, worker lifecycle management, and wiring of all modules together.

### 1.2 Design Decisions

**`InstancedMesh` instead of individual `BoxGeometry` objects.**  
A single `InstancedMesh` per item type means the GPU sees at most 20 draw calls
(one per palette colour) regardless of how many items are on screen. A BR1
instance with ~250 items per pallet would require 250 draw calls with naïve
`Mesh` objects; `InstancedMesh` makes it a flat constant. Unused instances are
hidden by setting their scale matrix to zero rather than being removed.

**Web Worker for frame pre-computation.**  
Building the frame list — sorting by `placement_order`, grouping by `layer_index`,
deciding animation mode — is O(n log n) in item count and blocks the main thread
for large solutions. Running it in a Worker keeps the UI responsive during the
"Running algorithm…" overlay and means the animation starts instantly once the
worker posts `READY`.

**Two animation modes: `box` and `layer`.**  
For solutions with ≤ 1 000 items the worker emits one frame per item (box mode),
showing each box drop individually. Above 1 000 items it emits one frame per Phase
1 layer (all items in that layer appear simultaneously) followed by individual
frames for Phase 2 residual items. The threshold avoids animation playback
becoming uselessly fast for large BR instances while keeping small author-CSV
demos visually informative.

**`placement_order` as the universal frame key.**  
Each `AnimatedPlacedItem` carries a `placement_order` (0-based integer, unique
within its container). The worker emits frames as arrays of `placement_order`
values; the animator resolves them via an O(1) `Map` lookup built at container
setup time. This decouples the frame schedule from the item array layout —
frames can reference single items or whole layers with the same data structure.

**`layer_index` band algorithm in `main.cpp`.**  
Phase 1 containers store items in layer-stacking order, but the item structs have
no layer field. `buildAnimatedPhase1Container` groups items into horizontal bands
by checking whether an item's z-range fits within any existing band (±1 mm
tolerance for integer rounding). The first unmatched item starts a new band. This
runs in O(n × L) (n items, L layers), which is O(n) in practice because L is
typically 5–10 for Euro-pallet stacks.

**Coordinate mapping: algorithm Z-up → Three.js Y-up.**  
The algorithm uses a right-hand Z-up coordinate system: X = length along the
pallet, Y = width, Z = height. Three.js is Y-up. The `algoToThree` function in
`renderer.js` maps:
```
Three.js center X = algo x + dx/2
Three.js center Y = algo z + dz/2    ← height in algo becomes Y in Three.js
Three.js center Z = algo y + dy/2    ← width in algo becomes Z in Three.js
Three.js scale    = (dx, dz, dy)
```
The pallet wireframe uses `BoxGeometry(L, H, W)` centred at `(L/2, H/2, W/2)`
to match. This mapping is self-contained in `renderer.js`; no other module needs
to know about it.

**Path-traversal guard in `server.py`.**  
The `/run` endpoint accepts a dataset path from the browser, resolves it with
`pathlib.Path.resolve()`, and checks that the result starts with the resolved
`data/` directory path before invoking the binary. This prevents a crafted
`dataset` value such as `../../etc/passwd` from being passed to the subprocess.

**No CDN dependencies.**  
Three.js r170 (ES module build) and `OrbitControls` are bundled in
`visualization/js/lib/`. The page uses an `importmap` to alias `"three"` and
`"three/addons/"` to local paths, so the viewer works fully offline on the dev
machine without any build step.

### 1.3 Files Created / Modified

| File | Action | Purpose |
|------|--------|---------|
| `include/AnimatedSolution.h` | Created | Animation-specific structs: `AnimatedPlacedItem`, `AnimatedContainer`, `GAGenerationSnapshot`, `AnimatedSolution` |
| `src/main.cpp` | Modified | Added `--animated-output` flag, `buildAnimatedPhase1Container`, Phase 2 item tagging, `writeAnimatedJSON` call |
| `src/JSONWriter.cpp` | Modified | Added `writeAnimatedJSON` (layer manifest + ga_history serialization) |
| `include/JSONWriter.h` | Modified | Declared `writeAnimatedJSON` |
| `server/server.py` | Created | Flask server: `/`, `/static/<path>`, `/datasets`, `POST /run` |
| `server/requirements.txt` | Created | `flask>=3.0.0` |
| `server/start.sh` | Created | One-liner launcher: `python3 server/server.py` from project root |
| `visualization/index.html` | Created | SPA shell; imports Three.js via importmap, loads `app.js` as ES module |
| `visualization/css/styles.css` | Created | Dark-theme layout (CSS variables, sidebar, transport bar, stats table, layer rows) |
| `visualization/js/app.js` | Created | Top-level init: dataset load, Run handler, Worker lifecycle, control wiring |
| `visualization/js/renderer.js` | Created | Three.js scene, `InstancedMesh` pool, `showItem`/`showLayer`/`highlightLayer`/`clearItems`, OrbitControls |
| `visualization/js/worker.js` | Created | Web Worker: sorts items, selects animation mode, builds frame list, posts `READY` |
| `visualization/js/phase1Animator.js` | Created | Animation state machine: `_tick` RAF loop, container transitions, `viewFinalContainer` |
| `visualization/js/controls.js` | Created | DOM bindings: play/pause/restart/step, speed slider, container dots, prev/next nav |
| `visualization/js/layerPanel.js` | Created | Layer manifest `<details>` list, `setActiveLayer` scroll-into-view, hover highlight |

### 1.4 Prerequisites

All prior phases complete (Phases 0–8). Phase 9 depends on:
- `main.cpp` pipeline (Phase 7) as the algorithm entry point
- `writeJSON` / `JSONWriter` (Phase 7) as the pattern for the animated writer
- `BlockBuilder` (Phase 4) as the source of Phase 1 container layout
- `Packer::decode` + `NSGA2::run` (Phases 6–7) for the Phase 2 items
- The release binary built with `cmake --build --preset release`

---

## 2. Step-by-Step Implementation Log

### Step 1 — Defined `AnimatedSolution.h` structs

`include/AnimatedSolution.h` introduces four structs that carry animation
metadata without modifying the core `PlacedItem` / `Container` types:

```cpp
struct AnimatedPlacedItem {
    // mirrors PlacedItem fields (item_type_index, orientation, x/y/z, dx/dy/dz)
    int phase           = 1;    // 1 = Phase 1 block, 2 = Phase 2 GA residual
    int placement_order = 0;    // 0-based sequence within this container
    int layer_index     = -1;   // index into layer_manifest; -1 for Phase 2 items
};

struct LayerManifestEntry {
    int layer_index = 0;
    int z_min = 0;  int z_max = 0;  int item_count = 0;
    std::vector<ItemTypeSummary> item_type_summary;
};

struct AnimatedContainer {
    int L, W, H;
    std::vector<AnimatedPlacedItem> items;
    std::vector<LayerManifestEntry> layer_manifest;   // empty for pure Phase 2 containers
};

struct AnimatedSolution {
    std::vector<AnimatedContainer>    containers;
    std::vector<GAGenerationSnapshot> ga_history;
    int total_items, phase1_item_count, phase2_item_count;
};
```

`ItemTypeSummary` (also in the header) stores `{item_type_index, count}` for the
layer panel's "Type 3 × 12" breakdown.

### Step 2 — Layer band algorithm (`buildAnimatedPhase1Container`)

`buildAnimatedPhase1Container` in `src/main.cpp` converts a `Container` (from
Phase 1) into an `AnimatedContainer`:

1. Copy each `PlacedItem` to an `AnimatedPlacedItem`, assigning `phase=1` and a
   sequential `placement_order`.
2. Run the band-assignment pass: for each item sorted by their existing order,
   find the first band whose `[z_min, z_max+1]` range contains the item's full
   z-extent. If found, merge the item's top into the band's `z_max`. If not, open
   a new band.
3. Build `layer_manifest` from the final band list, then accumulate
   `item_count` and `item_type_summary` per band by iterating all items again.

The `+1 mm` tolerance on `z_max` absorbs integer-rounding artefacts from Phase 1
dynamic shifting without ever merging physically distinct layers (minimum item
height in all datasets is 25 mm).

### Step 3 — Phase 2 item tagging in `main.cpp`

After `Packer::decode` returns `best_solution`, `main.cpp` walks the containers
and appends `AnimatedPlacedItem` entries with `phase=2`, `layer_index=-1`, and
`placement_order` continuing from the Phase 1 count already stored in
`anim_sol.containers[ci]`. New containers opened by the GA (not present in
`anim_sol.containers` yet) are added as empty `AnimatedContainer` structs first.

### Step 4 — GA history snapshots

`NSGA2::run` accepts an optional `std::vector<GASnapshot>*` out-parameter. When
`--animated-output` is active, `main.cpp` passes a pointer and the GA appends a
snapshot at every recorded generation. After `run()` returns, `main.cpp` converts
each `GASnapshot` to a `GAGenerationSnapshot` by re-encoding every
`PlacedItem` as an `AnimatedPlacedItem` with `phase=2` (GA snapshots are
exclusively Phase 2 items — Phase 1 blocks are constant across generations).

### Step 5 — `writeAnimatedJSON` in `JSONWriter.cpp`

`writeAnimatedJSON` serializes `AnimatedSolution` using nlohmann/json:

**`metadata`** object:
```json
{
  "container_count": 2,
  "avg_utilization": 0.498,
  "total_items": 168,
  "phase1_item_count": 156,
  "phase2_item_count": 12,
  "ga_generations_recorded": 50
}
```

**`containers`** array — one object per `AnimatedContainer`:
```json
{
  "id": 0,
  "dims": { "L": 587, "W": 233, "H": 220 },
  "layer_manifest": [
    { "layer_index": 0, "z_min": 0, "z_max": 119,
      "item_count": 24,
      "item_type_summary": [{"item_type_index": 0, "count": 24}] }
  ],
  "items": [
    { "item_type_index": 0, "orientation": "Original",
      "x": 0, "y": 0, "z": 0, "dx": 120, "dy": 80, "dz": 100,
      "orig_l": 120, "orig_w": 80, "orig_h": 100,
      "phase": 1, "placement_order": 0, "layer_index": 0 }
  ]
}
```

`layer_manifest` is omitted entirely for containers that have no Phase 1 items.
`layer_index` is serialized as JSON `null` for Phase 2 items (`layer_index = -1`
in C++).

**`ga_history`** array — one snapshot per recorded GA generation.

### Step 6 — Flask server (`server/server.py`)

Three endpoints:

**`GET /`** — serves `visualization/index.html` via `send_from_directory`.

**`GET /static/<path>`** — serves any file under `visualization/` (CSS, JS,
Three.js library bundles). The path variable is consumed by Flask's
`send_from_directory`, which prevents directory traversal by design.

**`GET /datasets`** — scans `data/` recursively for `.csv` and `.txt` files
(excluding `test_fixtures/`), groups them into `"Author"` or `"BR Benchmark"`
based on whether they live under `data/br_benchmark/`, and returns:
```json
{ "datasets": [
    { "label": "order_001", "path": "data/order_001.csv", "group": "Author" },
    { "label": "BR Benchmark (thpack1)", "path": "data/br_benchmark/thpack1.txt",
      "group": "BR Benchmark" }
] }
```

**`POST /run`** — accepts `{"dataset": "<relative path>"}`, resolves it with
`Path.resolve()`, rejects any path that does not start with `data/` (path
traversal guard), then invokes:
```
GA_3DBPP <dataset_path> output/solution_animated.json --animated-output
```
with a 300-second timeout. On success it reads and returns
`output/solution_animated.json` as JSON. On binary failure it returns HTTP 500
with the last 2 KB of stderr for diagnosis.

### Step 7 — `visualization/index.html` layout

The page has three regions:

- **`#header`** — title (`GA_3DBPP Demo`), a spinner shown during the `/run`
  request, and the Run button.
- **`#sidebar`** (left) — stacked sections:
  - Dataset `<select>` populated from `/datasets` with `<optgroup>` per group.
  - Transport controls: Restart / Step-back / Play-Pause / Step-forward buttons;
    Speed slider (5 steps: 0.25×, 0.5×, 1×, 2×, 5×); container dot indicators.
  - Layer Breakdown panel (`#layer-list`) — filled by `LayerPanel.render()`.
  - Stats table — Dataset, Containers, Items, Avg Util, GA Gens, Pallet Util.
- **`#viewport-wrap`** (center) — the Three.js `<canvas>`, a container
  prev/next navigation bar (`#container-nav`), and a `#loading-overlay` shown
  while the server is running the algorithm.
- **`#error-toast`** — positioned fixed, auto-dismisses after 5 seconds.

`window.SPEED_STEPS` is set inline so `controls.js` can read it before the ES
module import resolves.

### Step 8 — `renderer.js` (Three.js scene)

`Renderer` owns the Three.js scene, camera, lights, and `OrbitControls`:

- **Scene background:** `#1a1a2e` (dark navy).
- **Camera:** `PerspectiveCamera(50°, aspect, 1, 60000)`, initially positioned at
  `(2000, 1600, 2500)` mm and re-focused by `focusCameraOnPallet` after each
  container load. The look-at target is the pallet centre.
- **Lights:** `AmbientLight(0xffffff, 0.55)` + `DirectionalLight(0xffffff, 0.9)`
  from position `(3000, 5000, 2000)`.
- **OrbitControls:** damping factor 0.08 for smooth mouse drag.
- **Pallet wireframe:** `EdgesGeometry` of a `BoxGeometry(L, H, W)` with a
  semi-transparent white `LineBasicMaterial` (opacity 0.28). Rebuilt by
  `_setPalletWire` each time a new container is set up; the previous one is
  removed and its geometry disposed.
- **`InstancedMesh` pool:** `_allocateMeshes` counts item capacity per type,
  creates one `InstancedMesh(BoxGeometry(1,1,1), MeshLambertMaterial(colour), n)`
  per type, and sets all instance matrices to zero scale (invisible). `showItem`
  scales and positions the next unused instance; `clearItems` zeros all matrices.
- **`highlightLayer(li)`** tints every item not belonging to layer `li` to
  `#1c1c2c` (near-black) using `setColorAt`; `resetHighlight` restores palette
  colours. This is triggered by `LayerPanel` mouse-enter/leave events.
- **`_onResize`:** a `ResizeObserver` on the canvas parent updates the camera
  aspect ratio and renderer size whenever the viewport changes.

### Step 9 — `worker.js` (frame pre-computation)

The worker receives a `PREPARE` message containing the full solution JSON.

1. For each container, split items into `p1Items` (phase=1) and `p2Items`
   (phase≠1), both sorted by `placement_order`.
2. Compute `totalItems` across all containers. If `totalItems > 1000`, use
   `animationMode = 'layer'`; otherwise `'box'`.
3. In **box mode** emit one frame per item: `{containerIndex, itemIndices: [placement_order]}`.
4. In **layer mode** group `p1Items` by `layer_index` into a `Map`, sort by
   layer index, emit one frame per layer group (all placement orders in that
   layer), then emit individual frames for each `p2Item`.
5. Build `layerManifests`: for each container that has a non-empty
   `layer_manifest` in the solution JSON, store it by container index.
6. Post `{type: 'READY', animationMode, allFrames, layerManifests}`.

### Step 10 — `phase1Animator.js` (animation state machine)

`Phase1Animator.prepare()` receives the solution, frame list, animation mode, and
layer manifests. `Phase1Animator.start()` initialises `_frameIdx = 0`,
`_budget = 0`, calls `_setupContainer(0)` and enters the `requestAnimationFrame`
loop (`_tick`).

**`_tick` budget system:** each RAF callback adds `_speed` to `_budget`. While
`_budget >= 1.0` and fewer than 32 frames have been processed this callback, the
animator applies the next frame, decrements `_budget` by 1.0, and advances
`_frameIdx`. At speed 2×, two frames are processed per RAF; at 0.25×, a frame
is processed every fourth RAF.

**Container transitions:** when the next frame's `containerIndex` differs from
the current one, the animator marks the current dot `'done'`, sets a
`setTimeout` of 800 ms, and returns — this creates a brief pause between pallets
before the scene clears and rebuilds for the next container.

**`viewFinalContainer(ci)`** — used for inspection (dot click / prev / next):
cancels any pending RAF, calls `setupContainer` and `showLayer` with all items,
sets `_paused = true`, and marks all dots appropriately. This does not reset
`_frameIdx`, so the user can press Play to continue from where the animation was.

**`_applyFrame`** resolves `placement_order` values to items via `_itemLookup`,
calls `showItem` or `showLayer`, and updates the active layer row in the panel.

**`_updateStats`** reads `solution.metadata` and computes per-container
utilization inline, writing directly to the stats table cells.

### Step 11 — `controls.js` (DOM bindings)

`Controls.setHandlers` wires the transport buttons to the callbacks passed by
`app.js`. The play/pause button toggles its text content between `▶` and `⏸`
and calls the corresponding handler. The speed slider maps its integer value
(0–4) to `SPEED_STEPS[idx]` and calls `onSpeed`. Container dots are built
dynamically by `buildContainerDots(n)` — one `<div class="container-dot">` per
container — and each calls `onContainerSelect(i)`. `setContainerDot(idx, state)`
sets the CSS class to `''`, `'active'`, or `'done'` for visual feedback.

### Step 12 — `layerPanel.js` (layer breakdown sidebar)

`LayerPanel.render(containerIndex, manifest, items)` builds one `<details>` element
per `LayerManifestEntry`. The `<summary>` shows the layer index, z-range in mm,
and item count. An inner `<div>` lists the type breakdown (`Type 0 × 12,
Type 1 × 4`). `mouseenter` on a `<details>` calls
`renderer.highlightLayer(lme.layer_index)` and `mouseleave` calls
`renderer.resetHighlight()`. `setActiveLayer(li)` toggles the CSS class `active`
on the matching row and scrolls it into view smoothly — called by the animator
on each frame to track the in-progress layer.

---

## 3. JSON Schema Reference (`solution_animated.json`)

```
root
├── metadata
│   ├── container_count          int
│   ├── avg_utilization          float   (0–1)
│   ├── total_items              int
│   ├── phase1_item_count        int
│   ├── phase2_item_count        int
│   └── ga_generations_recorded  int
├── containers[]
│   ├── id                       int
│   ├── dims { L, W, H }         int mm
│   ├── layer_manifest[]         (omitted for pure Phase 2 containers)
│   │   ├── layer_index          int
│   │   ├── z_min, z_max         int mm
│   │   ├── item_count           int
│   │   └── item_type_summary[]  { item_type_index, count }
│   └── items[]
│       ├── item_type_index       int
│       ├── orientation           "Original" | "Rotated90"
│       ├── x, y, z               int mm  (bottom-left-front corner)
│       ├── dx, dy, dz            int mm  (placed dimensions)
│       ├── orig_l, orig_w, orig_h int mm (unrotated dimensions, for tooltips)
│       ├── phase                 1 | 2
│       ├── placement_order       int     (0-based, unique per container)
│       └── layer_index           int | null (null for Phase 2 items)
└── ga_history[]
    ├── generation                int
    ├── best_container_count      int
    ├── best_avg_utilization      float
    └── best_containers[]         (same structure as containers[], no layer_manifest)
```

---

## 4. How to Run the Viewer

```bash
# 1. Build the release binary (required once after any C++ change)
cmake --build --preset release

# 2. Install the Python dependency (required once)
pip install flask>=3.0.0
# or: pip install -r server/requirements.txt

# 3. Start the server from the project root
bash server/start.sh
# Server listens at http://localhost:5000

# 4. Open http://localhost:5000 in a browser
# 5. Select a dataset from the dropdown
# 6. Click "Run" — the algorithm executes on the server (~5–40 s depending on dataset)
# 7. Watch the animation; use transport controls to inspect the solution
```

---

## 5. Final File Structure After Phase 9

```
GA_3DBPP/
├── include/
│   └── AnimatedSolution.h    ← NEW
├── src/
│   ├── main.cpp              ← MODIFIED (--animated-output, layer bands, GA history)
│   └── JSONWriter.cpp        ← MODIFIED (writeAnimatedJSON)
├── include/
│   └── JSONWriter.h          ← MODIFIED (writeAnimatedJSON declaration)
├── server/
│   ├── server.py             ← NEW
│   ├── requirements.txt      ← NEW
│   └── start.sh              ← NEW
└── visualization/
    ├── index.html            ← NEW
    ├── css/
    │   └── styles.css        ← NEW
    └── js/
        ├── app.js            ← NEW
        ├── renderer.js       ← NEW
        ├── worker.js         ← NEW
        ├── phase1Animator.js ← NEW
        ├── controls.js       ← NEW
        ├── layerPanel.js     ← NEW
        └── lib/              ← Three.js r170 ES module bundle + OrbitControls
```

---

## 6. What Changes in Phase 10

Phase 10 would extend the viewer with:

- **GA history playback** — a second animation mode that steps through
  `ga_history` snapshots to show the Pareto front converging generation by
  generation.
- **Tooltip on hover** — raycaster in `renderer.js` identifies the hovered
  instance and shows a floating card with item type, dimensions, orientation,
  phase, and layer assignment.
- **Export** — a "Save PNG" button that calls `renderer.domElement.toDataURL()`
  and triggers a download.
- **BR instance picker** — extend the `/datasets` endpoint and front-end UI to
  support selecting a specific instance number within a multi-instance `.txt`
  file, rather than always using instance 1.
