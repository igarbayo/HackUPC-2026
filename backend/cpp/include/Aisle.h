#pragma once
#include "types.h"
#include "Box.h"
#include "Slot.h"
#include "Shuttle.h"
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

class Aisle {
public:
    struct Metadata {
        std::unordered_map<Family, int>      countByFamily;
        std::unordered_map<Family, int>      reservedByFamily;  // pending output instructions
        std::unordered_map<Family, Position> nearestByFamily;   // nearest to output port
        int                                  freeSlots      = 0;
        int                                  pendingInputs  = 0;
        int                                  pendingOutputs = 0;
    };

    struct Instruction {
        enum class Kind { Input, Output };
        Kind                  kind;
        std::optional<Box>    incomingBox;   // for Input
        std::optional<Family> requestedFam;  // for Output
        Tick                  issuedAt = 0;
        int                   priority = 0;  // higher = more urgent
    };

    // length: number of storage slots
    // inputPort at x=0, outputPort at x=length+1 (virtual positions, not storage slots)
    Aisle(int length, int numShuttles, Position inputPort, Position outputPort);

    void                input(Box newBox);                 // belt -> aisle
    void                requestOutput(Family f);           // robot -> aisle (non-blocking)
    std::optional<Box>  collectReadyOutput(Family f);      // robot collects if available

    void                ordenarInstrucciones();            // recalculates priorities

    Metadata            metadata()    const;
    Position            inputPort()   const;
    Position            outputPort()  const;

    void                tick();

    // Used by Shuttle to update metadata when placing/taking from storage slots
    void                notifyBoxPlaced(const Box& b, Position where);
    void                notifyBoxTaken (const Box& b, Position where);
    // Used by Shuttle when dropping box at output port
    void                notifyOutputReady(Box b);
    // Used by Shuttle (input mission) to get the next box from the input buffer
    std::optional<Box>  takeFromInputBuffer();

    // Slot access by position (storage slots only, indexed by x from 0 to length-1)
    Slot&               slotAt(Position pos);
    const Slot&         slotAt(Position pos) const;

    std::optional<Position> findNearestWithFamily(const Family& f, Position reference) const;
    std::optional<Position> findFreeSlot() const;

private:
    int                      length_;
    Position                 inputPort_;
    Position                 outputPort_;
    std::vector<Slot>        slots_;      // length_ storage slots, slot[i].position() = {i, 0}
    std::vector<Shuttle>     shuttles_;
    std::vector<Instruction> instructionQueue_;
    Tick                     currentTick_ = 0;
    Metadata                 meta_;

    // Boxes delivered to output port, ready for Robot to collect
    std::unordered_map<Family, std::queue<Box>> readyOutputs_;
    // Boxes waiting at input port (arrived via input(), waiting for shuttle)
    std::queue<Box>          pendingInputBoxes_;
    // How many output instructions are in-flight per family
    std::unordered_map<Family, int> outputReservedByFamily_;

    void assignInstructions();
    void updateMeta();
};
