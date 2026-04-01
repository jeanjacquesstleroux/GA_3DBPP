#include "CSVReader.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

CSVReader::CSVReader(const std::string& path) : path_(path) {}

std::map<int, std::vector<ItemType>> CSVReader::read() const {
    std::map<int, std::vector<ItemType>> orders;

    std::ifstream file(path_);
    if (!file.is_open()) {
        return orders;
    }

    // Discard the header row: "Order,Product,Quantity,Length,Width,Height,Weight"
    std::string line;
    std::getline(file, line);

    int line_number = 1;  // header was line 1
    while (std::getline(file, line)) {
        ++line_number;

        // ---- parse 7 comma-separated fields -----------------------------
        std::istringstream ss(line);
        std::string fields[7];
        int parsed = 0;
        while (parsed < 7 && std::getline(ss, fields[parsed], ',')) {
            ++parsed;
        }

        if (parsed < 7) {
            std::cerr << "[CSVReader] line " << line_number
                      << ": expected 7 fields, got " << parsed << " — skipping.\n";
            continue;
        }

        // ---- convert strings to numbers ---------------------------------
        int    orderID = 0;
        int    qty     = 0;
        int    len     = 0;
        int    wid     = 0;
        int    hgt     = 0;
        int    mass    = 0;

        try {
            orderID = std::stoi(fields[0]);
            // fields[1] = Product ID — not stored in ItemType
            qty     = std::stoi(fields[2]);
            len     = std::stoi(fields[3]);
            wid     = std::stoi(fields[4]);
            hgt     = std::stoi(fields[5]);
            mass    = static_cast<int>(std::lround(std::stod(fields[6])));
        } catch (const std::exception&) {
            std::cerr << "[CSVReader] line " << line_number
                      << ": non-numeric field — skipping.\n";
            continue;
        }

        // ---- validate dimensions and quantity ---------------------------
        if (len <= 0 || wid <= 0 || hgt <= 0 || qty <= 0) {
            std::cerr << "[CSVReader] line " << line_number
                      << ": non-positive value (l=" << len
                      << " w=" << wid << " h=" << hgt << " q=" << qty
                      << ") — skipping.\n";
            continue;
        }

        ItemType item;
        item.l = len;
        item.w = wid;
        item.h = hgt;
        item.m = mass;
        item.q = qty;

        orders[orderID].push_back(item);
    }

    return orders;
}
