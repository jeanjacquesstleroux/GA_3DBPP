# GA_3DBPP — UI Animation Rework: Project Plan for Claude Code

**Project root:** `~/repos/GA_3DBPP`
**Audience:** Claude Code agent
**Goal:** Replace the static drag-and-drop visualization with a live, animated demo that runs the algorithm on a selected dataset and animates Phase 1 (layer/block heuristic) and Phase 2 (genetic algorithm) with clear visual separation between phases.

---

## Architectural Overview

### Problem with the Current Setup
The current `visualization/index.html` loads a static `output/solution.json` via drag-and-drop. That JSON contains only the **final packed state** — no placement ordering, no phase attribution, no layer grouping, and no GA generation history. The entire animation system depends on data that does not yet exist in the output. Additionally, a static HTML file cannot invoke the C++ binary on demand; a local server layer is required.

### Solution Architecture (three components)

```
[User Browser]
     │  GET /          → served HTML/JS
     │  GET /datasets  → list of available CSV files
     │  POST /run      → invoke binary, stream back extended JSON
     ▼
[Local Python Server]   server/server.py
     │  subprocess: ./build/release/GA_3DBPP <path> output/solution_animated.json --animated-output
     ▼
[C++ Binary]            builds to build/release/GA_3DBPP
     │  writes extended solution JSON to output/solution_animated.json
     ▼
[Extended JSON]         output/solution_animated.json
     ▼
[Animation Engine]      visualization/js/animator.js + renderer.js
     │  Three.js (WebGL), Chart.js (fitness curve)
```

---

## Component 1: Extended C++ JSON Output

### 1.1 — New JSON Schema

The C++ binary must be extended to write `output/solution_animated.json` with the following schema. This is **in addition to** the existing `output/solution.json` (do not break existing output).

**Key architectural point:** Phase 2 residuals are placed by `Packer::decode` INTO the same `Container` objects that Phase 1 produced (`sol.containers = seedContainers` — Phase 1 items remain, Phase 2 items are appended to the same `.items` vector). A single physical pallet can contain both Phase 1 block items and Phase 2 EP-placed residuals. The schema therefore uses a **unified `containers[]` array** with a **per-item `phase` field** rather than separate top-level Phase 1 / Phase 2 arrays.

```jsonc
{
  "metadata": {
    "container_count": <int>,          // total containers used (Phase 1 + any new Phase 2)
    "avg_utilization": <double>,       // 0.0–1.0, existing field
    "total_items": <int>,              // sum of all placed items across all containers
    "phase1_item_count": <int>,        // NEW: items placed by Phase 1 layer/block heuristic
    "phase2_item_count": <int>,        // NEW: items placed by Phase 2 EP/GA
    "ga_generations_recorded": <int>   // NEW: entries in ga_history (0 if GA was skipped)
  },

  "containers": [                      // ALL containers — unified list, both phases
    {
      "id": <int>,                     // 0-based index
      "dims": { "L": <int>, "W": <int>, "H": <int> },
      "layer_manifest": [              // NEW: one entry per distinct Phase 1 layer
        {                              // (absent / empty array if container has no Phase 1 items)
          "layer_index": <int>,        // 0-based, index within this container
          "z_min": <int>,              // mm, bottom of layer
          "z_max": <int>,              // mm, top of layer
          "item_count": <int>,
          "item_type_summary": [       // for the layer info panel in UI
            { "item_type_index": <int>, "count": <int> }
          ]
        }
      ],
      "items": [
        {
          // All existing fields preserved:
          "item_type_index": <int>,
          "orientation": "Original" | "Rotated90",
          "x": <int>, "y": <int>, "z": <int>,
          "dx": <int>, "dy": <int>, "dz": <int>,
          "orig_l": <int>, "orig_w": <int>, "orig_h": <int>,
          // NEW fields:
          "phase": 1 | 2,             // 1 = Phase 1 block/layer item; 2 = Phase 2 EP/GA item
          "placement_order": <int>,    // 0-based, global within this container;
                                       // Phase 1 items occupy 0..N-1,
                                       // Phase 2 items continue at N..M-1
          "layer_index": <int> | null  // index into layer_manifest; null for phase-2 items
        }
      ]
    }
  ],

  "ga_history": [                      // NEW: one entry per recorded generation
                                       // Empty array ([]) when Phase 1 packs all items
                                       // (residualTypes was empty — GA was skipped)
    {
      "generation": <int>,             // 0-based
      "best_container_count": <int>,   // objectives[0] of best individual in this generation
      "best_avg_utilization": <double>,// –objectives[1] of best individual (0.0–1.0)
      "best_containers": [             // full container+items snapshot of the best individual
        {                              // (phase-2 items added on top of phase-1 base)
          "id": <int>,
          "dims": { "L": <int>, "W": <int>, "H": <int> },
          "items": [ /* same item structure; layer_manifest omitted for ga_history */ ]
        }
      ]
    }
  ]
}
```

