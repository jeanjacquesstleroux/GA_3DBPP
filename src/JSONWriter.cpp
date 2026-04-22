#include "JSONWriter.h"

#include <fstream>
#include <nlohmann/json.hpp>

// ── Shared helpers ────────────────────────────────────────────────────────────

static std::string orientationStr(Orientation o) {
    return (o == Orientation::Original) ? "Original" : "Rotated90";
}

// Serializes one AnimatedPlacedItem with orig_l/w/h looked up from item_types.
static nlohmann::json serializeAnimatedItem(const AnimatedPlacedItem&    ap,
                                            const std::vector<ItemType>& item_types)
{
    nlohmann::json j;
    j["item_type_index"] = ap.item_type_index;
    j["orientation"]     = orientationStr(ap.orientation);
    j["x"]  = ap.x;  j["y"]  = ap.y;  j["z"]  = ap.z;
    j["dx"] = ap.dx; j["dy"] = ap.dy; j["dz"] = ap.dz;
    const ItemType& t = item_types[ap.item_type_index];
    j["orig_l"] = t.l; j["orig_w"] = t.w; j["orig_h"] = t.h;
    j["phase"]           = ap.phase;
    j["placement_order"] = ap.placement_order;
    // layer_index of -1 means "no layer" (Phase 2 item) → serialize as JSON null.
    if (ap.layer_index < 0) j["layer_index"] = nullptr;
    else                    j["layer_index"] = ap.layer_index;
    return j;
}

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
            jitem["orientation"]     = orientationStr(p.orientation);
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

// ── writeAnimatedJSON ─────────────────────────────────────────────────────────

bool writeAnimatedJSON(const AnimatedSolution&      solution,
                       const std::vector<ItemType>& item_types,
                       const std::string&           path)
{
    nlohmann::json root;

    // ── metadata ──────────────────────────────────────────────────────────────
    double total_used = 0.0;
    double total_cap  = 0.0;
    for (const AnimatedContainer& c : solution.containers) {
        total_cap += static_cast<double>(c.L) * c.W * c.H;
        for (const AnimatedPlacedItem& ap : c.items) {
            total_used += static_cast<double>(ap.dx) * ap.dy * ap.dz;
        }
    }
    const double avg_util = (total_cap > 0.0)
                                ? (total_used / total_cap)
                                : 0.0;

    root["metadata"]["container_count"]        = static_cast<int>(solution.containers.size());
    root["metadata"]["avg_utilization"]        = avg_util;
    root["metadata"]["total_items"]            = solution.total_items;
    root["metadata"]["phase1_item_count"]      = solution.phase1_item_count;
    root["metadata"]["phase2_item_count"]      = solution.phase2_item_count;
    root["metadata"]["ga_generations_recorded"]= static_cast<int>(solution.ga_history.size());

    // ── containers (unified Phase 1 + Phase 2) ────────────────────────────────
    root["containers"] = nlohmann::json::array();

    for (int ci = 0; ci < static_cast<int>(solution.containers.size()); ++ci) {
        const AnimatedContainer& c = solution.containers[ci];

        nlohmann::json jc;
        jc["id"]        = ci;
        jc["dims"]["L"] = c.L;
        jc["dims"]["W"] = c.W;
        jc["dims"]["H"] = c.H;

        // layer_manifest — omit when empty (pure Phase 2 containers).
        if (!c.layer_manifest.empty()) {
            jc["layer_manifest"] = nlohmann::json::array();
            for (const LayerManifestEntry& lme : c.layer_manifest) {
                nlohmann::json jlme;
                jlme["layer_index"] = lme.layer_index;
                jlme["z_min"]       = lme.z_min;
                jlme["z_max"]       = lme.z_max;
                jlme["item_count"]  = lme.item_count;
                jlme["item_type_summary"] = nlohmann::json::array();
                for (const ItemTypeSummary& s : lme.item_type_summary) {
                    jlme["item_type_summary"].push_back(
                        {{"item_type_index", s.item_type_index}, {"count", s.count}});
                }
                jc["layer_manifest"].push_back(jlme);
            }
        }

        jc["items"] = nlohmann::json::array();
        for (const AnimatedPlacedItem& ap : c.items) {
            jc["items"].push_back(serializeAnimatedItem(ap, item_types));
        }

        root["containers"].push_back(jc);
    }

    // ── ga_history ────────────────────────────────────────────────────────────
    root["ga_history"] = nlohmann::json::array();

    for (const GAGenerationSnapshot& snap : solution.ga_history) {
        nlohmann::json jsnap;
        jsnap["generation"]           = snap.generation;
        jsnap["best_container_count"] = snap.best_container_count;
        jsnap["best_avg_utilization"] = snap.best_avg_utilization;
        jsnap["best_containers"]      = nlohmann::json::array();

        for (int ci = 0; ci < static_cast<int>(snap.containers.size()); ++ci) {
            const AnimatedContainer& c = snap.containers[ci];
            nlohmann::json jc;
            jc["id"]        = ci;
            jc["dims"]["L"] = c.L;
            jc["dims"]["W"] = c.W;
            jc["dims"]["H"] = c.H;
            jc["items"]     = nlohmann::json::array();
            for (const AnimatedPlacedItem& ap : c.items) {
                jc["items"].push_back(serializeAnimatedItem(ap, item_types));
            }
            jsnap["best_containers"].push_back(jc);
        }

        root["ga_history"].push_back(jsnap);
    }

    // ── write to disk ─────────────────────────────────────────────────────────
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << root.dump(2);
    return out.good();
}
