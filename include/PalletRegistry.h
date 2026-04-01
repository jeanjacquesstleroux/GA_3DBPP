#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Types.h"

// std::unordered_map requires a hash function for its key type.  enum classes
// have no default hash in the standard library, so we specialize std::hash<PalletID>
// here.  The implementation casts the enumerator to its underlying int and uses
// the existing std::hash<int>.  This must appear before the PalletRegistry namespace
// so the compiler can find it when instantiating the Registry alias below.
namespace std {
    template <>
    struct hash<PalletID> {
        size_t operator()(PalletID id) const noexcept {
            return std::hash<int>{}(static_cast<int>(id));
        }
    };
} // namespace std

// PalletRegistry — compile-once, look-up-anywhere catalog of known pallet specs.
//
// All dimensions are stored internally in millimeters regardless of native_unit.
// Conversion from inches to mm (×25.4, round to nearest mm) is done at population
// time so every caller gets consistent integer mm values.
//
// Usage:
//   const PalletSpec& spec = PalletRegistry::lookup(PalletID::EPAL_EUR_1);
//   LayerGenerator::generateFull(item, idx, spec.L, spec.W);
//
// Enum naming convention follows REGION_OPERATOR_WxL, e.g.:
//   PalletID::EPAL_EUR_1           — EPAL EUR 1, 800×1200 mm
//   PalletID::NA_CHEP_BLUE_48x40  — CHEP Blue Block 48"×40"
//   PalletID::AU_AS4068_1165x1165 — Australian AS 4068, 1165×1165 mm
//

namespace PalletRegistry {

using Registry = std::unordered_map<PalletID, PalletSpec>;

// Returns the singleton registry populated with all known pallet specs.
[[nodiscard]] const Registry& get();

// Returns a single PalletSpec by enum ID.
// Throws std::out_of_range if the ID is somehow absent from the registry
// (indicates a missing entry in PalletRegistry.cpp, not a caller typo).
[[nodiscard]] const PalletSpec& lookup(PalletID id);

} // namespace PalletRegistry
