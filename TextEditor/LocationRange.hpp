#pragma once
#include "TE_API.hpp"
#include "Location.hpp"

struct TEXTEDITOR_API LocationRange {
    Location start;
    Location end;

    LocationRange() = default;
    LocationRange(Location s, Location e) : start(s), end(e) {}
};
