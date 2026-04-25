#pragma once
#include "Box.h"
#include "types.h"
#include <deque>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct DemandPeriod {
    Tick   from_tick       = 0;
    Tick   to_tick         = UINT64_MAX;
    double rate_multiplier = 1.0;  // >1 = more boxes (shorter inter-arrival)
};

struct BoxGeneratorParams {
    int                                num_boxes        = 100;
    int                                num_destinations = 20;   // clamped to available list
    std::unordered_map<Family, double> weights;                 // empty = uniform
    uint64_t                           seed             = 42;

    double mean_inter_arrival_ticks = 3.6;    // ticks between boxes (3600/1000)
    double std_inter_arrival_ticks  = 0.072;  // ticks std dev (3.6 * 20/1000)
    std::vector<DemandPeriod>          demand_profile;          // empty = flat rate

    // Load from JSON file; missing fields keep their default values.
    static BoxGeneratorParams fromFile(const std::string& path);
};

class InputBelt {
public:
    void               push(Box b);
    std::optional<Box> pop();
    std::optional<Box> peek() const;
    bool               empty() const;
    std::size_t        size()  const;

    // Factory: generate full box sequence from config; returns a ready-to-drain InputBelt.
    static InputBelt generate(const BoxGeneratorParams& p);

    // Ordered list of all available Inditex destinations (>= 20 entries).
    static const std::vector<Family>& availableDestinations();

private:
    std::deque<Box> queue_;
};
