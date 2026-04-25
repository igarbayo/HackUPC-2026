#pragma once
#include "types.h"
#include "Box.h"
#include "Slot.h"
#include "Shuttle.h"
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>
// types.h defines Event; Aisle stores a pointer to the shared event log.

class Aisle {
public:
    struct Metadata {
        std::unordered_map<Family, int>      countByFamily;
        std::unordered_map<Family, int>      reservedByFamily;  // pending output instructions
        std::unordered_map<Family, Position> nearestByFamily;   // nearest to port
        int                                  freeSlots        = 0;
        int                                  pendingInputs    = 0;
        int                                  pendingOutputs   = 0;
        int                                  readyOutputCount = 0; // boxes at port ready for robot
        int                                  activeShuttles   = 0; // shuttles not idle
    };

    struct Instruction {
        enum class Kind { Input, Output };
        Kind                  kind;
        std::optional<Box>    incomingBox;   // for Input
        std::optional<Family> requestedFam;  // for Output
        Tick                  issuedAt = 0;
        int                   priority = 0;  // higher = more urgent
    };

    // length: number of storage slots (x=0..length-1)
    // port: single head position shared by input and output (e.g. Position{-1,0})
    Aisle(int length, int numShuttles, Position port);

    void                input(Box newBox);                 // belt -> aisle
    void                requestOutput(Family f);           // robot -> aisle (non-blocking)
    std::optional<Box>  collectReadyOutput(Family f);      // robot collects if available

    void                ordenarInstrucciones();            // recalculates priorities

    Metadata            metadata()    const;
    Position            port()        const;

    void                tick();

    void                setEventLog(std::vector<Event>* log);

    // Used by Shuttle to update free-slot index and metadata when placing/taking from storage slots
    void                notifyBoxPlaced(const Box& b, Position where);
    void                notifyBoxTaken (const Box& b, Position where);
    // Used by Shuttle when dropping box at port
    void                notifyOutputReady(Box b);
    // Used by Shuttle (input mission) to get the next box from the input buffer
    std::optional<Box>  takeFromInputBuffer();

    // Slot access by position (storage slots only, indexed by x from 0 to length-1)
    Slot&               slotAt(Position pos);
    const Slot&         slotAt(Position pos) const;

    std::optional<Position> findNearestWithFamily(const Family& f, Position reference) const;
    // preferNear=true  → smallest x (hot family, fast retrieval)
    // preferNear=false → largest x  (cold family, out of the way)
    std::optional<Position> findFreeSlot(bool preferNear) const;

private:
    int                      length_;
    Position                 port_;
    std::vector<Slot>        slots_;      // length_ storage slots, slot[i].position() = {i, 0}
    std::vector<Shuttle>     shuttles_;
    std::vector<Instruction> instructionQueue_;
    Tick                     currentTick_ = 0;
    Metadata                 meta_;

    // Sorted set of free slot x-coordinates: begin()=nearest, rbegin()=farthest
    std::set<int>            freeSlotXs_;

    // Boxes delivered to port, ready for Robot to collect
    std::unordered_map<Family, std::queue<Box>> readyOutputs_;
    // Boxes waiting at port (arrived via input(), waiting for shuttle)
    std::queue<Box>          pendingInputBoxes_;
    // How many output instructions are in-flight per family
    std::unordered_map<Family, int> outputReservedByFamily_;
    std::vector<Event>*             eventLog_ = nullptr;

    void assignInstructions();
    void updateMeta();
};
