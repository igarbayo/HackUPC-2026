#include "Slot.h"

Slot::Slot(Position pos) : pos_(pos) {}

Position   Slot::position() const { return pos_; }
bool       Slot::isEmpty()  const { return !box_.has_value(); }
const Box* Slot::peek()     const { return box_ ? &*box_ : nullptr; }

void Slot::place(Box b) {
    box_ = std::move(b);
}

Box Slot::take() {
    if (!box_) throw std::runtime_error("Slot::take() called on empty slot");
    Box b = std::move(*box_);
    box_.reset();
    return b;
}