**Selecting the "best" individual per generation for `ga_history`:** use the same `selectBest()` rule as `main.cpp` — minimum `objectives[0]` (container count), then minimum `objectives[2]` (wasted volume), then maximum `aux_max_util`. This guarantees the chart's fitness curve matches the final selected solution.

### 1.2 — Files to Modify

**`src/JSONWriter.cpp` / `include/JSONWriter.h`**

Add a new method `void writeAnimatedSolution(const AnimatedSolution& sol, const std::string& path)`.

`AnimatedSolution` is a new struct (defined in `include/AnimatedSolution.h`) that holds:
- All containers (unified list) with `PlacedItem` extended with `phase`, `placement_order`, and `layer_index`
- `layer_manifest` per container (only Phase 1 items contribute)
- `ga_history` vector of `{generation, best_container_count, best_avg_utilization, containers_snapshot}`

**`src/Packer.cpp`** (Phase 1 instrumentation)

The block builder already appends `PlacedItem` entries to each container's `.items` vector. Under `--animated-output`, after all Phase 1 blocks are assembled:
1. Walk each container's `.items` (which are Phase 1 items at this point) in order. Assign `phase = 1` and `placement_order = 0, 1, 2, …` (reset per container).
2. Run the layer assignment algorithm from §1.3 to assign `layer_index` to each Phase 1 item.
3. Build the `layer_manifest` from the collected layer assignments.

**For Phase 2 items:** `Packer::decode` appends residual `PlacedItem` entries to containers by continuing from the EP engine. After decode completes (under `--animated-output`), walk the newly added Phase 2 items (those whose `placement_order` is not yet set), assign `phase = 2`, `layer_index = -1` (written as JSON `null`), and `placement_order` continuing from where Phase 1 left off in that container.

**`src/NSGA2.cpp`** (Phase 2 GA history)

In `NSGA2::run`, after evaluating each generation:
1. `evaluateFitness` already calls `Packer::decode` internally and produces a `PackingSolution`. To capture the snapshot cheaply, add a new optional `PackingSolution*` out-parameter to `evaluateFitness` — if non-null, the solution is written there instead of discarded. This avoids a redundant re-decode.
2. If `gen % interval == 0 || gen == maxGenerations - 1` (using the throttle from §4.3), identify the current best individual (using `selectBest()` logic), obtain its solution via the cached output from evaluateFitness, and append to the `ga_history` vector.
3. The `--animated-output` flag is threaded into `NSGA2::run` as a bool parameter. When false (normal runs), none of this code runs.

**`src/main.cpp`**

Add `--animated-output` as an **optional** CLI flag. The existing positional argument interface is preserved:

```
GA_3DBPP <input.csv> [output.json] [--animated-output]
```

When `--animated-output` is present, construct the `AnimatedSolution` struct and call `JSONWriter::writeAnimatedSolution(...)` to write `output/solution_animated.json` alongside the existing `output/solution.json`.

**Important:** `main.cpp` currently uses `CSVReader` only. The BR benchmark datasets (`.txt` files) are parsed by `BRReader`, used in `BenchmarkMain.cpp`. The server must handle both formats. Add format detection to `main.cpp`: if the input file has a `.txt` extension, use `BRReader` to parse instance 1 (mirroring what `BenchmarkMain` does for single-instance runs). This does not change algorithm logic — it only adds an I/O branch.

