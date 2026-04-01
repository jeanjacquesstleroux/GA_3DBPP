#pragma once

#include <map>
#include <string>
#include <vector>
#include "Types.h"

// Parses an industrial order CSV file with columns:
//   Order, Product, Quantity, Length, Width, Height, Weight
//
// Validation rules (applied per row):
//   - Rows with fewer than 7 comma-separated fields are skipped.
//   - Rows where any dimension (l, w, h) or quantity is <= 0 are skipped.
//   - Rows where any numeric field is non-parseable are skipped.
//   All skipped rows emit a warning to stderr with the line number and reason.
//
// Weight (double kg) is rounded to the nearest integer and stored in ItemType::m.
// Returns an empty map if the file cannot be opened.
class CSVReader {
public:
    // Binds the reader to the given file path.
    // The path is stored but the file is not opened until read() is called.
    explicit CSVReader(const std::string& path);

    // Parses the file and returns all valid orders grouped by Order ID.
    [[nodiscard]] std::map<int, std::vector<ItemType>> read() const;

private:
    std::string path_;
};
