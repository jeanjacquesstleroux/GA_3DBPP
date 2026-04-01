#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "BinPacker v0.1 — build successful!\n";

    nlohmann::json j;
    j["project"] = "BinPacker";
    j["status"] = "compiles";
    std::cout << "JSON test: " << j.dump(2) << "\n";

    return 0;
}
