#include <gtest/gtest.h>

TEST(SmokeTest, TrueIsTrue) {
    EXPECT_TRUE(true);
}

TEST(SmokeTest, OnePlusOneIsTwo) {
    EXPECT_EQ(1 + 1, 2);
}
