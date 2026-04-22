#!/usr/bin/env python3
"""
B2 / B5 E2E tests: run the binary on valid_small.csv, validate the animated
JSON output for schema correctness, placement_order integrity, layer_index
consistency, and avg_utilization math.

Run from the repository root:
    python3 test/test_json_schema.py
"""

import json
import pathlib
import subprocess
import sys

PROJECT = pathlib.Path(__file__).resolve().parent.parent
BINARY  = PROJECT / "build" / "release" / "GA_3DBPP"
CSV     = PROJECT / "data" / "test_fixtures" / "valid_small.csv"
OUT_DIR = PROJECT / "output"
ANIM    = OUT_DIR / "solution_animated.json"

PASS = 0
FAIL = 0

def ok(name):
    global PASS
    PASS += 1
    print(f"  PASS  {name}")

def fail(name, detail=""):
    global FAIL
    FAIL += 1
    msg = f"  FAIL  {name}"
    if detail:
        msg += f"\n        {detail}"
    print(msg)

# ── Run binary ────────────────────────────────────────────────────────────────
print("Running binary on valid_small.csv …")
OUT_DIR.mkdir(exist_ok=True)
if ANIM.exists():
    ANIM.unlink()

result = subprocess.run(
    [str(BINARY), str(CSV), str(ANIM), "--animated-output"],
    cwd=str(PROJECT),
    capture_output=True, timeout=60,
)
if result.returncode != 0:
    print(f"FATAL: binary exited {result.returncode}\n{result.stderr.decode('utf-8','replace')}")
    sys.exit(1)
if not ANIM.exists():
    print("FATAL: output file not created")
    sys.exit(1)

with open(ANIM) as f:
    sol = json.load(f)

print("JSON loaded. Running schema checks …\n")

# ── B2-1: Required top-level fields ──────────────────────────────────────────
for field in ("metadata", "containers", "ga_history"):
    if field in sol:
        ok(f"B2-1 top-level field '{field}' present")
    else:
        fail(f"B2-1 top-level field '{field}' missing")

# ── B2-2: Required metadata fields ───────────────────────────────────────────
md = sol.get("metadata", {})
for field in ("container_count", "avg_utilization", "total_items",
              "phase1_item_count", "phase2_item_count", "ga_generations_recorded"):
    if field in md:
        ok(f"B2-2 metadata.{field} present")
    else:
        fail(f"B2-2 metadata.{field} missing")

# ── B2-3: container_count matches containers array length ─────────────────────
n = len(sol.get("containers", []))
if md.get("container_count") == n:
    ok("B2-3 container_count matches containers array length")
else:
    fail("B2-3 container_count mismatch",
         f"metadata={md.get('container_count')} array={n}")

# ── B2-4: total_items == phase1 + phase2 ─────────────────────────────────────
ti = md.get("total_items", -1)
p1 = md.get("phase1_item_count", -1)
p2 = md.get("phase2_item_count", -1)
if ti == p1 + p2:
    ok("B2-4 total_items == phase1_item_count + phase2_item_count")
else:
    fail("B2-4 total_items mismatch", f"total={ti} p1={p1} p2={p2}")

# ── B2-5: Every container has required fields ─────────────────────────────────
for ci, cont in enumerate(sol.get("containers", [])):
    for field in ("id", "dims", "items"):
        if field not in cont:
            fail(f"B2-5 container[{ci}] missing field '{field}'")
            break
    else:
        ok(f"B2-5 container[{ci}] has required fields")
    dims = cont.get("dims", {})
    for d in ("L", "W", "H"):
        if d not in dims:
            fail(f"B2-5 container[{ci}].dims missing '{d}'")

# ── B2-6: Every item has required fields ─────────────────────────────────────
item_field_failures = 0
for ci, cont in enumerate(sol.get("containers", [])):
    for ii, item in enumerate(cont.get("items", [])):
        for field in ("item_type_index", "orientation", "x", "y", "z",
                      "dx", "dy", "dz", "orig_l", "orig_w", "orig_h",
                      "phase", "placement_order", "layer_index"):
            if field not in item:
                item_field_failures += 1
if item_field_failures == 0:
    ok("B2-6 all items have required fields")
else:
    fail("B2-6 items missing required fields", f"{item_field_failures} occurrences")

# ── B2-7: orientation values are "Original" or "Rotated90" ───────────────────
bad_ori = 0
for cont in sol.get("containers", []):
    for item in cont.get("items", []):
        if item.get("orientation") not in ("Original", "Rotated90"):
            bad_ori += 1
if bad_ori == 0:
    ok("B2-7 all orientations are 'Original' or 'Rotated90'")
else:
    fail("B2-7 unexpected orientation values", f"{bad_ori} items")

# ── B2-8: dz == orig_h for all items (Z-rotation only swaps l and w) ─────────
bad_dz = 0
for cont in sol.get("containers", []):
    for item in cont.get("items", []):
        if item.get("dz") != item.get("orig_h"):
            bad_dz += 1
