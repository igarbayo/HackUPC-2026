#pragma once
#include "types.h"
#include "Box.h"
#include "Slot.h"
#include "Shuttle.h"
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

class Aisle {
public:
    struct Metadata {
        std::unordered_map<Family, int>      countByFamily;
        std::unordered_map<Family, int>      reservedByFamily;
        std::unordered_map<Family, Position> nearestByFamily;
        int                                  freeSlots        = 0;
        int                                  pendingInputs    = 0;
        int                                  pendingOutputs   = 0;
        int                                  readyOutputCount = 0;
        int                                  activeShuttles   = 0;
    };

    struct Instruction {
        enum class Kind { Input, Output };
        Kind                  kind;
        std::optional<Family> requestedFam;  // for Output
        Tick                  issuedAt = 0;
        int                   priority = 0;
    };

    // length: X storage slots (0..length-1).
    // numY:   height levels (1..numY); one shuttle per level.
    // numSides: 1 (Left only) or 2 (Left + Right).
    // port:   head position shared by all shuttles (port.x used; y/z/side per-shuttle).
    Aisle(int length, int numY, int numSides, Position port);

    void                input(Box newBox);
    void                requestOutput(Family f);
    std::optional<Box>  collectReadyOutput(Family f);

    void                ordenarInstrucciones();

    Metadata            metadata()    const;
    Position            port()        const;

    void                tick();
    void                setEventLog(std::vector<Event>* log);

    void                assignNextTo(Shuttle& s);

    void                notifyBoxPlaced(const Box& b, Position where);
    void                notifyBoxTaken (const Box& b, Position where);
    void                notifyOutputReady(Box b);
    std::optional<Box>  takeFromInputBuffer();

    Slot&               slotAt(Position pos);
    const Slot&         slotAt(Position pos) const;

    // Find the best accessible box of the given family for a specific shuttle.
    // Cost = dist(shuttle→box) + dist(box→port). Ties broken by largest x (prefer freeing far slots).
    std::optional<Position> findBestBoxForShuttle(const Family& f, Position shuttlePos) const;

    // Find best free slot at (side, y) for input placement.
    // preferNear=true  → smallest x (hot family)
    // preferNear=false → largest x  (cold family)
    std::optional<Position> findFreeSlot(int side, int y, bool preferNear) const;

private:
    int                      length_;
    int                      numY_;
    int                      numSides_;
    Position                 port_;

    // slots_[side-1][y-1][x]  (side 1-based, y 1-based, x 0-based)
    std::vector<std::vector<std::vector<Slot>>> slots_;

    std::vector<Shuttle>     shuttles_;         // one per Y level (shuttles_[y-1])
    std::vector<Instruction> instructionQueue_;
    Tick                     currentTick_ = 0;
    Metadata                 meta_;

    using LevelKey = std::pair<int,int>;  // {side, y}
    // freeZ1_[{side,y}] = set of x where z1 is free (can accept a new box at z1)
    std::map<LevelKey, std::set<int>> freeZ1_;

    std::unordered_map<Family, std::queue<Box>> readyOutputs_;
    std::queue<Box>          pendingInputBoxes_;
    std::unordered_map<Family, int> outputReservedByFamily_;
    std::vector<Event>*             eventLog_ = nullptr;

    void assignInstructions();
    void updateMeta();
};
