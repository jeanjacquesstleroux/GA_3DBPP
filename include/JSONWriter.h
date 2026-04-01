#pragma once

#include <string>
#include <vector>
#include "Types.h"

// Writes a PackingSolution to a JSON file for the Three.js visualiser.
//
// Output schema (top-level keys):
//   "metadata"   — { "container_count": int, "avg_utilization": double }
//   "containers" — array of container objects, each with:
//       "id"          : int
//       "dims"        : { "L": int, "W": int, "H": int }
//       "utilization" : double
//       "items"       : array of placed-item objects, each with:
//           "item_type_index" : int
//           "orientation"     : "Original" | "Rotated90"
//           "x", "y", "z"     : int   (near corner, mm)
//           "dx", "dy", "dz"  : int   (effective dims after rotation, mm)
//
// The item_types array is passed in so that the writer can embed the original
// unrotated dimensions alongside the placed dims — useful for the viewer's tooltip.
//
// Returns true on success, false if the file cannot be opened.
[[nodiscard]] bool writeJSON(const PackingSolution&       solution,
                             const std::vector<ItemType>& item_types,
                             const std::string&           path);
