#include "Hausdorff.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// Directed Hausdorff distance from point set `from` to point set `to`.
// For each point in `from`, finds the nearest point in `to`.
// Returns the maximum of those nearest distances.
double directed(const std::vector<std::array<int, 2>>& from,
                const std::vector<std::array<int, 2>>& to)
{
    double max_of_mins = 0.0;

    for (const auto& a : from) {
        double min_dist = std::numeric_limits<double>::infinity();

        for (const auto& b : to) {
            const double dx = static_cast<double>(a[0] - b[0]);
            const double dy = static_cast<double>(a[1] - b[1]);
            min_dist = std::min(min_dist, std::sqrt(dx * dx + dy * dy));
        }

        max_of_mins = std::max(max_of_mins, min_dist);
    }

    return max_of_mins;
}

} // anonymous namespace

namespace Hausdorff {

double distance(const std::vector<std::array<int, 2>>& A,
                const std::vector<std::array<int, 2>>& B)
{
    if (A.empty() || B.empty()) {
        return 0.0;
    }
    return std::max(directed(A, B), directed(B, A));
}

} // namespace Hausdorff
