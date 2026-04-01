# Phase 2: File I/O
**Environment:** Ubuntu 24.04, GCC 13.3.0, C++20, CMake 3.28.3 + Ninja, vcpkg (x64-linux)

---

## 1. Introduction

### 1.1 What This Phase Accomplishes

Phase 1 defined the vocabulary of the program — every struct and enum the algorithm operates on. At the end of Phase 1, those types existed but could not be loaded from disk or written back out. The program had no connection to the real world.

Phase 2 adds the I/O layer. By the end of this phase the program can:

- **Read industrial order data** from a CSV file into a `std::map` of `ItemType` vectors, with per-row validation.
- **Read BR benchmark instances** from the OR-Library thpack format into `BRProblem` structs for algorithm testing.
- **Write packing solutions** to a structured JSON file that the Three.js visualiser can load directly.
- **Log messages** at configurable severity levels to both the terminal (stderr, with colour) and a persistent log file on disk.

### 1.2 Design Decisions

**`CSVReader` as a class, not a free function.** The reader is structured as a class with an `explicit` constructor that takes the file path, and a `read()` method that performs the actual parsing. This separates *configuration* (which file) from *action* (reading it), which is the fundamental principle of encapsulation. A caller writes `CSVReader("orders.csv").read()`, making intent clear at every call site.

**Manual key assignment in `JSONWriter`, not `to_json` overloads.** The nlohmann/json library supports defining `void to_json(nlohmann::json& j, const T& t)` free functions for automatic serialisation. We deliberately chose manual key assignment instead:

```cpp
// Our approach — manual, explicit
jitem["x"]  = p.x;
jitem["dx"] = p.dx;

// Alternative — to_json overload
void to_json(nlohmann::json& j, const PlacedItem& p) {
    j = nlohmann::json{{"x", p.x}, {"dx", p.dx}, ...};
}
```

For a production API that serialises the same type in many places, `to_json` overloads reduce repetition. For this project, the JSON output is written in exactly one place (`JSONWriter.cpp`). Manual assignment makes every field mapping immediately visible at that location without requiring the reader to look up a separate overload definition. Clarity outweighs idiom here.

**`Logger` with dual file/stderr sinks.** A single `spdlog::logger` is wired to two sinks simultaneously: a coloured stderr sink for interactive feedback during a run, and a plain-text file sink so output is preserved for post-run review. The level can be set independently per sink, and the logger is initialized once through `initLogger()` then retrieved anywhere via `getLogger()`.

### 1.3 Files Created in This Phase

| File | Purpose |
|---|---|
| `include/CSVReader.h` | `CSVReader` class declaration |
| `src/CSVReader.cpp` | CSV parsing with per-row validation |
| `include/BRReader.h` | `BRProblem` struct + `loadBRFile()` declaration |
| `src/BRReader.cpp` | BR benchmark thpack format parser |
| `include/JSONWriter.h` | `writeJSON()` declaration + JSON schema comment |
| `src/JSONWriter.cpp` | nlohmann/json serialisation of `PackingSolution` |
| `include/Logger.h` | `initLogger()` / `getLogger()` declarations |
| `src/Logger.cpp` | spdlog dual-sink (stderr + file) logger |
| `test/test_io.cpp` | BRReader + JSONWriter tests |
| `test/test_csv.cpp` | CSVReader unit + validation tests |
| `data/test_fixtures/valid_small.csv` | Small valid CSV for unit tests |
| `data/test_fixtures/malformed_dims.csv` | CSV with zero/negative dimension rows |
| `data/test_fixtures/missing_cols.csv` | CSV with a row that has fewer than 7 fields |
| `data/test_fixtures/empty_data.csv` | CSV with only the header row |
| `docs/misc/json-schema.md` | Full JSON output schema reference |

### 1.4 Prerequisites

- Phase 1 complete (Tasks 1.1–1.9): `Types.h`, `Config.h`, and `test_types.cpp` all in place.
- vcpkg packages installed: `nlohmann-json`, `gtest`, `spdlog`.
- `data/Dataset10000.csv` present (industrial order dataset).
- `data/br_benchmark/thpack1.txt` through `thpack7.txt` present.