### 1.3 — Layer Assignment Algorithm (for Phase 1 items in `Packer.cpp`)

Use the following algorithm to assign `layer_index` to Phase 1 items after a container is fully packed by the block builder. This runs only under `--animated-output` and must not affect any placement decisions.

```
layers = []   // list of {z_min, z_max}

for each placed item (in placement_order, Phase 1 items only):
    item_z_top = item.z + item.dz
    matched = false
    for i, layer in enumerate(layers):
        if item.z >= layer.z_min AND item_z_top <= layer.z_max + TOLERANCE:
            item.layer_index = i
            layer.z_max = max(layer.z_max, item_z_top)
            matched = true
            break
    if not matched:
        item.layer_index = len(layers)
        layers.append({z_min: item.z, z_max: item_z_top})

TOLERANCE = 1   // mm, handles minor alignment gaps between block layers
```

**Note on growing z_max:** `layer.z_max` is expanded when a matching item is taller than prior items in the same layer. In Phase 1 block placement, items within a layer are homogeneous (same item type, same height), so `z_max` should not grow meaningfully in practice. The TOLERANCE handles only sub-millimeter gaps.

---

## Component 2: Local Python Server

### 2.1 — Purpose
A lightweight Flask server that:
1. Serves `visualization/index.html` and its assets.
2. Scans the data directory and returns available datasets.
3. Accepts a run request, invokes the binary, returns the extended JSON.

### 2.2 — File: `server/server.py`

Install dependency: `pip install flask` (add to `server/requirements.txt`).

```
GET  /                          → serve visualization/index.html
GET  /static/<path>             → serve visualization/ assets
GET  /datasets                  → return JSON list of available datasets (see below)
POST /run  body: {"dataset": "<relative path from project root>"}
                               → invoke binary, poll for output/solution_animated.json,
                                  return its contents as JSON
```

**`GET /datasets` response format:**
```json
{
  "datasets": [
    { "label": "Author Dataset 1", "path": "data/author/dataset1.csv", "group": "Author" },
    { "label": "BR Benchmark 1 (thpack1)", "path": "data/br_benchmark/thpack1.txt", "group": "BR Benchmark" },
    ...
  ]
}
```
The server scans `data/` recursively. Group logic:
- Files in `data/br_benchmark/` → group `"BR Benchmark"`, label `"BR Benchmark N (filename)"`
- All other CSVs → group `"Author"`, label from filename

**`POST /run` behavior:**
1. Validate that the provided path is within the project's `data/` directory (security check — reject path traversal).
2. Delete any stale `output/solution_animated.json`.
3. Run (note: positional args, not `--dataset` flag):
   ```
   subprocess.run(
       ["./build/release/GA_3DBPP", <path>, "output/solution_animated.json", "--animated-output"],
       cwd=PROJECT_ROOT, timeout=300
   )
   ```
4. If return code is 0 and `output/solution_animated.json` exists, read and return it as JSON with status 200.
5. On timeout or non-zero return code, return status 500 with `{"error": "..."}`.

**Important:** the server uses Flask's `send_from_directory` for static assets, not Flask's built-in `static_folder` mechanism, to avoid path conflicts with the `/static/<path>` route.

### 2.3 — File: `server/start.sh`
```bash
#!/bin/bash
cd "$(dirname "$0")/.."
python server/server.py
```
The server must print: `GA_3DBPP demo server running at http://localhost:5000`

---

## Component 3: UI Rework

### 3.1 — File Structure

```
visualization/
  index.html               ← main entry point (rewrite)
  js/
    app.js                 ← top-level init, dataset fetch, run button handler
    animator.js            ← animation state machine (Phase 1 and Phase 2)
    renderer.js            ← Three.js scene setup and update API
    phase1Animator.js      ← Phase 1 box/layer animation logic
    phase2Animator.js      ← Phase 2 GA animation logic (fitness chart + packing update)
    layerPanel.js          ← Layer manifest side panel
    controls.js            ← Play/Pause/Speed/Phase-nav control bindings
    worker.js              ← Web Worker: frame pre-computation
    lib/
      three.module.min.js  ← Three.js r170 (local copy — no CDN)
      OrbitControls.js     ← Three.js r170 addon (local copy)
      chart.umd.min.js     ← Chart.js 4.x (local copy — no CDN)
  css/
    styles.css
```

