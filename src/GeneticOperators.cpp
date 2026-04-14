#include "GeneticOperators.h"
#include "Config.h"

#include <algorithm>
#include <numeric>
#include <unordered_map>

namespace GeneticOperators {

// ─── Task 6.2: makeSeededIndividuals ─────────────────────────────────────────

// Internal helper: returns one Individual whose chromosome is residualTypes
// sorted by keyFn in the given direction.  keyFn must accept a const ItemType&
// and return a value comparable with operator<.  The template lets the compiler
// deduce the return type, so both int and long long keys work without casts at
// the call site.
template<typename KeyFn>
static Individual makeSorted(
    const std::vector<int>&      residualTypes,
    const std::vector<ItemType>& itemTypes,
    KeyFn                        keyFn,
    bool                         ascending)
{
    Individual ind;
    ind.chromosome = residualTypes;
    if (ascending) {
        std::sort(ind.chromosome.begin(), ind.chromosome.end(),
            [&](int a, int b) { return keyFn(itemTypes[a]) < keyFn(itemTypes[b]); });
    } else {
        std::sort(ind.chromosome.begin(), ind.chromosome.end(),
            [&](int a, int b) { return keyFn(itemTypes[a]) > keyFn(itemTypes[b]); });
    }
    return ind;
}

std::vector<Individual> makeSeededIndividuals(
    const std::vector<int>&      residualTypes,
    const std::vector<ItemType>& itemTypes)
{
    std::vector<Individual> seeded;
    seeded.reserve(10);

    // 1 & 2: weight (mass)
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.m; }, true));
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.m; }, false));

    // 3 & 4: quantity
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.q; }, true));
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.q; }, false));

    // 5 & 6: base area (l × w)
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.baseArea(); }, true));
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.baseArea(); }, false));

    // 7 & 8: volume (l × w × h)
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.volume(); }, true));
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) { return it.volume(); }, false));

    // 9 & 10: volume × quantity — cast to long long to avoid int overflow
    // (e.g. 1200×800×1400 × 100 ≈ 1.3×10¹¹ exceeds INT_MAX).
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) -> long long {
            return static_cast<long long>(it.volume()) * it.q;
        }, true));
    seeded.push_back(makeSorted(residualTypes, itemTypes,
        [](const ItemType& it) -> long long {
            return static_cast<long long>(it.volume()) * it.q;
        }, false));

    return seeded;
}

// ─── Task 6.3: fillRandom ────────────────────────────────────────────────────

void fillRandom(
    std::vector<Individual>&  pop,
    const std::vector<int>&   residualTypes,
    int                       targetSize,
    std::mt19937&             rng)
{
    while (static_cast<int>(pop.size()) < targetSize) {
        Individual ind;
        ind.chromosome = residualTypes;
        std::shuffle(ind.chromosome.begin(), ind.chromosome.end(), rng);
        pop.push_back(std::move(ind));
    }
}

std::vector<Individual> initPopulation(
    const std::vector<int>&      residualTypes,
    const std::vector<ItemType>& itemTypes,
    std::mt19937&                rng)
{
    auto pop = makeSeededIndividuals(residualTypes, itemTypes);
    fillRandom(pop, residualTypes, Config::GA_POPULATION, rng);
    return pop;
}

// ─── Task 6.4: crossover ─────────────────────────────────────────────────────

Individual crossover(
    const Individual& p1,
    const Individual& p2,
    std::mt19937&     rng)
{
    const int n = static_cast<int>(p1.chromosome.size());
    if (n <= 1) {
        Individual child;
        child.chromosome = p1.chromosome;
        return child;
    }

    // Pick a cut point in [1, n-1] so both halves are non-empty.
    std::uniform_int_distribution<int> dist(1, n - 1);
    const int cut = dist(rng);

    // Build child: prefix from p1, suffix from p2.
    Individual child;
    child.chromosome.resize(n);
    for (int i = 0; i < cut; ++i) child.chromosome[i] = p1.chromosome[i];
    for (int i = cut; i < n;   ++i) child.chromosome[i] = p2.chromosome[i];

    // Repair step — count occurrences of every gene in child.
    // Genes present in the prefix (from p1) will appear once there; genes
    // introduced by the suffix (from p2) may duplicate what is already in
    // the prefix.
    std::unordered_map<int, int> counts;
    counts.reserve(n);
    for (int g : child.chromosome) ++counts[g];

    // Collect missing genes: present in the full gene set (== p1.chromosome)
    // but with count 0 in the child.  These are the genes that the suffix
    // "displaced" by introducing duplicates.
    std::vector<int> missing;
    for (int g : p1.chromosome) {
        if (counts[g] == 0) missing.push_back(g);
    }

    // Replace duplicates in the suffix (indices [cut, n)) from left to right,
    // substituting each duplicate with the next missing gene.
    int miss_idx = 0;
    for (int i = cut; i < n && miss_idx < static_cast<int>(missing.size()); ++i) {
        int gene = child.chromosome[i];
        if (counts[gene] > 1) {
            --counts[gene];                       // one fewer copy of the duplicate
            child.chromosome[i] = missing[miss_idx];
            ++counts[missing[miss_idx]];          // now count == 1 for the inserted gene
            ++miss_idx;
        }
    }

    return child;
}

// ─── Task 6.5: mutate ────────────────────────────────────────────────────────

void mutate(Individual& ind, std::mt19937& rng)
{
    const int n = static_cast<int>(ind.chromosome.size());
    if (n < 2) return;

    std::uniform_int_distribution<int> dist(0, n - 1);
    int i = dist(rng);
    int j = dist(rng);
    // Ensure two distinct positions — resample j if it collides with i.
    while (j == i) j = dist(rng);

    std::swap(ind.chromosome[i], ind.chromosome[j]);
}

// ─── Task 6.6: tournamentSelect ──────────────────────────────────────────────

const Individual& tournamentSelect(
    const std::vector<Individual>& pop,
    std::mt19937&                  rng)
{
    const int n = static_cast<int>(pop.size());
    std::uniform_int_distribution<int> dist(0, n - 1);

    int a = dist(rng);
    int b = dist(rng);
    while (b == a) b = dist(rng);  // guarantee two distinct contestants

    const Individual& ia = pop[a];
    const Individual& ib = pop[b];

    // Lower Pareto rank is better (rank 0 = Pareto front).
    if (ia.rank < ib.rank) return ia;
    if (ib.rank < ia.rank) return ib;

    // Equal rank: higher crowding_distance wins (preserves population diversity).
    return (ia.crowding_distance >= ib.crowding_distance) ? ia : ib;
}

} // namespace GeneticOperators
