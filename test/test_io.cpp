#include <gtest/gtest.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "BRReader.h"
#include "JSONWriter.h"

// -------------------------------------------------------------------------
// BRReader
// -------------------------------------------------------------------------

TEST(BRReader, LoadsCorrectProblemCount) {
    auto problems = loadBRFile("../../data/br_benchmark/thpack1.txt");
    EXPECT_EQ(static_cast<int>(problems.size()), 100);
}

TEST(BRReader, Problem1ContainerDimensions) {
    auto problems = loadBRFile("../../data/br_benchmark/thpack1.txt");
    ASSERT_FALSE(problems.empty());
    EXPECT_EQ(problems[0].L, 587);
    EXPECT_EQ(problems[0].W, 233);
    EXPECT_EQ(problems[0].H, 220);
}

TEST(BRReader, Problem1ItemTypes) {
    auto problems = loadBRFile("../../data/br_benchmark/thpack1.txt");
    ASSERT_FALSE(problems.empty());
    ASSERT_EQ(static_cast<int>(problems[0].items.size()), 3);

    // First item type: l=108, w=76, h=30, q=40
    EXPECT_EQ(problems[0].items[0].l,  108);
    EXPECT_EQ(problems[0].items[0].w,   76);
    EXPECT_EQ(problems[0].items[0].h,   30);
    EXPECT_EQ(problems[0].items[0].q,   40);
}

TEST(BRReader, MissingFileReturnsEmpty) {
    auto problems = loadBRFile("nonexistent_file.txt");
    EXPECT_TRUE(problems.empty());
}

// -------------------------------------------------------------------------
// JSONWriter — round-trip tests
// -------------------------------------------------------------------------

// Builds a minimal PackingSolution with known values for use in all JSON tests.
static PackingSolution makeSolution(std::vector<ItemType>& types_out)
{
    ItemType t;
    t.l = 400;  t.w = 300;  t.h = 200;  t.m = 5;  t.q = 10;
    types_out.push_back(t);

    PlacedItem p;
    p.item_type_index = 0;
    p.orientation     = Orientation::Original;
    p.x = 10;  p.y = 20;  p.z = 30;
    p.dx = 400;  p.dy = 300;  p.dz = 200;

    Container c;
    c.items.push_back(p);

    PackingSolution sol;
    sol.containers.push_back(c);
    return sol;
}

TEST(JSONWriter, WriteSucceeds)
{
    std::vector<ItemType> types;
    PackingSolution sol = makeSolution(types);
    EXPECT_TRUE(writeJSON(sol, types, "test_roundtrip.json"));
}

TEST(JSONWriter, MissingDirectoryReturnsFalse)
{
    std::vector<ItemType> types;
    PackingSolution sol = makeSolution(types);
    EXPECT_FALSE(writeJSON(sol, types, "/nonexistent_dir/out.json"));
}

TEST(JSONWriter, RoundTripMetadata)
{
    std::vector<ItemType> types;
    PackingSolution sol = makeSolution(types);
    ASSERT_TRUE(writeJSON(sol, types, "test_roundtrip.json"));

    std::ifstream f("test_roundtrip.json");
    ASSERT_TRUE(f.is_open());
    nlohmann::json root = nlohmann::json::parse(f);

    EXPECT_EQ(root.at("metadata").at("container_count").get<int>(), 1);
}

TEST(JSONWriter, RoundTripContainerDims)
{
    std::vector<ItemType> types;
    PackingSolution sol = makeSolution(types);
    ASSERT_TRUE(writeJSON(sol, types, "test_roundtrip.json"));

    std::ifstream f("test_roundtrip.json");
    nlohmann::json root = nlohmann::json::parse(f);

    const auto& jcont = root.at("containers").at(0);
    EXPECT_EQ(jcont.at("dims").at("L").get<int>(), Config::PALLET_L);
    EXPECT_EQ(jcont.at("dims").at("W").get<int>(), Config::PALLET_W);
    EXPECT_EQ(jcont.at("dims").at("H").get<int>(), Config::PALLET_H);
}

TEST(JSONWriter, RoundTripPlacedItem)
{
    std::vector<ItemType> types;
    PackingSolution sol = makeSolution(types);
    ASSERT_TRUE(writeJSON(sol, types, "test_roundtrip.json"));

    std::ifstream f("test_roundtrip.json");
    nlohmann::json root = nlohmann::json::parse(f);

    const auto& jitem = root.at("containers").at(0).at("items").at(0);
    EXPECT_EQ(jitem.at("item_type_index").get<int>(),  0);
    EXPECT_EQ(jitem.at("orientation").get<std::string>(), "Original");
    EXPECT_EQ(jitem.at("x").get<int>(),  10);
    EXPECT_EQ(jitem.at("y").get<int>(),  20);
    EXPECT_EQ(jitem.at("z").get<int>(),  30);
    EXPECT_EQ(jitem.at("dx").get<int>(), 400);
    EXPECT_EQ(jitem.at("dy").get<int>(), 300);
    EXPECT_EQ(jitem.at("dz").get<int>(), 200);
    EXPECT_EQ(jitem.at("orig_l").get<int>(), 400);
    EXPECT_EQ(jitem.at("orig_w").get<int>(), 300);
    EXPECT_EQ(jitem.at("orig_h").get<int>(), 200);
}
