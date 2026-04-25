#pragma once
#include <string>
#include <cstdint>

using Family = std::string;
using Tick   = std::uint64_t;
using BoxId  = std::uint64_t;
using SlotId = std::uint64_t;

struct Position {
    int x    = 0;   // -1=port/head; 0..length-1 storage (0-based)
    int y    = 1;   // height level: 1..numY
    int z    = 1;   // depth: 1 (front/accessible) or 2 (back)
    int side = 1;   // 1=Left, 2=Right
    bool operator==(const Position& o) const {
        return x == o.x && y == o.y && z == o.z && side == o.side;
    }
    bool operator!=(const Position& o) const { return !(*this == o); }
};

// A single timestamped simulation event emitted to the event log.
// type values: "box_arrived", "box_stored", "box_retrieved",
//              "box_on_pallet", "pallet_dispatched", "done"
struct Event {
    std::string type;
    Tick        logical_time = 0;
    BoxId       box_id       = 0;
    int         pallet_id    = -1;  // pallet slot index; -1 if not applicable
    Family      family       = {};
};