**Library setup (one-time, before Task 6):** Download Three.js r170 (`three.module.min.js`, `OrbitControls.js`) and Chart.js 4.x (`chart.umd.min.js`) from their official GitHub release pages and place them in `visualization/js/lib/`. These must be local files — no CDN links anywhere in the codebase — to allow fully offline use.

### 3.2 — Layout

```
┌──────────────────────────────────────────────────────────────────┐
│  GA_3DBPP Demo                                         [▶ Run]   │
├──────────────┬──────────────────────────────┬────────────────────┤
│              │                              │ PHASE 2 PANEL      │
│  Dataset     │                              │ (hidden until      │
│  Selector    │   Three.js 3D Viewport       │  Phase 2 begins)   │
│              │                              │                    │
│  [Controls]  │   [Phase indicator banner]   │ Fitness curve      │
│              │                              │ (Chart.js canvas)  │
│  [Layer      │                              │                    │
│   Manifest   │                              │ Generation info    │
│   Panel]     │                              │ text               │
│              │                              │                    │
│  [Stats]     │                              │                    │
└──────────────┴──────────────────────────────┴────────────────────┘
```

The Phase 2 right panel is hidden (`display: none`) during Phase 1 and revealed with a CSS transition when Phase 2 begins.

### 3.3 — Dataset Selector (`app.js`)

On page load, `fetch('/datasets')` and populate a `<select>` element grouped with `<optgroup label="Author">` and `<optgroup label="BR Benchmark">`.

Run button click handler:
1. Disable Run button, show spinner.
2. `fetch('/run', { method: 'POST', body: JSON.stringify({ dataset: selectedPath }) })`
3. On success: pass JSON to `worker.js` via `postMessage`, wait for pre-computed frame data.
4. On error: show error toast with server's error message.
5. Re-enable Run button after animation completes or on error.

### 3.4 — Web Worker (`worker.js`)

The extended JSON for large BR datasets can be several MB. `Response.json()` (called on the main thread, unavoidable with the Fetch API) parses the JSON synchronously; the resulting object is then transferred to the worker via `postMessage` (structured clone). Frame pre-computation — which is the CPU-intensive step — runs inside the worker.

**Input message (from main thread):**
```json
{ "type": "PREPARE", "solution": { /* full solution_animated.json contents */ } }
```

**Processing in worker:**
1. Receive the already-parsed solution object (passed via structured clone from main thread).
2. For Phase 1: for each container (in order), filter items by `phase === 1` and sort by `placement_order`.
   - Compute `totalPhase1Items = sum of phase-1 items across all containers`.
   - Determine `animationMode` per §4.2 thresholds (defined as constants at the top of `worker.js`).
   - Build `phase1Frames`: array of frame objects, one per item (box mode) or one per layer (layer mode). Each frame: `{ containerIndex, itemIndices: [], layerIndex? }`.
3. For Phase 2: build `phase2Frames`: one frame per `ga_history` entry. Each frame: `{ generation, best_container_count, best_avg_utilization, containers }`. If `ga_history` is empty, set `phase2Frames = []` and skip Phase 2 animation entirely.
4. Post back `{ type: "READY", phase1Frames, phase2Frames, animationMode, layerManifests }`.

**Output message (to main thread):**
```json
{
  "type": "READY",
  "animationMode": "box" | "layer",
  "phase1Frames": [ ... ],
  "phase2Frames": [ ... ],
  "layerManifests": { "containerIndex": [ { "layer_index", "z_min", "z_max", "item_count", "item_type_summary" } ] }
}
```

### 3.5 — Three.js Renderer (`renderer.js`)

**Scene setup:**
- Background: `0x1a1a2e` (dark navy)
- Ambient light + directional light
- `OrbitControls` for camera (enabled at all times so user can rotate during animation)
- Pallet wireframe: `THREE.EdgesGeometry` of a `BoxGeometry` matching `dims.L/W/H`, rendered in white

