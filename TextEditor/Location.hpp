#pragma once
#include "TE_API.hpp"

struct TEXTEDITOR_API Location {
    int row;
    int col;

    Location(int r = 0, int c = 0) : row(r), col(c) {}

    bool operator==(const Location& other) const {
        return row == other.row && col == other.col;
    }

    bool operator<(const Location& other) const {
        return (row < other.row) || (row == other.row && col < other.col);
    }
};
