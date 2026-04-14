#pragma once

#include <random>
#include <vector>
#include "Types.h"

// GeneticOperators — chromosome-level operations for the NSGA-II GA.
//
// All functions are stateless: they take the population / individuals by
// reference (or value for inputs) and return results without storing anything.
// The std::mt19937 RNG is passed by reference so callers control seeding and
// tests can use a fixed seed for determinism.
//
// Paper reference: Ananno & Ribeiro (2024), Section IV-C.
namespace GeneticOperators {

// ─── Task 6.2: Population seeding ────────────────────────────────────────────
//
// Returns exactly 10 individuals whose chromosomes are the residualTypes
// permutation sorted by five criteria × {ascending, descending} (Table 5):
//
//   1. weight (mass m) ascending
//   2. weight descending
//   3. quantity (q) ascending
//   4. quantity descending
//   5. base area (l×w) ascending
//   6. base area descending
//   7. volume (l×w×h) ascending
//   8. volume descending
//   9. volume × quantity ascending
//  10. volume × quantity descending
//
// residualTypes — the ordered list of item-type indices that are residuals.
//                 Every seeded chromosome is a permutation of this list.
[[nodiscard]] std::vector<Individual> makeSeededIndividuals(
    const std::vector<int>&      residualTypes,
    const std::vector<ItemType>& itemTypes);

// ─── Task 6.3: Random population fill ────────────────────────────────────────
//
// Appends shuffled copies of residualTypes to pop until pop.size() == targetSize.
// Uses std::shuffle with rng, so the caller controls reproducibility via seeding.
void fillRandom(
    std::vector<Individual>&  pop,
    const std::vector<int>&   residualTypes,
    int                       targetSize,
    std::mt19937&             rng);

// Convenience: create the full initial population (10 seeded + random fill to
// Config::GA_POPULATION).  Returns a vector of GA_POPULATION individuals, each
// with an empty objectives vector and rank = 0, crowding_distance = 0.
[[nodiscard]] std::vector<Individual> initPopulation(
    const std::vector<int>&      residualTypes,
    const std::vector<ItemType>& itemTypes,
    std::mt19937&                rng);

// ─── Task 6.4: Single-point crossover with repair ────────────────────────────
//
// Produces one child from two parents:
//   child = p1.chromosome[0..cut) + p2.chromosome[cut..n)
// where cut is sampled uniformly in [1, n-1].
//
// Repair: after concatenation, any gene in the suffix that already appears in
// the prefix is a duplicate.  For each duplicate (left-to-right in the suffix),
// the repair substitutes the next missing gene (a gene present in p1/p2 but
// absent from the child so far).  The result is always a valid permutation of
// the same gene set.
//
// Precondition: p1 and p2 are permutations of the same gene set.
// If the chromosome has ≤ 1 elements, returns a copy of p1 unchanged.
[[nodiscard]] Individual crossover(
    const Individual& p1,
    const Individual& p2,
    std::mt19937&     rng);

// ─── Task 6.5: Swap mutation ──────────────────────────────────────────────────
//
// Selects two distinct random positions in ind.chromosome and swaps them.
// A swap always produces a valid permutation and is the simplest mutation that
// can explore all permutations from any starting point.
// No-op if the chromosome has fewer than 2 elements.
void mutate(Individual& ind, std::mt19937& rng);

// ─── Task 6.6: Binary tournament selection ───────────────────────────────────
//
// Randomly selects two distinct individuals from pop (uniform sampling without
// replacement) and returns a const-reference to the winner:
//   — Lower rank (Pareto front) wins.
//   — If ranks are equal, higher crowding_distance wins (preserves diversity).
//
// Precondition: pop has at least 2 elements and all individuals have rank and
// crowding_distance already set (i.e., fastNonDominatedSort + crowdingDistance
// have been called on the population).
[[nodiscard]] const Individual& tournamentSelect(
    const std::vector<Individual>& pop,
    std::mt19937&                  rng);

} // namespace GeneticOperators
