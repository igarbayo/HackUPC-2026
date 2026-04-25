#pragma once
#include "types.h"
#include "Box.h"
#include <optional>
#include <stdexcept>

class Slot {
public:
    explicit Slot(Position pos);
    Position     position() const;
    bool         isEmpty()  const;
    const Box*   peek()     const;   // nullptr if empty
    void         place(Box b);
    Box          take();             // throws if empty
private:
    Position           pos_;
    std::optional<Box> box_;
};
