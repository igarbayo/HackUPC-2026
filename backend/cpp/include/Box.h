#pragma once
#include "types.h"

class Box {
public:
    Box(BoxId id, Family family, Tick arrivalTick = 0);
    BoxId  id()          const;
    Family family()      const;
    Tick   arrivalTick() const;
private:
    BoxId  id_;
    Family family_;
    Tick   arrivalTick_;
};