**Coordinate mapping:** Three.js Y is up. The algorithm uses Z-up. Map: algorithm `(dx, dy, dz)` → Three.js `BoxGeometry(dx, dz, dy)`, and position `(x, y, z)` → Three.js `(x + dx/2, z + dz/2, y + dy/2)`. This matches the existing Phase 9 visualization coordinate convention.

**Box rendering with `InstancedMesh`:**

Group items by `item_type_index`. For each group, create one `THREE.InstancedMesh` with capacity equal to the max count of that item type across all containers. Use the existing 20-color PALETTE array for materials.

API exposed by `renderer.js`:
```javascript
renderer.init(container, palletDims)          // set up scene for a new pallet
renderer.showItem(item, instanceIndex)        // set transform + visibility for one item instance
renderer.showLayer(items)                     // show all items in a layer simultaneously
renderer.clearScene()                         // remove all item instances
renderer.highlightLayer(layerIndex)           // dim all layers except this one
renderer.resetHighlight()
renderer.focusCameraOnPallet(dims)            // animate camera to frame the pallet
```

All items start with `visible = false` (scale set to zero). `showItem` / `showLayer` set scale to 1 and optionally applies a brief drop-in tween (Y offset → 0, duration 80ms — skip if speed > 3x).

### 3.6 — Phase 1 Animator (`phase1Animator.js`)

**State machine:**
```
IDLE → CONTAINER_INTRO → ANIMATING_BOXES | ANIMATING_LAYERS → CONTAINER_DONE → [next container | PHASE1_DONE]
```

- `CONTAINER_INTRO`: camera flies to pallet, wireframe fades in (500ms). Show pallet index in stats panel.
- `ANIMATING_BOXES` / `ANIMATING_LAYERS`: driven by `requestAnimationFrame`. On each frame, advance by `Math.ceil(speed * BASE_ITEMS_PER_FRAME)` frames. `BASE_ITEMS_PER_FRAME = 1`.
- After each frame, update Layer Manifest Panel (highlight active layer row).
- `CONTAINER_DONE`: pause 800ms, then transition.

**Speed interpretation:**
| Speed slider value | Items per frame |
|---|---|
| 0.25x | 1 item every 4 frames |
| 1x | 1 item per frame |
| 2x | 2 items per frame |
| 5x | 5 items per frame |
| 10x | 10 items per frame |

At 10x, the drop-in tween is skipped for performance.

### 3.7 — Phase Transition

When Phase 1 completes (all containers done):
1. Display a full-width overlay banner: **"PHASE 1 COMPLETE — GENETIC ALGORITHM BEGINNING"** with a 2-second hold.
2. Slide the Phase 2 right panel in from the right.
3. Reset Three.js scene to the Phase 2 first-generation best packing.
4. If `phase2Frames` is empty (Phase 1 packed all items), display "All items packed in Phase 1 — GA not required" in the Phase 2 panel and skip Phase 2 animation.
5. Otherwise, begin Phase 2 animation.

The Phase nav buttons (`< Phase 1` / `Phase 2 >`) in the controls allow the user to jump back to Phase 1 or forward to Phase 2 at any time. Jumping backward resets the Three.js scene and restarts Phase 1 animation from the beginning (replays from frame 0 — no backward scrubbing through individual frames).

### 3.8 — Phase 2 Animator (`phase2Animator.js`)

Two simultaneous animations driven by the same `requestAnimationFrame` loop:

**Left: Three.js packing view**
- Driven by `phase2Frames` (one frame = one recorded generation)
- Each frame: call `renderer.clearScene()` then replay all items from `best_containers` snapshot
- Phase 1 items (permanent base) are rendered first using the final Phase 1 scene state; Phase 2 items from the snapshot are overlaid
- Camera stays focused on the first container

