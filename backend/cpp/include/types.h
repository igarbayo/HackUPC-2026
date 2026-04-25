#pragma once
#include <string>
#include <vector>
#include <cstdint>

using Family = std::string;
using Tick   = std::uint64_t;
using BoxId  = std::string;
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
    BoxId       box_id       = {};
    int         pallet_id    = -1;  // pallet slot index; -1 if not applicable
    Family      family       = {};
};

// --- Per-tick state snapshots ---

struct BoxSnap {
    BoxId    id;
    Family   family;
    Tick     arrival_tick = 0;
    Position position;
};

struct ShuttleSnap {
    int         y_level      = 1;
    Position    position;
    std::string phase;          // "Idle"|"MovingToPickup"|"Loading"|"MovingToDrop"|"Unloading"
    bool        is_carrying  = false;
    BoxId       carried_box_id;
    Family      carried_box_family;
    std::vector<BoxSnap> floor_boxes;
};

struct AisleSnap {
    std::vector<ShuttleSnap> shuttles;
};

struct PalletSnap {
    int    slot          = 0;   // 0..3
    Family family;
    int    placed_count  = 0;
    int    reserved_count = 0;
    std::vector<BoxSnap> boxes;
};

struct TickSnapshot {
    Tick     tick = 0;
    AisleSnap aisle;
    std::vector<PalletSnap> pallets;
    std::vector<Event> events;  // events emitted during this tick; last snap carries "done"
};
