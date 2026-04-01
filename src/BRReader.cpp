#include "BRReader.h"

#include <fstream>
#include <sstream>

std::vector<BRProblem> loadBRFile(const std::string& path) {
    std::vector<BRProblem> problems;

    std::ifstream file(path);
    if (!file.is_open()) {
        return problems;  // return empty vector on failure
    }

    int numProblems = 0;
    file >> numProblems;
    problems.reserve(numProblems);

    for (int i = 0; i < numProblems; ++i) {
        BRProblem prob;

        // Skip the "problem_index  order_id" line — we don't need either value.
        int dummy1, dummy2;
        file >> dummy1 >> dummy2;

        // Read container dimensions.
        file >> prob.L >> prob.W >> prob.H;

        // Read item types.
        int numItems = 0;
        file >> numItems;
        prob.items.reserve(numItems);

        for (int j = 0; j < numItems; ++j) {
            ItemType item;
            int idx, rotL, rotW, rotH;  // fields we read but don't use
            file >> idx >> item.l >> rotL >> item.w >> rotW >> item.h >> rotH >> item.q;
            prob.items.push_back(item);
        }

        problems.push_back(prob);
    }

    return problems;
}
