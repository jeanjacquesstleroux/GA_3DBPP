#include <gtest/gtest.h>
#include "Types.h"
#include "Config.h"

// -------------------------------------------------------------------------
// ItemType
// -------------------------------------------------------------------------

TEST(ItemType, DefaultFields) {
    ItemType item;
    EXPECT_EQ(item.l, 0);
    EXPECT_EQ(item.w, 0);
    EXPECT_EQ(item.h, 0);
    EXPECT_EQ(item.m, 0);
    EXPECT_EQ(item.q, 0);
}

TEST(ItemType, VolumeAndBaseArea) {
    ItemType item;
    item.l = 100; item.w = 80; item.h = 50;
    EXPECT_EQ(item.volume(),   100 * 80 * 50);
    EXPECT_EQ(item.baseArea(), 100 * 80);
}

// -------------------------------------------------------------------------
// PlacedItem
// -------------------------------------------------------------------------

TEST(PlacedItem, DefaultFields) {
    PlacedItem p;
    EXPECT_EQ(p.item_type_index, 0);
    EXPECT_EQ(p.orientation, Orientation::Original);
    EXPECT_EQ(p.x, 0); EXPECT_EQ(p.y, 0); EXPECT_EQ(p.z, 0);
    EXPECT_EQ(p.dx, 0); EXPECT_EQ(p.dy, 0); EXPECT_EQ(p.dz, 0);
}

TEST(PlacedItem, ExtentAndCenter) {
    PlacedItem p;
    p.x = 10; p.y = 20; p.z = 30;
    p.dx = 100; p.dy = 80; p.dz = 50;
    EXPECT_EQ(p.extent(), (std::array<int,3>{110, 100, 80}));
    EXPECT_EQ(p.center(), (std::array<int,3>{ 60,  60, 55}));
}

// -------------------------------------------------------------------------
// Container
// -------------------------------------------------------------------------

TEST(Container, DefaultDimensions) {
    Container c;
    EXPECT_EQ(c.L, Config::PALLET_L);
    EXPECT_EQ(c.W, Config::PALLET_W);
    EXPECT_EQ(c.H, Config::PALLET_H);
    EXPECT_TRUE(c.items.empty());
}

TEST(Container, UtilizationEmpty) {
    Container c;
    EXPECT_DOUBLE_EQ(c.utilization(), 0.0);
}

TEST(Container, UtilizationAndTotalWeight) {
    // One item type: 600x400x700mm, mass 10
    std::vector<ItemType> types(1);
    types[0] = {600, 400, 700, 10, 1};

    // Place one box that exactly fills half the container volume
    PlacedItem p;
    p.item_type_index = 0;
    p.dx = 600; p.dy = 400; p.dz = 700;

    Container c;
    c.items.push_back(p);

    // Placed volume = 600*400*700 = 168,000,000
    // Container volume = 1200*800*1400 = 1,344,000,000
    // Utilization = 168,000,000 / 1,344,000,000 = 0.125
    EXPECT_DOUBLE_EQ(c.utilization(), 0.125);
    EXPECT_EQ(c.totalWeight(types), 10);
}

// -------------------------------------------------------------------------
// PackingSolution
// -------------------------------------------------------------------------

TEST(PackingSolution, EmptyReturnsZero) {
    PackingSolution sol;
    EXPECT_TRUE(sol.containers.empty());
    EXPECT_DOUBLE_EQ(sol.avgUtilization(), 0.0);
}

// -------------------------------------------------------------------------
// ExtremePoint
// -------------------------------------------------------------------------

TEST(ExtremePoint, DefaultFields) {
    ExtremePoint ep;
    EXPECT_EQ(ep.x, 0);
    EXPECT_EQ(ep.y, 0);
    EXPECT_EQ(ep.z, 0);
}

// -------------------------------------------------------------------------
// Layer and Block
// -------------------------------------------------------------------------

TEST(Layer, DefaultFields) {
    Layer layer;
    EXPECT_EQ(layer.item_type_index, 0);
    EXPECT_EQ(layer.type, LayerType::Full);
    EXPECT_EQ(layer.item_count, 0);
    EXPECT_EQ(layer.height, 0);
}

TEST(Block, DefaultFields) {
    Block block;
    EXPECT_TRUE(block.layers.empty());
    EXPECT_EQ(block.z_base, 0);
}

// -------------------------------------------------------------------------
// Individual
// -------------------------------------------------------------------------

TEST(Individual, DefaultFields) {
    Individual ind;
    EXPECT_TRUE(ind.chromosome.empty());
    EXPECT_TRUE(ind.objectives.empty());
    EXPECT_EQ(ind.rank, 0);
    EXPECT_DOUBLE_EQ(ind.crowding_distance, 0.0);
}

// -------------------------------------------------------------------------
// Config constants
// -------------------------------------------------------------------------

TEST(Config, PalletDimensions) {
    EXPECT_EQ(Config::PALLET_L, 1200);
    EXPECT_EQ(Config::PALLET_W,  800);
    EXPECT_EQ(Config::PALLET_H, 1400);
}

TEST(Config, GAParameters) {
    EXPECT_EQ(Config::GA_POPULATION, 100);
    EXPECT_EQ(Config::GA_MU,          15);
    EXPECT_EQ(Config::GA_LAMBDA,       30);
    EXPECT_DOUBLE_EQ(Config::GA_CROSSOVER_RATE, 0.5);
    EXPECT_DOUBLE_EQ(Config::GA_MUTATION_RATE,  0.2);
    EXPECT_EQ(Config::GA_NGEN,          30);
    EXPECT_EQ(Config::GA_MAX_STAGNATION, 5);
}