**Right: Fitness curve (Chart.js)**
- Initialize a `Chart` with type `"line"`, x-axis = generation, y-axis = `best_avg_utilization` (0–1)
- On each frame, push `{ x: generation, y: best_avg_utilization }` to the dataset and call `chart.update('none')` (no animation — the animation is handled by frame pacing)
- Show current generation, best container count, and best avg utilization as text below the chart
- Y-axis label: "Avg Utilization"; annotate the target line at 0.5 (50% = paper's benchmark baseline)

**Speed:** same slider as Phase 1 — controls generations per frame.

### 3.9 — Layer Manifest Panel (`layerPanel.js`)

A scrollable panel in the left sidebar. Contents:
- Header: "Container N — Layer Breakdown"
- One row per layer: `Layer N (z: Z_MIN–Z_MAX mm) — K items`
  - Expandable: click to reveal `item_type_summary` (e.g., "Type 3 × 12, Type 7 × 4")
- Active layer row is highlighted in yellow during animation
- In layer mode, hovering a row calls `renderer.highlightLayer(layerIndex)` to dim other layers in the 3D view

### 3.10 — Controls (`controls.js`)

```
[◀ Phase 1]  [⏮ Restart]  [⏪]  [⏸ Pause / ▶ Play]  [⏩]  [Phase 2 ▶]

Speed: [━━━━●━━━━] 1.0x

Container: [●○○○] 1 / 4
```

- `⏸ / ▶`: toggle `animator.paused`
- `⏪ / ⏩`: step back/forward by one container (Phase 1) or one generation (Phase 2). Stepping backward replays from the beginning up to the target container/generation index — the scene is rebuilt from the precomputed frames.
- Speed slider: range 0.25–10, logarithmic scale, updates `animator.speed`
- Container dots: one dot per Phase 1 container, filled as each completes
- Phase nav buttons: jump between Phase 1 and Phase 2 (described in 3.7)

### 3.11 — Stats Panel

Displayed in the left sidebar below controls:

| Stat | Source |
|---|---|
| Dataset | selected filename |
| Total Containers | `metadata.container_count` |
| Phase 1 Items | `metadata.phase1_item_count` |
| Phase 2 Items | `metadata.phase2_item_count` |
| Avg Utilization | `metadata.avg_utilization` formatted as % |
| GA Generations | `metadata.ga_generations_recorded` |
| Current Container Util % | computed client-side as `sum(dx*dy*dz) / (L*W*H)` |

---

## Component 4: Performance Strategy

### 4.1 — Instanced Rendering

All box geometry must use `THREE.InstancedMesh`. Do **not** use individual `THREE.Mesh` objects per item — this will cause severe frame drops on BR7 (20 box types, potentially hundreds of items per container).

Implementation:
- On `renderer.init(container, palletDims)`, compute the maximum item count per `item_type_index` across the current container's items, then create one `InstancedMesh` per type with that count as capacity.
- Set all instance scales to 0 initially (effectively invisible without modifying draw calls).
- `renderer.showItem(item, ...)` sets the instance matrix's scale to `(dx, dz, dy)` (Three.js Y-up) and translation to the computed center position.

### 4.2 — Auto-Detection Threshold

In `worker.js`, compute `totalPhase1Items = sum of phase-1 items across all containers`.

| totalPhase1Items | animationMode | BASE_ITEMS_PER_FRAME |
|---|---|---|
| ≤ 200 | `"box"` | 1 |
| 201–1000 | `"box"` | 2 |
| > 1000 | `"layer"` | — (whole layer per frame) |

These three constants (`THRESHOLD_BOX_FAST`, `THRESHOLD_LAYER`, `BASE_ITEMS_PER_FRAME`) are defined once at the top of `worker.js`. They must not be duplicated elsewhere.

### 4.3 — GA History Throttling (C++ side)

`GA_HISTORY_INTERVAL = 5` means record every 5th generation. For a 200-generation run this produces 40 snapshots. If each snapshot is ~50 items × ~150 bytes/item, that is ~300 KB — acceptable.

If the GA runs fewer than 20 generations, record every generation (interval = 1).

Add this logic to `NSGA2::run` (only active when `--animated-output` is set):
```cpp
int interval = (maxGenerations < 20) ? 1 : GA_HISTORY_INTERVAL;
if (gen % interval == 0 || gen == maxGenerations - 1) {
    recordSnapshot(gen, bestIndividual, cachedSolution);
    // cachedSolution is the PackingSolution already computed by evaluateFitness
    // for the best individual — no re-decode needed.
}
```

### 4.4 — JSON Size Estimate

For BR7 (worst case, ~20 box types, large problem):
- Phase 1 items: estimate ~500 items × 150 bytes = 75 KB
- GA history: 40 snapshots × 50 items × 150 bytes = 300 KB
- Total: ~375 KB — well within acceptable range for a local file read.

---

## Implementation Order

Execute in this order. Each task is independently testable.

### Task 1: C++ — Add `--animated-output` flag and `AnimatedSolution` struct
- Add `include/AnimatedSolution.h` with the struct definition (using existing C++ conventions: Rule of Zero, default member initializers, AoS layout)
- Add `--animated-output` flag parsing to `src/main.cpp` (scan `argv` for the literal string `"--animated-output"` after the positional args — no-op initially)
- Add BR format detection to `src/main.cpp`: if input path ends in `.txt`, use `BRReader` to parse instance 1; otherwise use `CSVReader` as today
- **Test:** binary still runs without the flag without any change in behavior; binary accepts `.txt` path and produces valid output

### Task 2: C++ — Instrument Phase 1 packing (placement_order + layer_index)
- After `BlockBuilder::buildBlocks` completes (under `--animated-output` only), walk each container's Phase 1 items, assign `placement_order` and `layer_index` per §1.3
- Populate `AnimatedSolution.containers` with the extended data
- **Test:** run with `--animated-output`, print `placement_order` and `layer_index` for first 10 items of container 0

### Task 3: C++ — Instrument Phase 2 items (placement_order, phase tag)
- After `Packer::decode` (under `--animated-output` only), identify newly added Phase 2 items (those not yet tagged), assign `phase = 2`, `layer_index = -1`, and `placement_order` continuing from each container's Phase 1 count
- **Test:** run with `--animated-output`, verify Phase 2 items have `phase=2`, `layer_index=-1`, and correct `placement_order`

### Task 4: C++ — Instrument Phase 2 GA (ga_history)
- Add optional `PackingSolution*` out-parameter to `evaluateFitness` (null in normal mode)
- Modify `NSGA2::run` to cache the best individual's solution per generation and record snapshots per §4.3
- **Test:** run with `--animated-output`, print `ga_history.size()` and `ga_history[0].best_avg_utilization`

### Task 5: C++ — JSONWriter extension
- Implement `JSONWriter::writeAnimatedSolution(...)` to serialize `AnimatedSolution` to `output/solution_animated.json`
- **Test:** run full algorithm with `--animated-output`, validate JSON schema matches §1.1 by hand inspection; verify `output/solution.json` is unchanged

### Task 6: Server — `server/server.py`
- Download Three.js r170 and Chart.js 4.x into `visualization/js/lib/` first
- Implement all endpoints per §2.2
- **Test:** `curl http://localhost:5000/datasets` returns grouped list; `curl -X POST /run -d '{"dataset":"data/br_benchmark/thpack1.txt"}'` returns valid JSON

### Task 7: UI — Static scaffolding (`index.html`, `css/styles.css`)
- Implement the layout from §3.2 with placeholder panels
- No animation logic yet — just structure and styling
- **Test:** load in browser, layout matches spec, no console errors

### Task 8: UI — Dataset selector and Run button (`app.js`)
- Fetch `/datasets`, populate grouped `<select>`, wire Run button to POST `/run`
- On success, log JSON to console
- **Test:** select BR1, click Run, confirm JSON logged with correct schema

### Task 9: UI — Web Worker (`worker.js`)
- Implement frame pre-computation per §3.4
- **Test:** post a sample solution, confirm `READY` message with correct `animationMode` and frame counts

### Task 10: UI — Three.js renderer (`renderer.js`)
- Implement `InstancedMesh` setup, `showItem`, `showLayer`, `clearScene` per §3.5
- **Test:** manually call `renderer.showItem` on 10 items, confirm correct positions in 3D view

### Task 11: UI — Phase 1 animator (`phase1Animator.js`)
- Implement the state machine and `requestAnimationFrame` loop per §3.6
- **Test:** run BR1, confirm boxes animate sequentially, layer manifest updates correctly

### Task 12: UI — Phase transition and controls (`controls.js`)
- Implement transition banner, panel slide-in, phase nav buttons, speed slider per §3.7 and §3.10
- **Test:** let Phase 1 complete, confirm banner appears, Phase 2 panel slides in

### Task 13: UI — Phase 2 animator (`phase2Animator.js`)
- Implement fitness chart and packing update per §3.8
- **Test:** run any dataset, confirm chart grows generation-by-generation and packing updates match fitness trend

### Task 14: UI — Layer manifest panel (`layerPanel.js`)
- Implement per §3.9
- **Test:** run in layer mode (dataset with >1000 items), hover a layer row, confirm 3D view dims other layers

### Task 15: End-to-end test with all datasets
- Run each BR benchmark (BR1–BR7) and both author datasets
- Confirm no lag, no console errors, correct phase separation
- Verify animation mode switches to `"layer"` for large datasets
- **Critical check:** run the existing benchmark suite (without `--animated-output`) and confirm results are identical to Phase 8 results — avg util ~49.5–50%, max util ~70–79%, zero AABB/bounds violations

---

## File Summary

| File | Action |
|---|---|
| `include/AnimatedSolution.h` | CREATE |
| `src/JSONWriter.cpp` / `include/JSONWriter.h` | MODIFY — add `writeAnimatedSolution` |
| `src/Packer.cpp` | MODIFY — placement_order, layer_index, phase tag (under `--animated-output` only) |
| `src/NSGA2.cpp` / `include/NSGA2.h` | MODIFY — ga_history snapshots (under `--animated-output` only) |
| `src/main.cpp` | MODIFY — `--animated-output` flag, BR format detection |
| `server/server.py` | CREATE |
| `server/requirements.txt` | CREATE — `flask` |
| `server/start.sh` | CREATE |
| `visualization/index.html` | REWRITE |
| `visualization/css/styles.css` | CREATE |
| `visualization/js/app.js` | CREATE |
| `visualization/js/animator.js` | CREATE |
| `visualization/js/renderer.js` | CREATE |
| `visualization/js/phase1Animator.js` | CREATE |
| `visualization/js/phase2Animator.js` | CREATE |
| `visualization/js/layerPanel.js` | CREATE |
| `visualization/js/controls.js` | CREATE |
| `visualization/js/worker.js` | CREATE |
| `visualization/js/lib/` | CREATE — Three.js r170 + Chart.js 4.x (manual download) |

---

## Constraints and Non-Negotiables

1. The existing `output/solution.json` must not be modified or removed. The new output goes to `output/solution_animated.json` only when `--animated-output` is passed.
2. The `--animated-output` flag must gate ALL new C++ instrumentation. Without this flag, the binary must behave bit-for-bit identically to Phase 8 — no change to placement decisions, no change to benchmark results.
3. Never use individual `THREE.Mesh` per item. Always use `THREE.InstancedMesh` grouped by `item_type_index`.
4. JSON frame pre-computation happens in `worker.js`. The main thread may parse JSON (unavoidable with `Response.json()`), but no CPU-heavy frame building runs on the main thread.
5. `requestAnimationFrame` drives the animation loop. `setInterval` must not be used anywhere in the animation pipeline.
6. `BASE_ITEMS_PER_FRAME` and the auto-detection thresholds are constants defined once in `worker.js`. They must not be duplicated elsewhere.
7. The drop-in tween (80ms box appearance animation) is skipped entirely when speed > 3x.
8. The server must reject any `dataset` path that resolves outside the project's `data/` directory.
9. Three.js, Chart.js, and any other JS libraries must be loaded from local files under `visualization/js/lib/` — not from CDNs — to allow fully offline use.
10. Phase 2 items in the JSON have `layer_index: null` and `phase: 2`. The UI must handle null `layer_index` gracefully (no crash, no layer panel entry).
11. If `ga_history` is empty (Phase 1 packed everything), the UI must handle this gracefully: skip Phase 2 animation and display an informational message.