---

## Step 1: Implement `CSVReader` — header

The header declares a class with two public members: an `explicit` constructor and a `const` `read()` method. The `explicit` keyword prevents the compiler from silently constructing a `CSVReader` anywhere a `std::string` is expected. The trailing underscore on `path_` is a convention for private member variables — it visually distinguishes the stored field from the constructor parameter.

```cpp
// include/CSVReader.h
#pragma once

#include <map>
#include <string>
#include <vector>
#include "Types.h"

// Parses an industrial order CSV file with columns:
//   Order, Product, Quantity, Length, Width, Height, Weight
//
// Validation rules (applied per row):
//   - Rows with fewer than 7 comma-separated fields are skipped.
//   - Rows where any dimension (l, w, h) or quantity is <= 0 are skipped.
//   - Rows where any numeric field is non-parseable are skipped.
//   All skipped rows emit a warning to stderr with the line number and reason.
//
// Weight (double kg) is rounded to the nearest integer and stored in ItemType::m.
class CSVReader {
public:
    explicit CSVReader(const std::string& path);
    [[nodiscard]] std::map<int, std::vector<ItemType>> read() const;
private:
    std::string path_;
};
```

**Key points:**
- `explicit` — prevents implicit construction. Without it, a function expecting a `CSVReader` could silently accept a bare `std::string`, hiding a bug.
- `read() const` — the method promises not to modify any member of the object. Since it only reads `path_`, this is correct and enforced by the compiler.
- `private: std::string path_` — callers cannot touch this field directly. The class controls its own state.

---

## Step 2: Implement `CSVReader` — parsing and validation

The constructor uses a **member initializer list** (`: path_(path)`) to initialize the member directly. The `read()` method opens the file, discards the header, then processes each data row. A `line_number` counter enables precise error messages. The validation pipeline has three gates:

1. **Field count** — `std::getline` into a fixed `std::string fields[7]` array, counting fills. Fewer than 7 → skip.
2. **Numeric conversion** — `std::stoi` / `std::stod` wrapped in `try/catch(const std::exception&)`. Non-parseable string → skip.
3. **Positive values** — `len <= 0 || wid <= 0 || hgt <= 0 || qty <= 0`. Any non-positive value → skip.

All skipped rows emit a descriptive message to `std::cerr` with the line number. These messages appear on stderr, not stdout, so they don't pollute program output.

```cpp
// src/CSVReader.cpp  (key sections)

CSVReader::CSVReader(const std::string& path) : path_(path) {}

std::map<int, std::vector<ItemType>> CSVReader::read() const {
    std::ifstream file(path_);
    if (!file.is_open()) return {};

    std::string line;
    std::getline(file, line);  // discard header

    int line_number = 1;
    while (std::getline(file, line)) {
        ++line_number;

        std::string fields[7];
        int parsed = 0;
        std::istringstream ss(line);
        while (parsed < 7 && std::getline(ss, fields[parsed], ',')) ++parsed;

        if (parsed < 7) {
            std::cerr << "[CSVReader] line " << line_number
                      << ": expected 7 fields, got " << parsed << " — skipping.\n";
            continue;
        }

        int orderID, qty, len, wid, hgt, mass;
        try {
            orderID = std::stoi(fields[0]);
            qty     = std::stoi(fields[2]);
            len     = std::stoi(fields[3]);
            wid     = std::stoi(fields[4]);
            hgt     = std::stoi(fields[5]);
            mass    = static_cast<int>(std::lround(std::stod(fields[6])));
        } catch (const std::exception&) {
            std::cerr << "[CSVReader] line " << line_number
                      << ": non-numeric field — skipping.\n";
            continue;
        }

        if (len <= 0 || wid <= 0 || hgt <= 0 || qty <= 0) {
            std::cerr << "[CSVReader] line " << line_number
                      << ": non-positive value — skipping.\n";
            continue;
        }

        ItemType item;
        item.l = len; item.w = wid; item.h = hgt; item.m = mass; item.q = qty;
        orders[orderID].push_back(item);
    }
    return orders;
}
```

