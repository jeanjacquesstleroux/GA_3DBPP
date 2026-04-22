# Code Review & E2E Test Plan

## Phase A: Static Code Review

- [x] A1. JSON schema: `json-schema.md` vs `JSONWriter.cpp` vs `AnimatedSolution.h` vs frontend readers
- [x] A2. Placement math: `LayerGenerator.cpp`, `BlockBuilder.cpp`, `ExtremePointEngine.cpp`, `SupportChecker.cpp`, `AABB.h`
- [x] A3. Coordinate system consistency: origin convention, `dx/dy/dz` vs `l/w/h`, `JSONWriter.cpp`
- [x] A4. Frontend coordinate mapping: `renderer.js`, `phase1Animator.js`, `phase2Animator.js`, `app.js`
- [x] A5. Stats panel math: utilization %, Phase 1/2 counts, avg util in `app.js`
- [x] A6. Residuals / item count integrity: `main.cpp`, `Packer.cpp`, `NSGA2.cpp`, `JSONWriter.cpp`
- [x] A7. Animation data integrity: `placement_order` uniqueness, `layer_index` vs `layer_manifest`
- [x] A8. GA selection: `fastNonDominatedSort`, `muLambdaSelect`, best-solution selection in `main.cpp`
- [x] A9. Existing tests gap analysis: all 12 test files mapped to components

## Phase B: E2E Tests

- [x] B1. Backend integration smoke tests — 6 new C++ tests in test_integration.cpp (placement math, utilization formula, no-negative-coords, avg-util vs manual)
- [x] B2. JSON schema E2E tests — test/test_json_schema.py (22 checks: required fields, orientation values, dz==orig_h, bounds, overlaps)
- [ ] B3. Frontend E2E tests — SKIPPED: Playwright not installed; API-level coverage provided by B2
- [x] B4. Coordinate mapping test — FarCornerWithinBounds C++ test + B2-8 (dz==orig_h) + B5-5/B5-6
- [x] B5. Animation sequence test — placement_order contiguity, layer_index validity, avg_util math (test_json_schema.py B5-1..B5-7)

## Phase C: Findings & Fixes

- [x] C1. Compile findings (Critical / Major / Minor)
- [x] C2. Propose fix list for approval
- [x] C3. Implement approved fixes

## Findings (Phase C — awaiting approval)

### CRITICAL
**C-1. `writeAnimatedJSON` avg_utilization divided by container count twice (JSONWriter.cpp:107–109)**
Formula: `total_used / total_cap / n_containers`
- `total_cap` is already the SUM of all n container volumes, so `total_used / total_cap` = correct avg util.
- The extra `/ n_containers` makes the displayed "Avg Util" stat n× too small whenever n > 1.
- For 3 containers each at 80% fill → displays 26.7% instead of 80%.
- The gen-util in the Phase 2 panel is correct (uses `PackingSolution::avgUtilization()` path); only `metadata.avg_utilization` in the animated JSON is wrong.
- Fix: remove `/ static_cast<double>(solution.containers.size())` from that expression.

### MINOR
**C-2. Dead-code condition in GA-snapshot phase assignment (main.cpp:407)**
`ap.phase = (order < static_cast<int>(c.items.size())) ? 1 : 2;`
`order` runs 0..c.items.size()-1 inside the loop; the condition is always true → phase always = 1.
No visible rendering impact (Phase 2 animator doesn't use the `phase` field on GA-snapshot items), but it is logically incorrect.
Fix: replace with `ap.phase = 2;` (these are GA history items, conceptually Phase 2).

**C-3. `selectBest` in test_integration.cpp lacks aux_max_util tiebreaker**
`main.cpp`'s `selectBest` has a third tiebreak on `aux_max_util`; `test_integration.cpp` does not.
Impact: tests may select a marginally different Pareto-front member in the degenerate case where objectives[0] and objectives[2] are equal. All existing assertions still pass either way.
Fix: sync the lambda in test_integration.cpp with main.cpp.

### INFORMATIONAL (no code change needed)
- `stat-cur-util` ("Pallet Util") includes Phase 2 items while Phase 1 is still animating — shows the final value early. Correct math, not misleading once you know the label means "final pallet utilization".
- `json-schema.md` documents only `writeJSON`, not the animated format. Documentation gap only.

## Status
COMPLETE. All phases done. 131 C++ tests pass, 22 Python schema tests pass.
