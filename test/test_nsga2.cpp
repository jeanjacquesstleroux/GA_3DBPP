#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <unordered_set>

#include "Config.h"
#include "GeneticOperators.h"
#include "NSGA2.h"
#include "Packer.h"
#include "Types.h"

// ─── Shared test fixtures ─────────────────────────────────────────────────────

// Build a minimal ItemType for use in tests.
static ItemType makeItem(int l, int w, int h, int m = 1, int q = 1)
{
    ItemType it;
    it.l = l; it.w = w; it.h = h; it.m = m; it.q = q;
    return it;
}

// Build an Individual with a given chromosome and rank/crowding preset.
static Individual makeInd(std::vector<int> chrom, int rank, double crowding)
{
    Individual ind;
    ind.chromosome        = std::move(chrom);
    ind.rank              = rank;
    ind.crowding_distance = crowding;
    ind.objectives        = {static_cast<double>(rank), 0.0, 0.0};
    return ind;
}

// Check that a chromosome is a valid permutation of the expected gene set.
static bool isValidPermutation(const std::vector<int>& chromosome,
                                const std::vector<int>& expected_genes)
{
    if (chromosome.size() != expected_genes.size()) return false;
    std::vector<int> sorted_chrom = chromosome;
    std::vector<int> sorted_genes = expected_genes;
    std::sort(sorted_chrom.begin(), sorted_chrom.end());
    std::sort(sorted_genes.begin(), sorted_genes.end());
    return sorted_chrom == sorted_genes;
}

// ─── Task 6.4: crossover ─────────────────────────────────────────────────────

TEST(CrossoverTest, ProducesValidPermutation)
{
    // With genes {0,1,2,3,4}, any crossover of two permutations must produce
    // a permutation of the same set — no duplicates, no missing genes.
    std::mt19937 rng(42);
    Individual p1; p1.chromosome = {0, 1, 2, 3, 4};
    Individual p2; p2.chromosome = {4, 3, 2, 1, 0};

    for (int trial = 0; trial < 50; ++trial) {
        Individual child = GeneticOperators::crossover(p1, p2, rng);
        EXPECT_TRUE(isValidPermutation(child.chromosome, p1.chromosome))
            << "Crossover produced an invalid permutation on trial " << trial;
    }
}

TEST(CrossoverTest, HandlesLengthOne)
{
    // A single-gene chromosome cannot be split — crossover must return a
    // copy of the parent unchanged.
    std::mt19937 rng(7);
    Individual p1; p1.chromosome = {5};
    Individual p2; p2.chromosome = {5};
    Individual child = GeneticOperators::crossover(p1, p2, rng);
    ASSERT_EQ(child.chromosome.size(), 1u);
    EXPECT_EQ(child.chromosome[0], 5);
}

TEST(CrossoverTest, PrefixFromFirstParent)
{
    // When both parents are the same permutation, every crossover must
    // return the same permutation (no repair needed because there are
    // no duplicates).
    std::mt19937 rng(0);
    Individual p1; p1.chromosome = {0, 1, 2, 3};
    Individual p2; p2.chromosome = {0, 1, 2, 3};
    Individual child = GeneticOperators::crossover(p1, p2, rng);
    EXPECT_EQ(child.chromosome, p1.chromosome);
}

// ─── Task 6.5: mutate ────────────────────────────────────────────────────────

TEST(MutateTest, ProducesValidPermutation)
{
    std::mt19937 rng(99);
    std::vector<int> genes = {0, 1, 2, 3, 4, 5};
    Individual ind; ind.chromosome = genes;

    for (int trial = 0; trial < 30; ++trial) {
        GeneticOperators::mutate(ind, rng);
        EXPECT_TRUE(isValidPermutation(ind.chromosome, genes))
            << "Mutation produced an invalid permutation on trial " << trial;
    }
}

TEST(MutateTest, NoOpOnSingleGene)
{
    std::mt19937 rng(1);
    Individual ind; ind.chromosome = {42};
    GeneticOperators::mutate(ind, rng);  // must not crash or change the chromosome
    ASSERT_EQ(ind.chromosome.size(), 1u);
    EXPECT_EQ(ind.chromosome[0], 42);
}

// ─── Task 6.6: tournamentSelect ──────────────────────────────────────────────

TEST(TournamentTest, SelectsLowerRank)
{
    // A rank-0 individual must always beat a rank-1 individual in tournament.
    // We set crowding_distance equal so the tiebreaker does not interfere.
    std::mt19937 rng(5);
    std::vector<Individual> pop = {
        makeInd({0, 1}, 0, 1.0),  // index 0: rank 0
        makeInd({1, 0}, 1, 1.0),  // index 1: rank 1
    };

    for (int trial = 0; trial < 30; ++trial) {
        const Individual& winner = GeneticOperators::tournamentSelect(pop, rng);
        EXPECT_EQ(winner.rank, 0)
            << "Tournament should always pick rank-0 over rank-1";
    }
}