**Key points:**
- `: path_(path)` — member initializer list. Initializes the member directly rather than default-constructing then assigning, which is both more efficient and more correct for types that lack a sensible default state.
- `std::string fields[7]` — a fixed-size stack array of 7 strings. Sized to the exact number of expected columns so the fill loop has a natural termination condition.
- `catch (const std::exception&)` — both `std::invalid_argument` (non-numeric) and `std::out_of_range` (overflow) inherit from `std::exception`, so one catch covers both failure modes.
- `static_cast<int>(std::lround(...))` — `lround` returns `long`; the cast narrows it to `int` to match `ItemType::m`.

**Validation in practice.** When run against `Dataset10000.csv`, the validator correctly identifies and skips 13 rows with zero dimensions. Without validation, these rows would produce `ItemType` objects with `l=0, w=0, h=0`, which would generate zero volume and corrupt all utilization calculations downstream.

---

## Step 3: Implement `BRReader`

The BR benchmark format (OR-Library, Bischoff & Ratcliff 1995) is a whitespace-delimited text file. The first integer is the total number of problems. Each problem begins with a two-integer header (problem index and order ID, both discarded), followed by three container dimensions, a count of item types, and then one item-type record per line.

Each item record has the format:
```
index  l  allow_l_rotation  w  allow_w_rotation  h  allow_h_rotation  quantity
```

We read `l`, `w`, `h`, and `quantity`; the rotation flags and index are read into throwaway variables.

```cpp
// include/BRReader.h
struct BRProblem {
    int L = 0, W = 0, H = 0;
    std::vector<ItemType> items;
};
[[nodiscard]] std::vector<BRProblem> loadBRFile(const std::string& path);
```

**Key points:**
- `BRProblem` is a standalone struct (not a class) because it is pure data with no invariants to enforce.
- `loadBRFile` is a free function, not a class method. The BR reader has no state to encapsulate between calls — each call is independent.
- The two-integer header per problem (`dummy1, dummy2`) is read and discarded using throwaway variables. This keeps the stream position correct without littering the data model with fields we never use.

---

## Step 4: Implement `JSONWriter`

`JSONWriter.cpp` serialises a `PackingSolution` into a JSON file for the Three.js visualiser. It uses nlohmann/json's `operator[]` approach: a `json` variable is treated like a nested dictionary, with keys assigned by subscript.

**Why not `to_json` overloads?** The nlohmann library supports defining `void to_json(nlohmann::json& j, const T& t)` free functions, which allow writing `j = myObject` directly. We chose not to use them for this project. The JSON output is produced in exactly one location (`JSONWriter.cpp`), so there is no repetition to eliminate. Manual assignment keeps every field mapping visible at that single location — a reader of `JSONWriter.cpp` sees the complete picture without needing to look up three separate overload definitions scattered across `Types.h`. For a library with many serialisation call sites, overloads would be the right choice; for a single-purpose writer, clarity wins.

```cpp
// src/JSONWriter.cpp  (key sections)

nlohmann::json root;
root["metadata"]["container_count"] = static_cast<int>(solution.containers.size());
root["metadata"]["avg_utilization"] = solution.avgUtilization();

root["containers"] = nlohmann::json::array();  // explicit [] prevents null on empty solution
for (int ci = 0; ...) {
    nlohmann::json jcont;
    jcont["dims"]["L"] = cont.L;
    // ...
    for (const PlacedItem& p : cont.items) {
        nlohmann::json jitem;
        jitem["orientation"] = (p.orientation == Orientation::Original)
                                   ? "Original" : "Rotated90";
        jitem["orig_l"] = item_types[p.item_type_index].l;
        // ...
        jcont["items"].push_back(jitem);
    }
    root["containers"].push_back(jcont);
}

std::ofstream out(path);
if (!out.is_open()) return false;
out << root.dump(2);
return out.good();
```