if bad_dz == 0:
    ok("B2-8 dz == orig_h for all items (height unchanged by rotation)")
else:
    fail("B2-8 dz != orig_h", f"{bad_dz} items")

# ── B5-1: placement_order is contiguous (0-based) within each container ───────
po_fail = 0
for ci, cont in enumerate(sol.get("containers", [])):
    orders = sorted(item["placement_order"] for item in cont.get("items", []))
    expected = list(range(len(orders)))
    if orders != expected:
        po_fail += 1
        fail(f"B5-1 container[{ci}] placement_order not contiguous",
             f"got {orders[:10]}{'...' if len(orders)>10 else ''}")
if po_fail == 0:
    ok("B5-1 placement_order is contiguous (0-based) in all containers")

# ── B5-2: Phase 1 items have non-negative layer_index ────────────────────────
bad_li = 0
for cont in sol.get("containers", []):
    manifest_indices = {lme["layer_index"] for lme in cont.get("layer_manifest", [])}
    for item in cont.get("items", []):
        if item.get("phase") == 1:
            li = item.get("layer_index")
            if li is None or li < 0:
                bad_li += 1
            elif manifest_indices and li not in manifest_indices:
                bad_li += 1
if bad_li == 0:
    ok("B5-2 all phase-1 items have a valid layer_index in layer_manifest")
else:
    fail("B5-2 phase-1 items with invalid layer_index", f"{bad_li} items")

# ── B5-3: Phase 2 items have layer_index == null ─────────────────────────────
bad_p2 = 0
for cont in sol.get("containers", []):
    for item in cont.get("items", []):
        if item.get("phase") == 2 and item.get("layer_index") is not None:
            bad_p2 += 1
if bad_p2 == 0:
    ok("B5-3 all phase-2 items have layer_index == null")
else:
    fail("B5-3 phase-2 items with non-null layer_index", f"{bad_p2} items")

# ── B5-4: avg_utilization matches manual computation ─────────────────────────
# Manually compute: sum(item_volumes) / sum(container_volumes)
# This verifies the C-1 fix (no extra /n division).
total_item_vol = 0.0
total_cont_vol = 0.0
for cont in sol.get("containers", []):
    dims = cont.get("dims", {})
    L, W, H = dims.get("L", 0), dims.get("W", 0), dims.get("H", 0)
    total_cont_vol += L * W * H
    for item in cont.get("items", []):
        total_item_vol += item["dx"] * item["dy"] * item["dz"]

if total_cont_vol > 0:
    expected_avg = total_item_vol / total_cont_vol
    actual_avg   = md.get("avg_utilization", -1.0)
    if abs(actual_avg - expected_avg) < 1e-9:
        ok(f"B5-4 avg_utilization correct ({actual_avg:.6f})")
    else:
        fail("B5-4 avg_utilization wrong",
             f"expected={expected_avg:.6f} got={actual_avg:.6f}")

# ── B5-5: No item has negative coords or zero/negative dimensions ─────────────
bad_geom = 0
for cont in sol.get("containers", []):
    for item in cont.get("items", []):
        if item["x"] < 0 or item["y"] < 0 or item["z"] < 0:
            bad_geom += 1
        if item["dx"] <= 0 or item["dy"] <= 0 or item["dz"] <= 0:
            bad_geom += 1
if bad_geom == 0:
    ok("B5-5 all items have non-negative positions and positive dimensions")
else:
    fail("B5-5 geometry violations", f"{bad_geom} items")

# ── B5-6: No item extends outside its container ───────────────────────────────
oob = 0
for cont in sol.get("containers", []):
    dims = cont.get("dims", {})
    L, W, H = dims.get("L",0), dims.get("W",0), dims.get("H",0)
    for item in cont.get("items", []):
        if item["x"]+item["dx"] > L or item["y"]+item["dy"] > W or item["z"]+item["dz"] > H:
            oob += 1
if oob == 0:
    ok("B5-6 all items within container bounds")
else:
    fail("B5-6 items outside container bounds", f"{oob} items")

# ── B5-7: No two items in the same container overlap ─────────────────────────
def overlaps(a, b):
    return (a["x"] < b["x"]+b["dx"] and b["x"] < a["x"]+a["dx"] and
            a["y"] < b["y"]+b["dy"] and b["y"] < a["y"]+a["dy"] and
            a["z"] < b["z"]+b["dz"] and b["z"] < a["z"]+a["dz"])

overlap_count = 0
for cont in sol.get("containers", []):
    items = cont.get("items", [])
    for i in range(len(items)):
        for j in range(i+1, len(items)):
            if overlaps(items[i], items[j]):
                overlap_count += 1
if overlap_count == 0:
    ok("B5-7 no overlapping items in any container")
else:
    fail("B5-7 overlapping items detected", f"{overlap_count} pairs")

# ── Summary ───────────────────────────────────────────────────────────────────
print(f"\n{'='*50}")
print(f"Results: {PASS} passed, {FAIL} failed")
sys.exit(0 if FAIL == 0 else 1)
