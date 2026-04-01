#include "JSONWriter.h"

#include <fstream>
#include <nlohmann/json.hpp>

bool writeJSON(const PackingSolution&       solution,
               const std::vector<ItemType>& item_types,
               const std::string&           path)
{
    nlohmann::json root;

    // ---- metadata -------------------------------------------------------
    root["metadata"]["container_count"] = static_cast<int>(solution.containers.size());
    root["metadata"]["avg_utilization"] = solution.avgUtilization();

    // ---- containers -----------------------------------------------------
    root["containers"] = nlohmann::json::array();

    for (int ci = 0; ci < static_cast<int>(solution.containers.size()); ++ci) {
        const Container& cont = solution.containers[ci];

        nlohmann::json jcont;
        jcont["id"]          = ci;
        jcont["dims"]["L"]   = cont.L;
        jcont["dims"]["W"]   = cont.W;
        jcont["dims"]["H"]   = cont.H;
        jcont["utilization"] = cont.utilization();

        // ---- placed items -----------------------------------------------
        jcont["items"] = nlohmann::json::array();

        for (const PlacedItem& p : cont.items) {
            nlohmann::json jitem;
            jitem["item_type_index"] = p.item_type_index;
            jitem["orientation"]     = (p.orientation == Orientation::Original)
                                           ? "Original"
                                           : "Rotated90";
            jitem["x"]  = p.x;
            jitem["y"]  = p.y;
            jitem["z"]  = p.z;
            jitem["dx"] = p.dx;
            jitem["dy"] = p.dy;
            jitem["dz"] = p.dz;

            // Embed the original (unrotated) dimensions for viewer tooltips.
            const ItemType& t    = item_types[p.item_type_index];
            jitem["orig_l"] = t.l;
            jitem["orig_w"] = t.w;
            jitem["orig_h"] = t.h;

            jcont["items"].push_back(jitem);
        }

        root["containers"].push_back(jcont);
    }

    // ---- write to disk --------------------------------------------------
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }

    out << root.dump(2);   // 2-space indentation for human-readable output
    return out.good();
}
