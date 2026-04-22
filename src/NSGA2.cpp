#include "NSGA2.h"
#include "GeneticOperators.h"
#include "Packer.h"
#include "Config.h"

#include <algorithm>
#include <climits>
#include <limits>

namespace NSGA2 {

// ─── Internal helper: assign crowding distance to every front in pop ──────────
//
// Groups individuals by rank, then calls crowdingDistance() per front.
// Resets crowding_distance to 0.0 before computing so the result is always
// a fresh, correct value regardless of prior state.
static void assignCrowdingToAll(std::vector<Individual>& pop)
{
    if (pop.empty()) return;

    // Reset all crowding distances before computing fresh values.
    for (Individual& ind : pop) ind.crowding_distance = 0.0;

    int max_rank = 0;
    for (const Individual& ind : pop) {
        if (ind.rank > max_rank) max_rank = ind.rank;
    }

    // Process one Pareto front at a time.
    for (int r = 0; r <= max_rank; ++r) {
        std::vector<Individual*> front;
        for (Individual& ind : pop) {
            if (ind.rank == r) front.push_back(&ind);
        }
        if (!front.empty()) crowdingDistance(front);
    }
}

// ─── Task 6.10: evaluateFitness ──────────────────────────────────────────────

void evaluateFitness(
    Individual&                   ind,
    const std::vector<int>&       residualCounts,
    const std::vector<ItemType>&  itemTypes,
    const std::vector<Container>& seedContainers,
    bool                          relaxed)
{
    int unplaced = 0;
    PackingSolution sol = Packer::decode(
        ind.chromosome, residualCounts, itemTypes, seedContainers, unplaced, relaxed);

    const int    n_containers = static_cast<int>(sol.containers.size());
    const double avg_util     = sol.avgUtilization();

    // Compute total wasted volume across all containers.
    // Use double arithmetic; individual container volumes can be up to
    // 1200×800×1400 = 1,344,000,000 mm³ (fits in int, but summing several
    // containers may exceed INT_MAX, so double is safer here).
    double wasted = 0.0;
    for (const Container& c : sol.containers) {
        const double cap = static_cast<double>(c.L) * c.W * c.H;
        double used = 0.0;
        for (const PlacedItem& pi : c.items) {
            used += static_cast<double>(pi.dx) * pi.dy * pi.dz;
        }
        wasted += cap - used;
    }

    // Auxiliary metric: utilization of the single most-filled container.
    // Not used by NSGA-II dominance/crowding (paper-compliant 3-objective scheme).
    // Stored so selectBest() can use it as a final tiebreaker when all three
    // objectives are equal — this happens for high-density BR instances where
    // every chromosome produces the same container count and all items are placed.
    double max_util = 0.0;
    for (const Container& c : sol.containers) {
        const double cap = static_cast<double>(c.L) * c.W * c.H;
        if (cap <= 0.0) continue;
        double used = 0.0;
        for (const PlacedItem& pi : c.items)
            used += static_cast<double>(pi.dx) * pi.dy * pi.dz;
        const double u = used / cap;
        if (u > max_util) max_util = u;
    }
    ind.aux_max_util = max_util;

    // Penalty for any items that could not be placed even in a fresh empty
    // container (physically infeasible items).  A penalty of 1000× the raw
    // objective value is large enough to ensure every penalised individual is
    // dominated by any feasible solution, regardless of how many containers
    // the feasible solution uses.
    constexpr double PENALTY = 1000.0;
    const double pen = static_cast<double>(unplaced) * PENALTY;

    ind.objectives.resize(3);
    ind.objectives[0] = static_cast<double>(n_containers) + pen;
    ind.objectives[1] = -avg_util + pen;  // negated so "minimize" == "maximise util"
    ind.objectives[2] = wasted            + pen;
}

// ─── Task 6.7: fastNonDominatedSort ──────────────────────────────────────────

void fastNonDominatedSort(std::vector<Individual>& pop)
{
    const int n = static_cast<int>(pop.size());
    if (n == 0) return;

    const int m = static_cast<int>(pop[0].objectives.size());

    // domination_count[i]: how many individuals in pop dominate individual i.
    // If this drops to 0, individual i joins the current Pareto front.
    std::vector<int> domination_count(n, 0);

    // dominated_set[i]: indices of individuals that i dominates.
    // When i is added to a front, each member of its dominated_set gets its
    // domination_count decremented by 1.
    std::vector<std::vector<int>> dominated_set(n);

    // Lambda: returns true if individual a dominates individual b.
    // A dominates B: A[k] ≤ B[k] for all k, AND A[k] < B[k] for at least one k.
    auto dominates = [&](int a, int b) -> bool {
        bool strictly_better = false;
        for (int k = 0; k < m; ++k) {
            if (pop[a].objectives[k] > pop[b].objectives[k]) return false;
            if (pop[a].objectives[k] < pop[b].objectives[k]) strictly_better = true;
        }
        return strictly_better;
    };

    // Build domination relationships for every pair.
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            if (dominates(i, j)) {
                dominated_set[i].push_back(j);
            } else if (dominates(j, i)) {
                ++domination_count[i];
            }
        }
    }

    // Identify the first Pareto front (rank 0): individuals not dominated by anyone.
    std::vector<int> current_front;
    for (int i = 0; i < n; ++i) {
        if (domination_count[i] == 0) {
            pop[i].rank = 0;
            current_front.push_back(i);
        }
    }

    // Propagate ranks to successive fronts.
    int rank = 0;
    while (!current_front.empty()) {
        std::vector<int> next_front;
        for (int i : current_front) {
            for (int j : dominated_set[i]) {
                --domination_count[j];
                if (domination_count[j] == 0) {
                    pop[j].rank = rank + 1;
                    next_front.push_back(j);
                }
            }
        }
        ++rank;
        current_front = std::move(next_front);
    }
}

