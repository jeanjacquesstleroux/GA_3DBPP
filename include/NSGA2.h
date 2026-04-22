#pragma once

#include <random>
#include <vector>
#include "Types.h"

// GASnapshot records the best individual's full packing at one generation.
// Used only under --animated-output to build the ga_history in AnimatedSolution.
// Keeping this struct in NSGA2.h (rather than AnimatedSolution.h) avoids a
// dependency on animation types inside the core algorithm headers.
struct GASnapshot {
    int             generation           = 0;
    int             best_container_count = 0;
    double          best_avg_utilization = 0.0;
    PackingSolution solution;
};

// NSGA2 — Non-dominated Sorting Genetic Algorithm II.
//
// Implements the multi-objective evolutionary optimizer that finds good
// permutations of residual item-type indices for the bin packing problem.
//
// Objectives (all minimized):
//   [0] container count          — fewer containers = better
//   [1] –avg volume utilization  — more compact packing = smaller (more negative)
//   [2] total wasted volume (mm³) — less empty space = better
//
// Paper reference: Ananno & Ribeiro (2024), Section IV-C, Figure 12.
// NSGA-II base algorithm: Deb et al. (2002), IEEE Trans. Evolutionary Computation.
namespace NSGA2 {

// ─── Task 6.10: Fitness evaluation ───────────────────────────────────────────
//
// Decodes ind.chromosome via Packer::decode, then computes the three objectives
// and stores them in ind.objectives[0..2].  If any items cannot be placed even
// in a fresh empty container (out_unplaced > 0), all three objectives receive a
// large additive penalty so penalised individuals are always dominated by any
// feasible solution.
//
// residualCounts[i] — how many residual items of type i must be placed.
// seedContainers    — containers with Phase 1 blocks (passed unchanged to decode).
void evaluateFitness(
    Individual&                   ind,
    const std::vector<int>&       residualCounts,
    const std::vector<ItemType>&  itemTypes,
    const std::vector<Container>& seedContainers,
    bool                          relaxed = false);

// ─── Task 6.7: Fast non-dominated sorting ────────────────────────────────────
//
// Assigns ind.rank to every individual in pop.  Rank 0 = Pareto front (no
// individual in the population dominates this one).  Rank k means exactly k
// fronts dominate it.
//
// Individual A dominates B iff A ≤ B on all objectives AND A < B on at least one.
// Time complexity: O(M × N²) where M = number of objectives, N = population size.
void fastNonDominatedSort(std::vector<Individual>& pop);

// ─── Task 6.8: Crowding distance ─────────────────────────────────────────────
//
// Computes and adds to crowding_distance for each individual in a single Pareto
// front (front contains pointers into the population vector).
//
// For each objective k:
//   — The two boundary individuals (lowest and highest value) get +∞.
//   — Interior individuals get += (next[k] − prev[k]) / (max[k] − min[k]).
//
// If all individuals in the front share the same value for objective k (range ==
// 0), that objective contributes 0 to the distance — no division by zero.
// Caller is responsible for zeroing crowding_distance before calling this
// function if a fresh computation is needed.
void crowdingDistance(std::vector<Individual*>& front);

// ─── Task 6.9: Mu+Lambda selection ───────────────────────────────────────────
//
// Combines parents (GA_POPULATION) + offspring (GA_LAMBDA) into a pool of
// GA_POPULATION + GA_LAMBDA individuals.  Runs fastNonDominatedSort and
// crowdingDistance on the combined pool, then selects the best GA_POPULATION
// individuals by (rank ascending, crowding_distance descending).
//
// Both arguments are taken by value (moved in) because the combined pool is
// built by appending offspring to parents — no need to copy.
[[nodiscard]] std::vector<Individual> muLambdaSelect(
    std::vector<Individual> parents,
    std::vector<Individual> offspring);

// ─── Task 6.11: Pareto front extraction ──────────────────────────────────────
//
// Returns copies of all rank-0 individuals from pop.
// fastNonDominatedSort must have been called on pop before this function.
[[nodiscard]] std::vector<Individual> extractParetoFront(
    const std::vector<Individual>& pop);

// ─── Task 6.13: GA main loop ─────────────────────────────────────────────────
//
// Initialises population → evaluates → sorts → crowding → for each generation:
//   select GA_MU parent pairs → produce GA_LAMBDA offspring via crossover +
//   mutation → evaluate → Mu+Lambda selection → check stagnation.
//
// Stopping criteria (whichever fires first):
//   1. GA_NGEN generations completed.
//   2. Best f₀ (container count, rank-0 front) unchanged for GA_MAX_STAGNATION
//      consecutive generations.
//
// residualTypes — the ordered list of residual item-type indices (chromosome domain).
// residualCounts[i] — count of residual items of type i.
// Returns the final population (sorted, with rank and crowding set).
[[nodiscard]] std::vector<Individual> run(
    const std::vector<int>&       residualTypes,
    const std::vector<int>&       residualCounts,
    const std::vector<ItemType>&  itemTypes,
    const std::vector<Container>& seedContainers,
    std::mt19937&                 rng,
    bool                          relaxed    = false,
    std::vector<GASnapshot>*      ga_history = nullptr);

} // namespace NSGA2
