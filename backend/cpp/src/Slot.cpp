#include "Slot.h"

Slot::Slot(Position pos) : pos_(pos) {}

Position   Slot::position()  const { return pos_; }
bool       Slot::isEmpty()   const { return !z1_.has_value(); }
bool       Slot::isFull()    const { return z1_.has_value() && z2_.has_value(); }
bool       Slot::hasZ2()     const { return !z1_.has_value() && z2_.has_value(); }
const Box* Slot::peek()      const { return z1_ ? &*z1_ : nullptr; }
const Box* Slot::peekZ2()    const { return (!z1_ && z2_) ? &*z2_ : nullptr; }

void Slot::place(Box b) {
    if (z1_) throw std::runtime_error("Slot::place: z1 already occupied");
    z1_ = std::move(b);
}

void Slot::placeZ2(Box b) {
    if (!z1_) throw std::runtime_error("Slot::placeZ2: z1 must be occupied first");
    if (z2_)  throw std::runtime_error("Slot::placeZ2: z2 already occupied");
    z2_ = std::move(b);
}

Box Slot::take() {
    if (!z1_) throw std::runtime_error("Slot::take: z1 is empty");
    Box b = std::move(*z1_);
    z1_.reset();
    return b;
}

Box Slot::takeZ2() {
    if (z1_)  throw std::runtime_error("Slot::takeZ2: z1 must be empty to access z2");
    if (!z2_) throw std::runtime_error("Slot::takeZ2: z2 is empty");
    Box b = std::move(*z2_);
    z2_.reset();
    return b;
}
