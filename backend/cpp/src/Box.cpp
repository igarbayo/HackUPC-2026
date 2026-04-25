#include "Box.h"

Box::Box(BoxId id, Family family, Tick arrivalTick)
    : id_(id), family_(std::move(family)), arrivalTick_(arrivalTick) {}

BoxId  Box::id()          const { return id_; }
Family Box::family()      const { return family_; }
Tick   Box::arrivalTick() const { return arrivalTick_; }
