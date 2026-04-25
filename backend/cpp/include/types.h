#pragma once
#include <string>
#include <cstdint>

using Family = std::string;
using Tick   = std::uint64_t;
using BoxId  = std::uint64_t;
using SlotId = std::uint64_t;

struct Position {
    int x = 0;
    int y = 0;
    bool operator==(const Position& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Position& o) const { return !(*this == o); }
};