// ─── Task 6.8: crowdingDistance ──────────────────────────────────────────────

void crowdingDistance(std::vector<Individual*>& front)
{
    const int n = static_cast<int>(front.size());

    // Trivial cases: 1 or 2 individuals always get infinite crowding distance
    // (they are always boundary solutions regardless of objective values).
    if (n <= 2) {
        for (Individual* ind : front) {
            ind->crowding_distance = std::numeric_limits<double>::infinity();
        }
        return;
    }

    // crowding_distance is ADD-ed to, not assigned, so the caller must zero it
    // first if a fresh computation is needed.  assignCrowdingToAll() handles this.
    const int m = static_cast<int>(front[0]->objectives.size());

    for (int k = 0; k < m; ++k) {
        // Sort the front by objective k (ascending) to find neighbours.
        std::sort(front.begin(), front.end(),
            [k](const Individual* a, const Individual* b) {
                return a->objectives[k] < b->objectives[k];
            });

        // Boundary solutions always get infinity for this objective.
        front[0]->crowding_distance     = std::numeric_limits<double>::infinity();
        front[n - 1]->crowding_distance = std::numeric_limits<double>::infinity();

        const double obj_min = front[0]->objectives[k];
        const double obj_max = front[n - 1]->objectives[k];
        const double range   = obj_max - obj_min;

        // If all individuals share the same value for objective k, this objective
        // contributes nothing to the crowding distance — skip to avoid division
        // by zero.
        if (range == 0.0) continue;

        for (int i = 1; i < n - 1; ++i) {
            double gap = front[i + 1]->objectives[k] - front[i - 1]->objectives[k];
            front[i]->crowding_distance += gap / range;
        }
    }
}

// ─── Task 6.9: muLambdaSelect ────────────────────────────────────────────────

std::vector<Individual> muLambdaSelect(
    std::vector<Individual> parents,
    std::vector<Individual> offspring)
{
    // Combine by appending offspring into parents (move avoids copying chromosomes).
    parents.reserve(parents.size() + offspring.size());
    for (Individual& ind : offspring) {
        parents.push_back(std::move(ind));
    }

    // Fresh non-dominated sort on the combined pool.
    fastNonDominatedSort(parents);

    // Fresh crowding distance per front on the combined pool.
    assignCrowdingToAll(parents);

    // Sort combined pool: rank ascending, then crowding_distance descending.
    // std::stable_sort preserves the original order among equal elements, which
    // gives deterministic tie-breaking independent of internal RNG state.
    std::stable_sort(parents.begin(), parents.end(),
        [](const Individual& a, const Individual& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.crowding_distance > b.crowding_distance;
        });

    // Keep only the best GA_POPULATION individuals.
    if (static_cast<int>(parents.size()) > Config::GA_POPULATION) {
        parents.resize(Config::GA_POPULATION);
    }

    return parents;
}

// ─── Task 6.11: extractParetoFront ───────────────────────────────────────────

std::vector<Individual> extractParetoFront(const std::vector<Individual>& pop)
{
    std::vector<Individual> front;
    for (const Individual& ind : pop) {
        if (ind.rank == 0) front.push_back(ind);
    }
    return front;
}

// ─── Task 6.13: run — GA main loop ───────────────────────────────────────────

