#pragma once
#include <string>
#include <cstdint>

using Family = std::string;
using Tick   = std::uint64_t;
using BoxId  = std::string;
using SlotId = std::uint64_t;

struct Position {
    int x = 0;
    int y = 0;
    bool operator==(const Position& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Position& o) const { return !(*this == o); }
};

// A single timestamped simulation event emitted to the event log.
// type values: "box_arrived", "box_stored", "box_retrieved",
//              "box_on_pallet", "pallet_dispatched", "done"
struct Event {
    std::string type;
    Tick        logical_time = 0;
    BoxId       box_id       = {};
    int         pallet_id    = -1;  // pallet slot index; -1 if not applicable
    Family      family       = {};
};