TEST(TournamentTest, TieBreaksByCrowding)
{
    // Two individuals with equal rank: the one with higher crowding distance wins.
    std::mt19937 rng(3);
    std::vector<Individual> pop = {
        makeInd({0, 1}, 0, 10.0),   // high crowding
        makeInd({1, 0}, 0,  0.5),   // low crowding
    };

    int high_crowding_wins = 0;
    for (int trial = 0; trial < 40; ++trial) {
        const Individual& winner = GeneticOperators::tournamentSelect(pop, rng);
        if (winner.crowding_distance == 10.0) ++high_crowding_wins;
    }
    // With 40 trials and a strong preference, the high-crowding individual
    // should win at least 35 times (it always wins any contest it enters).
    EXPECT_GE(high_crowding_wins, 35);
}

// ─── Task 6.2: makeSeededIndividuals ─────────────────────────────────────────

TEST(SeedingTest, ProducesTenValidPermutations)
{
    std::vector<ItemType> items = {
        makeItem(300, 200, 100, 5, 10),
        makeItem(400, 300, 200, 2,  5),
        makeItem(100, 100, 100, 8, 20),
    };
    std::vector<int> residualTypes = {0, 1, 2};

    auto seeded = GeneticOperators::makeSeededIndividuals(residualTypes, items);

    ASSERT_EQ(seeded.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(isValidPermutation(seeded[i].chromosome, residualTypes))
            << "Seeded individual " << i << " is not a valid permutation";
    }
}

// ─── Task 6.3: initPopulation ────────────────────────────────────────────────

TEST(PopulationTest, SizeEqualsGAPopulation)
{
    std::mt19937 rng(42);
    std::vector<ItemType> items = {
        makeItem(200, 200, 200, 1, 3),
        makeItem(300, 200, 100, 2, 2),
    };
    std::vector<int> residualTypes = {0, 1};

    auto pop = GeneticOperators::initPopulation(residualTypes, items, rng);
    EXPECT_EQ(static_cast<int>(pop.size()), Config::GA_POPULATION);

    for (const Individual& ind : pop) {
        EXPECT_TRUE(isValidPermutation(ind.chromosome, residualTypes));
    }
}

// ─── Task 6.7: fastNonDominatedSort ──────────────────────────────────────────

TEST(NSDSortTest, ParetoFrontHasRankZero)
{
    // Construct a population where (1,1) and (2,0) are on the Pareto front
    // and (2,2) is dominated by both.
    // Objectives: [f0, f1], both minimized.
    std::vector<Individual> pop(3);
    pop[0].objectives = {1.0, 1.0};  // Pareto front
    pop[1].objectives = {2.0, 0.0};  // Pareto front
    pop[2].objectives = {2.0, 2.0};  // dominated by pop[0]

    NSGA2::fastNonDominatedSort(pop);

    EXPECT_EQ(pop[0].rank, 0);
    EXPECT_EQ(pop[1].rank, 0);
    EXPECT_EQ(pop[2].rank, 1);
}

TEST(NSDSortTest, LinearChainRanks)
{
    // (0,0) dominates (1,1) dominates (2,2) — a strict chain.
    std::vector<Individual> pop(3);
    pop[0].objectives = {0.0, 0.0};
    pop[1].objectives = {1.0, 1.0};
    pop[2].objectives = {2.0, 2.0};

    NSGA2::fastNonDominatedSort(pop);

    EXPECT_EQ(pop[0].rank, 0);
    EXPECT_EQ(pop[1].rank, 1);
    EXPECT_EQ(pop[2].rank, 2);
}

// ─── Task 6.8: crowdingDistance ──────────────────────────────────────────────

TEST(CrowdingTest, BoundaryIndividualsGetInfinity)
{
    // A front of 3 individuals: the two extremes must have infinite crowding.
    std::vector<Individual> storage(3);
    storage[0].objectives = {0.0, 2.0};
    storage[1].objectives = {1.0, 1.0};
    storage[2].objectives = {2.0, 0.0};
    for (Individual& ind : storage) ind.crowding_distance = 0.0;

    std::vector<Individual*> front = {&storage[0], &storage[1], &storage[2]};
    NSGA2::crowdingDistance(front);

    // After sorting by objective[0], storage[0] is at one end and storage[2]
    // at the other — both must have infinity.
    int inf_count = 0;
    for (Individual& ind : storage) {
        if (ind.crowding_distance == std::numeric_limits<double>::infinity())
            ++inf_count;
    }
    EXPECT_EQ(inf_count, 2);
}

