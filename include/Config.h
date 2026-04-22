#pragma once

// Config.h — compile-time algorithm parameters for GA_3DBPP.
// All constants are constexpr: typed, scoped, debugger-visible, and guaranteed
// to be evaluated at compile time. Use Config::PALLET_L etc. throughout the codebase.
// Never use #define for these values.

namespace Config {

    // -------------------------------------------------------------------------
    // Pallet (container) dimensions — Euro pallet standard (mm)
    // -------------------------------------------------------------------------
    constexpr int PALLET_L = 1200;  // length (mm)
    constexpr int PALLET_W =  800;  // width  (mm)
    constexpr int PALLET_H = 1400;  // height (mm)

    // Maximum load mass per pallet (kg). Euro pallet static load limit.
    constexpr int PALLET_MAX_WEIGHT = 1000;

    // -------------------------------------------------------------------------
    // Support checking parameters (Ananno & Ribeiro 2024, Section IV-B-1)
    // -------------------------------------------------------------------------
    constexpr int    SUPPORT_VERTEX_INSET  =  10;   // mm pushed inward per base vertex
    constexpr int    SUPPORT_TIER1_VERTS   =   4;   // Tier 1: 4 vertices + 40% area
    constexpr double SUPPORT_TIER1_AREA    = 0.40;
    constexpr int    SUPPORT_TIER2_VERTS   =   3;   // Tier 2: 3 vertices + 50% area
    constexpr double SUPPORT_TIER2_AREA    = 0.50;
    constexpr int    SUPPORT_TIER3_VERTS   =   2;   // Tier 3: 2 vertices + 75% area
    constexpr double SUPPORT_TIER3_AREA    = 0.75;

    // Center of mass deviation limit from the pallet's geometric center (mm).
    constexpr int    COM_MAX_DEVIATION     =  60;

    // -------------------------------------------------------------------------
    // Layer fill-rate thresholds (Ananno & Ribeiro 2024, Section IV-B-2)
    // A layer is accepted only if the fraction of its footprint covered by items
    // meets or exceeds the threshold for its type.
    // -------------------------------------------------------------------------
    constexpr double LAYER_MIN_FILL_FULL_HALF = 0.90; // Full and Half layers need ≥ 90 %
    constexpr double LAYER_MIN_FILL_QUARTER   = 0.85; // Quarter layers need ≥ 85 %

    // -------------------------------------------------------------------------
    // Genetic algorithm hyperparameters (Ananno & Ribeiro 2024, Section IV-B)
    // -------------------------------------------------------------------------
    constexpr int    GA_POPULATION     = 100;  // total individuals per generation
    constexpr int    GA_MU             =  15;  // number of parents selected each generation
    constexpr int    GA_LAMBDA         =  30;  // number of offspring produced each generation
    constexpr double GA_CROSSOVER_RATE = 0.5;  // probability of crossover between two parents
    constexpr double GA_MUTATION_RATE  = 0.2;  // probability of mutating an offspring
    constexpr int    GA_NGEN           =  30;  // maximum number of generations
    constexpr int    GA_MAX_STAGNATION =   5;  // stop early if best fitness unchanged this long

    // ── Animated output — GA history recording interval ──────────────────────
    // Under --animated-output, record a snapshot of the best packing every
    // GA_HISTORY_INTERVAL generations.  Generation 0 and the final generation
    // are always recorded regardless of this value.
    // When GA_NGEN < 20, the interval is overridden to 1 (record every gen).
    constexpr int    GA_HISTORY_INTERVAL = 5;

} // namespace Config

// Compile-time sanity checks on GA parameters.
// These fire at build time if the constants above are set to logically invalid values.
static_assert(Config::GA_MU >= 1,
    "GA_MU must be at least 1. The GA needs at least one parent.");
static_assert(Config::GA_LAMBDA >= 1,
    "GA_LAMBDA must be at least 1. The GA needs at least one offspring.");
static_assert(Config::GA_MU <= Config::GA_POPULATION,
    "GA_MU cannot exceed GA_POPULATION. Cannot select more parents than exist.");
static_assert(Config::GA_LAMBDA >= Config::GA_MU,
    "GA_LAMBDA must be >= GA_MU. Offspring count must meet or exceed parent count.");
