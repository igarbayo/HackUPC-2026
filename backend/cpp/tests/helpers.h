#pragma once
#include "Box.h"
#include <cassert>
#include <iostream>
#include <string>

#define SUITE(desc)   std::cout << "\nSuite: " << (desc) << "\n"
#define SECTION(desc) std::cout << "\n  -- " << (desc) << " --\n"

inline int passed = 0;
inline int failed = 0;

#define RUN(name) \
    do { \
        std::cout << "  " << #name << " ... "; \
        try { name(); std::cout << "PASS\n"; ++passed; } \
        catch (const std::exception& e) { std::cout << "FAIL (" << e.what() << ")\n"; ++failed; } \
        catch (...) { std::cout << "FAIL (unknown exception)\n"; ++failed; } \
    } while (0)

inline Box makeBox(BoxId id, Family family, Tick arrival = 0) {
    return Box(id, family, arrival);
}
