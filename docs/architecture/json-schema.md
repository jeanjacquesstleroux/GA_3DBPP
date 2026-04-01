# GA_3DBPP JSON Output Schema

This document defines the structure of the `.json` files produced by `writeJSON()` in `src/JSONWriter.cpp`.
These files are consumed by the Three.js visualiser in `visualization/index.html`.

---

## Top-level structure

```json
{
  "metadata":   { ... },
  "containers": [ ... ]
}
```

---

## `metadata` object

| Key | Type | Description |
|-----|------|-------------|
| `container_count` | `int` | Number of pallets used in this solution |
| `avg_utilization` | `double` | Mean volume utilisation across all containers (0.0–1.0) |

Example:
```json
"metadata": {
  "container_count": 2,
  "avg_utilization": 0.7843
}
```

---

## `containers` array

Each element describes one pallet.

| Key | Type | Description |
|-----|------|-------------|
| `id` | `int` | Zero-based container index |
| `dims` | object | Container dimensions (see below) |
| `utilization` | `double` | Volume utilisation of this container (0.0–1.0) |
| `items` | array | Placed items on this pallet (see below) |

### `dims` object

| Key | Type | Description |
|-----|------|-------------|
| `L` | `int` | Pallet length (mm), X-axis |
| `W` | `int` | Pallet width (mm), Y-axis |
| `H` | `int` | Pallet height limit (mm), Z-axis |

Example:
```json
{
  "id": 0,
  "dims": { "L": 1200, "W": 800, "H": 1400 },
  "utilization": 0.812,
  "items": [ ... ]
}
```

---

## `items` array (per container)

Each element describes one physically placed box.

| Key | Type | Description |
|-----|------|-------------|
| `item_type_index` | `int` | Index into the original `ItemType` list (used for colour-coding in the viewer) |
| `orientation` | `string` | `"Original"` or `"Rotated90"` (90° Z-axis rotation swaps l and w) |
| `x` | `int` | Near-corner X position (mm) |
| `y` | `int` | Near-corner Y position (mm) |
| `z` | `int` | Near-corner Z position (mm) — base of the box |
| `dx` | `int` | Effective length after rotation (mm) — extent along X |
| `dy` | `int` | Effective width after rotation (mm) — extent along Y |
| `dz` | `int` | Effective height (mm) — extent along Z (unchanged by rotation) |
| `orig_l` | `int` | Original unrotated length from `ItemType` — used for tooltips |
| `orig_w` | `int` | Original unrotated width from `ItemType` — used for tooltips |
| `orig_h` | `int` | Original unrotated height from `ItemType` — used for tooltips |

The far corner of a placed item is `(x+dx, y+dy, z+dz)`.

Example:
```json
{
  "item_type_index": 2,
  "orientation": "Rotated90",
  "x": 0, "y": 400, "z": 200,
  "dx": 300, "dy": 400, "dz": 200,
  "orig_l": 400, "orig_w": 300, "orig_h": 200
}
```

---

## Coordinate system

```
Z (up)
│
│    Y (width)
│   /
│  /
│ /
└──────── X (length)
```

- Origin `(0, 0, 0)` is the left-bottom-front corner of the pallet.
- `x` runs along the pallet length (0 → L).
- `y` runs along the pallet width (0 → W).
- `z` runs upward (0 → H).
- Items are placed with their near corner at `(x, y, z)` and extend to `(x+dx, y+dy, z+dz)`.

---

## Three.js viewer usage

The viewer reads this file via the JSON loader (`<input type="file">` or drag-and-drop) and:

1. Creates one `THREE.BoxGeometry(dx, dz, dy)` per item.
   - Note: Three.js uses Y-up; the viewer maps the packing Z-axis to Three.js Y and packing Y to Three.js Z.
2. Positions each mesh at `(x + dx/2, z + dz/2, y + dy/2)` (center of the box).
3. Colours each mesh by `item_type_index` using a fixed palette.
4. Displays `orig_l × orig_w × orig_h` in the tooltip on hover.
5. Uses `container_count` and `avg_utilization` from `metadata` for the statistics panel.

---

## Full example (single container, two items)

```json
{
  "metadata": {
    "container_count": 1,
    "avg_utilization": 0.25
  },
  "containers": [
    {
      "id": 0,
      "dims": { "L": 1200, "W": 800, "H": 1400 },
      "utilization": 0.25,
      "items": [
        {
          "item_type_index": 0,
          "orientation": "Original",
          "x": 0, "y": 0, "z": 0,
          "dx": 400, "dy": 300, "dz": 200,
          "orig_l": 400, "orig_w": 300, "orig_h": 200
        },
        {
          "item_type_index": 1,
          "orientation": "Rotated90",
          "x": 400, "y": 0, "z": 0,
          "dx": 300, "dy": 400, "dz": 150,
          "orig_l": 400, "orig_w": 300, "orig_h": 150
        }
      ]
    }
  ]
}
```
