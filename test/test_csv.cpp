#include <gtest/gtest.h>
#include "CSVReader.h"

// All paths are relative to the CTest working directory (build/debug).

// -------------------------------------------------------------------------
// Basic file handling
// -------------------------------------------------------------------------

TEST(CSVReader, MissingFileReturnsEmpty) {
    auto orders = CSVReader("nonexistent.csv").read();
    EXPECT_TRUE(orders.empty());
}

TEST(CSVReader, HeaderOnlyReturnsEmpty) {
    // empty_data.csv contains only the header row — no data rows.
    auto orders = CSVReader("../../data/test_fixtures/empty_data.csv").read();
    EXPECT_TRUE(orders.empty());
}

// -------------------------------------------------------------------------
// Valid parsing
// -------------------------------------------------------------------------

TEST(CSVReader, ValidFileOrderCount) {
    auto orders = CSVReader("../../data/test_fixtures/valid_small.csv").read();
    // Two distinct order IDs: 1001 and 1002
    EXPECT_EQ(static_cast<int>(orders.size()), 2);
}

TEST(CSVReader, ValidFileOrder1001ItemCount) {
    auto orders = CSVReader("../../data/test_fixtures/valid_small.csv").read();
    ASSERT_TRUE(orders.count(1001) > 0);
    EXPECT_EQ(static_cast<int>(orders.at(1001).size()), 2);
}

TEST(CSVReader, ValidFileFirstItemFields) {
    auto orders = CSVReader("../../data/test_fixtures/valid_small.csv").read();
    ASSERT_TRUE(orders.count(1001) > 0);
    const ItemType& item = orders.at(1001)[0];
    EXPECT_EQ(item.l,  400);
    EXPECT_EQ(item.w,  300);
    EXPECT_EQ(item.h,  200);
    EXPECT_EQ(item.q,    2);
    EXPECT_EQ(item.m,   13);  // std::lround(12.5) = 13 (rounds to even → 12? No: lround rounds away from zero → 13)
}

TEST(CSVReader, ValidFileOrder1002ItemCount) {
    auto orders = CSVReader("../../data/test_fixtures/valid_small.csv").read();
    ASSERT_TRUE(orders.count(1002) > 0);
    EXPECT_EQ(static_cast<int>(orders.at(1002).size()), 1);
}

// -------------------------------------------------------------------------
// Validation — non-positive dimensions
// -------------------------------------------------------------------------

TEST(CSVReader, SkipsZeroDimension) {
    // malformed_dims.csv has 4 data rows for order 2001:
    //   row 1: valid (400x300x200)
    //   row 2: l=0  — invalid, skipped
    //   row 3: w=-1 — invalid, skipped
    //   row 4: valid (200x200x200)
    // Only 2 valid items should be loaded.
    auto orders = CSVReader("../../data/test_fixtures/malformed_dims.csv").read();
    ASSERT_TRUE(orders.count(2001) > 0);
    EXPECT_EQ(static_cast<int>(orders.at(2001).size()), 2);
}

TEST(CSVReader, ValidRowsStillLoadedAfterBadRow) {
    // Verify the valid rows are the ones that actually loaded.
    auto orders = CSVReader("../../data/test_fixtures/malformed_dims.csv").read();
    ASSERT_TRUE(orders.count(2001) > 0);
    const auto& items = orders.at(2001);
    EXPECT_EQ(items[0].l, 400);  // first valid row
    EXPECT_EQ(items[1].l, 200);  // fourth row — zero/negative rows were skipped
}

// -------------------------------------------------------------------------
// Validation — missing columns
// -------------------------------------------------------------------------

TEST(CSVReader, SkipsMissingColumns) {
    // missing_cols.csv has 3 data rows for order 3001:
    //   row 1: valid (7 fields)
    //   row 2: only 5 fields — skipped
    //   row 3: valid (7 fields)
    // Only 2 valid items should be loaded.
    auto orders = CSVReader("../../data/test_fixtures/missing_cols.csv").read();
    ASSERT_TRUE(orders.count(3001) > 0);
    EXPECT_EQ(static_cast<int>(orders.at(3001).size()), 2);
}

// -------------------------------------------------------------------------
// Large real-world dataset (Dataset10000.csv)
// -------------------------------------------------------------------------

TEST(CSVReader, LargeDatasetOrderCount) {
    // 9456 order IDs exist in the raw file, but 13 rows have zero dimensions
    // and are skipped by validation, leaving 9433 orders with at least one valid item.
    auto orders = CSVReader("../../data/Dataset10000.csv").read();
    EXPECT_EQ(static_cast<int>(orders.size()), 9433);
}

TEST(CSVReader, LargeDatasetOrder95125Fields) {
    auto orders = CSVReader("../../data/Dataset10000.csv").read();
    ASSERT_TRUE(orders.count(95125) > 0);
    const ItemType& item = orders.at(95125)[0];
    EXPECT_EQ(item.l, 211);
    EXPECT_EQ(item.w, 230);
    EXPECT_EQ(item.h, 264);
    EXPECT_EQ(item.q,   2);
    EXPECT_EQ(item.m,  13);  // std::lround(12.569) = 13
}
