#pragma once
#include "types.h"
#include "Box.h"
#include <optional>
#include <stdexcept>

class Slot {
public:
    explicit Slot(Position pos);
    Position     position()  const;
    bool         isEmpty()   const;   // z1 empty (no front box)
    bool         isFull()    const;   // z1 and z2 both occupied
    bool         hasZ2()     const;   // z1 empty, z2 occupied (directly accessible)
    const Box*   peek()      const;   // z1 box or nullptr
    const Box*   peekZ2()    const;   // z2 box only when z1 empty, else nullptr
    void         place(Box b);        // place at z1 (requires z1 empty)
    void         placeZ2(Box b);      // place at z2 (requires z1 occupied, z2 empty)
    Box          take();              // take z1 (requires z1 occupied)
    Box          takeZ2();            // take z2 (requires z1 empty, z2 occupied)
private:
    Position           pos_;
    std::optional<Box> z1_;
    std::optional<Box> z2_;
};