**Key points:**
- `root["containers"] = nlohmann::json::array()` — explicitly creates an empty array. Without this, an empty solution would write `"containers": null` rather than `"containers": []`, which would crash the Three.js viewer on iteration.
- `enum class` → string conversion — `Orientation` is an `enum class` and nlohmann has no built-in serialiser for user-defined enums. The ternary converts it to a human-readable string that the viewer can match on directly.
- `return out.good()` — checks stream error flags after writing. `is_open()` only confirms the file descriptor was created; a disk-full condition would set the `failbit` after writing begins. `good()` catches this.

**JSON schema.** The full schema is documented in `docs/misc/json-schema.md`. The top-level keys are `metadata` (container count, average utilisation) and `containers` (array of pallets, each with dims, utilisation, and an `items` array). Each item carries its near-corner position (`x, y, z`), post-rotation effective dimensions (`dx, dy, dz`), orientation string, item type index, and original unrotated dimensions (`orig_l, orig_w, orig_h`) for viewer tooltips.

---

## Step 5: Implement `Logger` — dual file/stderr sinks

The logger wraps spdlog with two sinks combined into a single named logger:

- **`ansicolor_stderr_sink_mt`** — writes coloured output to stderr. `warn` messages appear yellow, `error` red. The `mt` suffix means thread-safe (multi-threaded mutex protection).
- **`basic_file_sink_mt`** — writes plain text to a log file on disk, truncated at startup. The file sink uses a timestamp-prefixed pattern (`[YYYY-MM-DD HH:MM:SS.mmm]`) so the log is useful for post-run review even if the run spans multiple days.

Both sinks are combined by constructing the logger with a `spdlog::sinks_init_list`. A single `logger->set_level(level)` call controls the minimum severity for both sinks simultaneously.

```cpp
// src/Logger.cpp  (key sections)

auto stderr_sink = std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>();
stderr_sink->set_pattern("[%T.%e] [%^%l%$] %v");

auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, /*truncate=*/true);
file_sink->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");

auto logger = std::make_shared<spdlog::logger>(
    "GA_3DBPP",
    spdlog::sinks_init_list{stderr_sink, file_sink}
);
logger->set_level(level);
spdlog::register_logger(logger);
spdlog::set_default_logger(logger);
```

**Key points:**
- `std::make_shared<T>(...)` — constructs a heap-allocated `T` and wraps it in a `shared_ptr` in one step. Equivalent to `std::shared_ptr<T>(new T(...))` but exception-safe.
- `spdlog::sinks_init_list{...}` — an initializer list of `shared_ptr<sink>`. The logger holds a reference to each sink and dispatches every log call to all of them.
- `register_logger` + `set_default_logger` — `register_logger` makes the logger findable by name via `spdlog::get("GA_3DBPP")`. `set_default_logger` routes bare `spdlog::info(...)` calls (without going through `getLogger()`) to the same logger.
- The `static bool initialized` guard inside both `initLogger` and `getLogger` uses static local variables — initialized exactly once, the first time that line executes, and then persistent for the program's lifetime. The two variables are completely independent despite sharing a name: each is scoped to its own function and lives at a distinct memory address.

---

## Step 6: Write `test_csv.cpp` and update `test_io.cpp`

CSV tests are separated into `test_csv.cpp`. Tests in `test_io.cpp` cover only `BRReader` and `JSONWriter`. This split means that if CSV parsing changes, only `test_csv.cpp` needs attention.

### `test_csv.cpp` test cases

| Suite | Case | What it checks |
|---|---|---|
| `CSVReader` | `MissingFileReturnsEmpty` | Non-existent path → empty map |
| `CSVReader` | `HeaderOnlyReturnsEmpty` | File with only a header row → empty map |
| `CSVReader` | `ValidFileOrderCount` | 2 distinct order IDs parsed from `valid_small.csv` |
| `CSVReader` | `ValidFileOrder1001ItemCount` | Order 1001 has exactly 2 items |
| `CSVReader` | `ValidFileFirstItemFields` | First item has correct `l, w, h, q, m` values |
| `CSVReader` | `ValidFileOrder1002ItemCount` | Order 1002 has exactly 1 item |
| `CSVReader` | `SkipsZeroDimension` | `malformed_dims.csv`: 4 rows, 2 valid, 2 skipped (zero/negative dims) |
| `CSVReader` | `ValidRowsStillLoadedAfterBadRow` | Correct items survive after bad rows are skipped |
| `CSVReader` | `SkipsMissingColumns` | `missing_cols.csv`: 3 rows, 2 valid, 1 skipped (5 fields) |
| `CSVReader` | `LargeDatasetOrderCount` | `Dataset10000.csv`: 9433 orders after 13 zero-dim rows filtered |
| `CSVReader` | `LargeDatasetOrder95125Fields` | Spot-check specific item fields in the full dataset |

