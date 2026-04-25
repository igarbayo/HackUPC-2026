#include "InputBelt.h"

void InputBelt::push(Box b) {
    queue_.push_back(std::move(b));
}

std::optional<Box> InputBelt::pop() {
    if (queue_.empty()) return std::nullopt;
    Box b = std::move(queue_.front());
    queue_.pop_front();
    return b;
}

bool        InputBelt::empty() const { return queue_.empty(); }
std::size_t InputBelt::size()  const { return queue_.size(); }
