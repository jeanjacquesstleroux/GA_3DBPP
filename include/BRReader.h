#pragma once

#include <string>
#include <vector>
#include "Types.h"

// BRProblem holds one packing problem loaded from a BR benchmark thpack file.
// It stores the container dimensions for that problem and the list of item types.
// Note: the BR benchmark uses its own container sizes (truck containers),
// which differ from the Euro pallet dimensions in Config.h.
struct BRProblem {
    int L = 0;  // container length
    int W = 0;  // container width
    int H = 0;  // container height
    std::vector<ItemType> items;
};

// Parses a BR benchmark thpack file and returns all problems it contains.
// Returns an empty vector if the file cannot be opened.
[[nodiscard]] std::vector<BRProblem> loadBRFile(const std::string& path);
