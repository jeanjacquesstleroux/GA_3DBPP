#include "CenterOfMass.h"

#include <cmath>

std::array<double, 3> CenterOfMass::compute(
    const std::vector<PlacedItem>& items,
    const std::vector<ItemType>&   item_types)
{
    double total_mass = 0.0;
    double sum_x      = 0.0;
    double sum_y      = 0.0;
    double sum_z      = 0.0;

    for (const PlacedItem& p : items) {
        const double mass = static_cast<double>(item_types[p.item_type_index].m);
        // Each item's contribution is its mass × its geometric centre on each axis.
        // The centre of a placed box is (x + dx/2, y + dy/2, z + dz/2).
        sum_x      += mass * (p.x + p.dx / 2.0);
        sum_y      += mass * (p.y + p.dy / 2.0);
        sum_z      += mass * (p.z + p.dz / 2.0);
        total_mass += mass;
    }

    if (total_mass == 0.0) {
        return {0.0, 0.0, 0.0};
    }

    return {sum_x / total_mass, sum_y / total_mass, sum_z / total_mass};
}

bool CenterOfMass::isStable(const std::array<double, 3>& com,
                             const Container&              container)
{
    const double center_x = container.L / 2.0;
    const double center_y = container.W / 2.0;

    return std::abs(com[0] - center_x) <= Config::COM_MAX_DEVIATION &&
           std::abs(com[1] - center_y) <= Config::COM_MAX_DEVIATION;
}
