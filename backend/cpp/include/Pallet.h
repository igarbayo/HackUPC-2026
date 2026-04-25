#pragma once
#include "types.h"
#include "Box.h"
#include <vector>
#include <stdexcept>

class Pallet {
public:
    static constexpr int CAPACITY = 12;

    explicit Pallet(Family family);
    Family  family()        const;
    int     placedCount()   const;
    int     reservedCount() const;
    int     freeSlots()     const;  // CAPACITY - placed - reserved
    bool    isFull()        const;  // placedCount == CAPACITY

    void    reserve();
    void    releaseReservation();
    void    placeBox(Box b);        // converts reservation -> real box

    const std::vector<Box>& boxes() const;
private:
    Family           family_;
    int              reserved_ = 0;
    std::vector<Box> boxes_;
};
