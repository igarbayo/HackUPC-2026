#pragma once
#include "Box.h"
#include <deque>
#include <optional>
#include <cstddef>

class InputBelt {
public:
    void               push(Box b);
    std::optional<Box> pop();
    bool               empty() const;
    std::size_t        size()  const;
private:
    std::deque<Box> queue_;
};
