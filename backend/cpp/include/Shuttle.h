#pragma once
#include "types.h"
#include "Box.h"
#include <optional>

class Aisle; // forward declaration

class Shuttle {
public:
    enum class Phase { Idle, MovingToPickup, Loading, MovingToDrop, Unloading };

    explicit Shuttle(Position startPos);

    // Getters
    Position    position()    const;
    int         yLevel()      const;   // fixed Y level (pos_.y); never changes
    bool        isFree()      const;   // true when Phase::Idle
    bool        isOnMission() const;
    Phase       phase()       const;
    const Box*  carriedBox()  const;   // nullptr if not carrying

    // For output: pickup from a storage slot, drop at output port
    void        assignOutputMission(Position pickupFrom, Position dropAt);
    // For input: pickup from input port (box taken from aisle's input buffer), drop at free slot
    void        assignInputMission(Position pickupFrom, Position dropAt);
    // For relocation: pickup from a far storage slot, drop at a closer storage slot
    void        assignRelocateMission(Position pickupFrom, Position dropAt);

    void        tick(Aisle& aisle);    // advances one step/phase

private:
    enum class MissionKind { Input, Output, Relocate };

    Position           pos_;
    Phase              phase_ = Phase::Idle;
    std::optional<Box> carried_;
    Position           pickup_{};
    Position           drop_{};
    MissionKind        missionKind_ = MissionKind::Output;

    void moveToward(Position target);
};