### `test_io.cpp` test cases

| Suite | Case | What it checks |
|---|---|---|
| `BRReader` | `LoadsCorrectProblemCount` | `thpack1.txt` contains 100 problems |
| `BRReader` | `Problem1ContainerDimensions` | Container dims: 587×233×220 |
| `BRReader` | `Problem1ItemTypes` | 3 item types; first is 108×76×30, qty 40 |
| `BRReader` | `MissingFileReturnsEmpty` | Non-existent path → empty vector |
| `JSONWriter` | `WriteSucceeds` | `writeJSON` returns `true` for a valid solution |
| `JSONWriter` | `MissingDirectoryReturnsFalse` | Returns `false` when path is non-writable |
| `JSONWriter` | `RoundTripMetadata` | `container_count` field survives write → parse |
| `JSONWriter` | `RoundTripContainerDims` | `L, W, H` match `Config::PALLET_*` constants |
| `JSONWriter` | `RoundTripPlacedItem` | All item fields survive write → parse |

---

## Step 7: Update `CMakeLists.txt`

Three source files were added to both targets, and `test_csv.cpp` was added to the test target:

```cmake
add_executable(GA_3DBPP
    src/main.cpp
    src/BRReader.cpp
    src/CSVReader.cpp
    src/JSONWriter.cpp
    src/Logger.cpp       # added
)

add_executable(GA_3DBPPTests
    test/test_main.cpp
    test/test_types.cpp
    test/test_io.cpp
    test/test_csv.cpp    # added
    src/BRReader.cpp
    src/CSVReader.cpp
    src/JSONWriter.cpp
    src/Logger.cpp       # added
)
```

The `nlohmann_json::nlohmann_json` and `spdlog::spdlog` link targets were already present from Phase 0; no new `target_link_libraries` calls were needed.

---

## Final file structure added in Phase 2

```
include/
├── CSVReader.h      # CSVReader class (explicit ctor, read() const)
├── BRReader.h       # BRProblem struct + loadBRFile() free function
├── JSONWriter.h     # writeJSON() + JSON schema comment
└── Logger.h         # initLogger() / getLogger()

src/
├── CSVReader.cpp    # Parsing + 3-gate validation (field count, numeric, positive)
├── BRReader.cpp     # thpack whitespace-delimited format parser
├── JSONWriter.cpp   # nlohmann/json manual key assignment serialiser
└── Logger.cpp       # spdlog dual-sink: ansicolor_stderr_sink_mt + basic_file_sink_mt

test/
├── test_io.cpp      # BRReader + JSONWriter tests (9 cases)
└── test_csv.cpp     # CSVReader tests including validation (11 cases)

data/test_fixtures/
├── valid_small.csv      # 3 valid rows, 2 orders
├── malformed_dims.csv   # 4 rows: 2 valid, 2 with zero/negative dims
├── missing_cols.csv     # 3 rows: 2 valid, 1 with 5 fields
└── empty_data.csv       # Header only, no data rows

docs/misc/
└── json-schema.md   # Full JSON output schema reference
```

---

## What changes in Phase 3

Phase 3 adds the geometric constraint modules: AABB collision detection, support checking, centre-of-mass calculation, and Hausdorff interlocking distance. All of these operate on `PlacedItem` and `Container` objects — the types defined in Phase 1. Phase 2's I/O infrastructure is used to load test inputs and write outputs, but Phase 3 itself is pure geometry logic with no new file I/O.