TEST(CrowdingTest, InteriorIndividualGetsFiniteDistance)
{
    // Three evenly-spaced individuals: the middle one gets a finite, positive
    // crowding distance (normalised gap = 1 in each objective).
    std::vector<Individual> storage(3);
    storage[0].objectives = {0.0, 0.0};
    storage[1].objectives = {1.0, 1.0};
    storage[2].objectives = {2.0, 2.0};
    for (Individual& ind : storage) ind.crowding_distance = 0.0;

    std::vector<Individual*> front = {&storage[0], &storage[1], &storage[2]};
    NSGA2::crowdingDistance(front);

    // storage[1] is interior in both objectives, so its crowding distance is
    // 2 × (2-0)/(2-0) = 2.0 (one full normalised gap per objective).
    // Check it is finite and positive.
    EXPECT_TRUE(std::isfinite(storage[1].crowding_distance));
    EXPECT_GT(storage[1].crowding_distance, 0.0);
}

// ─── Task 6.1: Packer::decode ────────────────────────────────────────────────

TEST(PackerTest, PlacesSingleItemInEmptyContainer)
{
    // One item type of size 100×100×100, one residual.
    // decode with an empty seedContainers should open a new container and
    // place the item at the origin.
    std::vector<ItemType> items = { makeItem(100, 100, 100) };
    std::vector<int> residualCounts = {1};
    std::vector<int> chromosome     = {0};

    int unplaced = -1;
    PackingSolution sol = Packer::decode(
        chromosome, residualCounts, items, {}, unplaced);

    EXPECT_EQ(unplaced, 0);
    ASSERT_EQ(sol.containers.size(), 1u);
    ASSERT_EQ(sol.containers[0].items.size(), 1u);

    const PlacedItem& pi = sol.containers[0].items[0];
    EXPECT_EQ(pi.x, 0);
    EXPECT_EQ(pi.y, 0);
    EXPECT_EQ(pi.z, 0);
    EXPECT_EQ(pi.dx, 100);
    EXPECT_EQ(pi.dy, 100);
    EXPECT_EQ(pi.dz, 100);
}

TEST(PackerTest, OpensNewContainerWhenFull)
{
    // Fill the pallet with large items so the second one must go on a new pallet.
    // Item fills the entire footprint in one placement (1200×800×200).
    // Two residual items — first fills container[0], second must open container[1].
    std::vector<ItemType> items = { makeItem(1200, 800, 200) };
    std::vector<int> residualCounts = {2};
    std::vector<int> chromosome     = {0};

    // The pallet is 1200×800×1400 — one item fills the whole floor.
    // The second item cannot fit on top without exceeding height 1400, so a
    // new container is opened (2×200 = 400 < 1400 is fine, but EP generation
    // after the first placement will produce an EP at z=200 with dims 1200×800,
    // which will be accepted as long as 400 ≤ 1400).  Use 4 items to guarantee
    // that at least 2 containers are needed (4×200 = 800, still fits in 1400).
    // Instead, use items taller than half the pallet height so 2 cannot stack.
    ItemType tall_item = makeItem(1200, 800, 800);
    tall_item.q = 3;
    std::vector<ItemType> items2 = { tall_item };
    residualCounts = {3};

    int unplaced = -1;
    PackingSolution sol = Packer::decode(
        chromosome, residualCounts, items2, {}, unplaced);

    EXPECT_EQ(unplaced, 0);
    // 3 items of height 800 cannot all fit in one container (1400 mm max).
    // Two fit (800+800=1600 > 1400? No: 800+800=1600 > 1400, so only 1 fits).
    // Actually 800 ≤ 1400 but the second needs to go at z=800, giving extent
    // z+dz = 800+800 = 1600 > 1400 — so each item needs its own container.
    EXPECT_GE(sol.containers.size(), 2u);

    // Total placed items across all containers must equal 3.
    int total_placed = 0;
    for (const Container& c : sol.containers) {
        total_placed += static_cast<int>(c.items.size());
    }
    EXPECT_EQ(total_placed, 3);
}

