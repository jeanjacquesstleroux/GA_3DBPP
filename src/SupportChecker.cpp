#include "SupportChecker.h"

#include <algorithm>
#include <array>

SupportChecker::SupportChecker(int vertex_inset) : inset_(vertex_inset) {}

bool SupportChecker::isSupported(const PlacedItem&              item,
                                  const std::vector<PlacedItem>& others) const
{
    // Items resting on the container floor need no further checking.
    if (item.z == 0) {
        return true;
    }

    // Collect supporters: items whose top face is exactly at item's bottom face.
    std::vector<PlacedItem> supporters;
    for (const PlacedItem& other : others) {
        if (other.z + other.dz == item.z) {
            supporters.push_back(other);
        }
    }

    if (supporters.empty()) {
        return false;  // floating — nothing below
    }

    const int    verts    = countSupportedVertices(item, supporters);
    const double coverage = baseAreaCoverage(item, supporters);

    // Evaluate tiers from most lenient area requirement to most strict.
    if (verts >= Config::SUPPORT_TIER1_VERTS && coverage >= Config::SUPPORT_TIER1_AREA) {
        return true;
    }
    if (verts >= Config::SUPPORT_TIER2_VERTS && coverage >= Config::SUPPORT_TIER2_AREA) {
        return true;
    }
    if (verts >= Config::SUPPORT_TIER3_VERTS && coverage >= Config::SUPPORT_TIER3_AREA) {
        return true;
    }
    return false;
}

int SupportChecker::countSupportedVertices(
    const PlacedItem&              item,
    const std::vector<PlacedItem>& supporters) const
{
    // The 4 base vertices of item's bottom face, each pulled inward by inset_.
    const std::array<std::array<int, 2>, 4> verts = {{
        {item.x + inset_,         item.y + inset_        },  // near-left
        {item.x + item.dx - inset_, item.y + inset_      },  // near-right
        {item.x + inset_,         item.y + item.dy - inset_},// far-left
        {item.x + item.dx - inset_, item.y + item.dy - inset_} // far-right
    }};

    int count = 0;
    for (const auto& v : verts) {
        const int vx = v[0];
        const int vy = v[1];
        for (const PlacedItem& s : supporters) {
            if (vx >= s.x && vx <= s.x + s.dx &&
                vy >= s.y && vy <= s.y + s.dy) {
                ++count;
                break;  // vertex supported — no need to check remaining supporters
            }
        }
    }
    return count;
}

double SupportChecker::baseAreaCoverage(
    const PlacedItem&              item,
    const std::vector<PlacedItem>& supporters) const
{
    const int item_area = item.dx * item.dy;
    if (item_area == 0) {
        return 0.0;
    }

    int covered = 0;
    for (const PlacedItem& s : supporters) {
        // 1D interval overlap on X and Y, clamped to zero if no overlap.
        const int ox = std::max(0, std::min(item.x + item.dx, s.x + s.dx)
                                 - std::max(item.x, s.x));
        const int oy = std::max(0, std::min(item.y + item.dy, s.y + s.dy)
                                 - std::max(item.y, s.y));
        covered += ox * oy;
    }

    return static_cast<double>(covered) / static_cast<double>(item_area);
}