std::vector<Individual> run(
    const std::vector<int>&       residualTypes,
    const std::vector<int>&       residualCounts,
    const std::vector<ItemType>&  itemTypes,
    const std::vector<Container>& seedContainers,
    std::mt19937&                 rng,
    bool                          relaxed,
    std::vector<GASnapshot>*      ga_history)
{
    // Edge case: nothing to pack — return an empty population immediately.
    if (residualTypes.empty()) return {};

    // ── Initialise population ────────────────────────────────────────────────
    auto pop = GeneticOperators::initPopulation(residualTypes, itemTypes, rng);

    // Evaluate fitness for every individual in the initial population.
    for (Individual& ind : pop) {
        evaluateFitness(ind, residualCounts, itemTypes, seedContainers, relaxed);
    }

    // Initial non-dominated sort + crowding so that tournament selection has
    // valid rank and crowding_distance values from the very first generation.
    fastNonDominatedSort(pop);
    assignCrowdingToAll(pop);

    // ── Stagnation tracking ──────────────────────────────────────────────────
    // Track the best container count (objectives[0]) seen across rank-0
    // individuals.  Using INT_MAX as the initial sentinel ensures the first
    // generation always resets it.
    int best_f0        = INT_MAX;
    int stagnation     = 0;

    std::uniform_real_distribution<double> unit(0.0, 1.0);

    // ── GA history recording (animated output only) ───────────────────────────
    // Interval: every GA_HISTORY_INTERVAL generations, plus generation 0 and
    // the last generation.  When GA_NGEN < 20 the interval collapses to 1 so
    // every generation is recorded.
    const int history_interval = (Config::GA_NGEN < 20) ? 1 : Config::GA_HISTORY_INTERVAL;

    // Lambda: find the best Individual in pop using the same selectBest() logic
    // as main.cpp (min objectives[0] → min objectives[2] → max aux_max_util).
    auto findBest = [](const std::vector<Individual>& p) -> const Individual& {
        return *std::min_element(p.begin(), p.end(),
            [](const Individual& a, const Individual& b) {
                if (a.objectives[0] != b.objectives[0]) return a.objectives[0] < b.objectives[0];
                if (a.objectives[2] != b.objectives[2]) return a.objectives[2] < b.objectives[2];
                return a.aux_max_util > b.aux_max_util;
            });
    };

    // Lambda: decode the best individual and append a GASnapshot to ga_history.
    auto recordSnapshot = [&](int gen_idx, const std::vector<Individual>& p) {
        if (!ga_history) return;
        const Individual& best = findBest(p);
        int unplaced = 0;
        GASnapshot snap;
        snap.generation           = gen_idx;
        snap.best_container_count = static_cast<int>(best.objectives[0]);
        snap.best_avg_utilization = -best.objectives[1];
        snap.solution = Packer::decode(
            best.chromosome, residualCounts, itemTypes, seedContainers, unplaced, relaxed);
        ga_history->push_back(std::move(snap));
    };

    // Record generation 0 (initial population, before any evolution).
    recordSnapshot(0, pop);

    // ── Generational loop ────────────────────────────────────────────────────
    for (int gen = 0; gen < Config::GA_NGEN; ++gen) {

        // ── Generate offspring ───────────────────────────────────────────────
        // GA_MU parent pairs each produce 2 offspring, giving GA_LAMBDA total.
        // (Config enforces GA_LAMBDA == 2 × GA_MU via static_assert.)
        std::vector<Individual> offspring;
        offspring.reserve(Config::GA_LAMBDA);

        for (int pair = 0; pair < Config::GA_MU; ++pair) {
            // Select two parents via binary tournament.
            const Individual& p1 = GeneticOperators::tournamentSelect(pop, rng);
            const Individual& p2 = GeneticOperators::tournamentSelect(pop, rng);

            // Crossover with probability GA_CROSSOVER_RATE; otherwise clone.
            Individual c1, c2;
            if (unit(rng) < Config::GA_CROSSOVER_RATE) {
                c1 = GeneticOperators::crossover(p1, p2, rng);
                c2 = GeneticOperators::crossover(p2, p1, rng);
            } else {
                c1.chromosome = p1.chromosome;
                c2.chromosome = p2.chromosome;
            }

            // Mutation applied independently to each offspring.
            if (unit(rng) < Config::GA_MUTATION_RATE) GeneticOperators::mutate(c1, rng);
            if (unit(rng) < Config::GA_MUTATION_RATE) GeneticOperators::mutate(c2, rng);

            offspring.push_back(std::move(c1));
            offspring.push_back(std::move(c2));
        }

        // ── Evaluate offspring ───────────────────────────────────────────────
        for (Individual& ind : offspring) {
            evaluateFitness(ind, residualCounts, itemTypes, seedContainers, relaxed);
        }

        // ── Mu+Lambda selection: parents + offspring → next generation ───────
        pop = muLambdaSelect(std::move(pop), std::move(offspring));

        // ── Task 6.12: Stagnation detection ─────────────────────────────────
        // Find the minimum container count across rank-0 individuals.
        int current_best = INT_MAX;
        for (const Individual& ind : pop) {
            if (ind.rank == 0) {
                int cnt = static_cast<int>(ind.objectives[0]);
                if (cnt < current_best) current_best = cnt;
            }
        }

        if (current_best < best_f0) {
            best_f0    = current_best;
            stagnation = 0;
        } else {
            ++stagnation;
        }

        // Record snapshot for this generation if it falls on the interval.
        // Generation 0 was already recorded above. The final generation is
        // always recorded (the break below fires after this check).
        const bool is_last      = (stagnation >= Config::GA_MAX_STAGNATION)
                                || (gen == Config::GA_NGEN - 1);
        const bool on_interval  = ((gen + 1) % history_interval == 0);
        if (ga_history && (on_interval || is_last)) {
            recordSnapshot(gen + 1, pop);
        }

        if (stagnation >= Config::GA_MAX_STAGNATION) break;
    }

    return pop;
}

} // namespace NSGA2