TEST(PackerTest, NoCollisionsBetweenPlacedItems)
{
    // Place several small items and verify no two overlap.
    std::vector<ItemType> items = { makeItem(100, 100, 100) };
    std::vector<int> residualCounts = {8};
    std::vector<int> chromosome     = {0};

    int unplaced = -1;
    PackingSolution sol = Packer::decode(
        chromosome, residualCounts, items, {}, unplaced);

    EXPECT_EQ(unplaced, 0);

    for (const Container& cont : sol.containers) {
        const auto& placed = cont.items;
        for (int i = 0; i < static_cast<int>(placed.size()); ++i) {
            for (int j = i + 1; j < static_cast<int>(placed.size()); ++j) {
                const PlacedItem& a = placed[i];
                const PlacedItem& b = placed[j];
                // AABB overlap: intervals [ax, ax+adx) and [bx, bx+bdx) overlap
                // iff ax < bx+bdx AND bx < ax+adx (and same for y, z).
                bool overlap =
                    a.x < b.x + b.dx && b.x < a.x + a.dx &&
                    a.y < b.y + b.dy && b.y < a.y + a.dy &&
                    a.z < b.z + b.dz && b.z < a.z + a.dz;
                EXPECT_FALSE(overlap)
                    << "Items " << i << " and " << j << " overlap in the same container";
            }
        }
    }
}

// ─── Task 6.10: evaluateFitness ──────────────────────────────────────────────

TEST(FitnessTest, SetThreeObjectives)
{
    std::vector<ItemType> items = { makeItem(100, 100, 100) };
    std::vector<int> residualCounts = {2};
    Individual ind;
    ind.chromosome = {0};

    NSGA2::evaluateFitness(ind, residualCounts, items, {});

    ASSERT_EQ(ind.objectives.size(), 3u);
    // At least one container was used (f0 ≥ 1).
    EXPECT_GE(ind.objectives[0], 1.0);
    // Utilization is in [0,1], so f1 = -util is in [-1, 0].
    EXPECT_LE(ind.objectives[1], 0.0);
    // Wasted volume is non-negative.
    EXPECT_GE(ind.objectives[2], 0.0);
}

// ─── Task 6.9: muLambdaSelect ────────────────────────────────────────────────

TEST(MuLambdaTest, ReturnsExactlyPopulationSize)
{
    // Build a parent population and offspring of the right sizes, then verify
    // that muLambdaSelect returns exactly GA_POPULATION individuals.
    std::vector<Individual> parents, offspring;

    for (int i = 0; i < Config::GA_POPULATION; ++i) {
        Individual ind;
        ind.chromosome = {0, 1};
        ind.objectives = {static_cast<double>(i % 5), static_cast<double>(i % 3), 0.0};
        parents.push_back(ind);
    }
    for (int i = 0; i < Config::GA_LAMBDA; ++i) {
        Individual ind;
        ind.chromosome = {1, 0};
        ind.objectives = {static_cast<double>(i % 4), static_cast<double>(i % 2), 0.0};
        offspring.push_back(ind);
    }

    auto result = NSGA2::muLambdaSelect(parents, offspring);
    EXPECT_EQ(static_cast<int>(result.size()), Config::GA_POPULATION);
}

// ─── Task 6.11: extractParetoFront ───────────────────────────────────────────

TEST(ParetoFrontTest, ReturnsOnlyRankZero)
{
    std::vector<Individual> pop = {
        makeInd({0}, 0, 1.0),   // Pareto front
        makeInd({1}, 1, 2.0),   // rank 1
        makeInd({2}, 0, 0.5),   // Pareto front
        makeInd({3}, 2, 3.0),   // rank 2
    };

    auto front = NSGA2::extractParetoFront(pop);
    ASSERT_EQ(front.size(), 2u);
    for (const Individual& ind : front) {
        EXPECT_EQ(ind.rank, 0);
    }
}

// ─── Task 6.13: run — convergence smoke test ──────────────────────────────────

TEST(RunTest, ConvergesOnTrivialInstance)
{
    // Two small item types, one of each — both trivially fit in a single pallet.
    // The GA must find a one-container solution in the Pareto front.
    std::vector<ItemType> items = {
        makeItem(100, 100, 100, 1, 1),
        makeItem(200, 150,  80, 1, 1),
    };
    std::vector<int> residualTypes  = {0, 1};
    std::vector<int> residualCounts = {1, 1};

    std::mt19937 rng(42);
    auto pop = NSGA2::run(residualTypes, residualCounts, items, {}, rng);

    ASSERT_FALSE(pop.empty());

    auto front = NSGA2::extractParetoFront(pop);
    ASSERT_FALSE(front.empty());

    // The best solution must use exactly 1 container for this tiny instance.
    double best_f0 = front[0].objectives[0];
    for (const Individual& ind : front) {
        best_f0 = std::min(best_f0, ind.objectives[0]);
    }
    EXPECT_DOUBLE_EQ(best_f0, 1.0);
}
