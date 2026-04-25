#pragma once
#include "Aisle.h"
#include "types.h"
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

// Abstract base: every heuristic scores a candidate family for a new pallet slot.
// Higher score = higher priority to open.
class RobotHeuristic {
public:
    virtual ~RobotHeuristic() = default;
    virtual float       score(const Family& f, const Aisle::Metadata& meta) const = 0;
    virtual std::string name()                                                     const = 0;
};

// available / (avgDist + 1): balance stock count against shuttle travel cost.
class StockProximityHeuristic : public RobotHeuristic {
public:
    float score(const Family& f, const Aisle::Metadata& meta) const override {
        int available = countAvailable(f, meta);
        if (available <= 0) return 0.0f;

        float avgDist = 1.0f;
        auto dit = meta.avgDistanceByFamily.find(f);
        if (dit != meta.avgDistanceByFamily.end() && dit->second > 0.0f)
            avgDist = dit->second;

        return static_cast<float>(available) / (avgDist + 1.0f);
    }
    std::string name() const override { return "stock_proximity"; }

private:
    static int countAvailable(const Family& f, const Aisle::Metadata& meta) {
        auto cit = meta.countByFamily.find(f);
        int n = (cit != meta.countByFamily.end()) ? cit->second : 0;
        auto rit = meta.reservedByFamily.find(f);
        if (rit != meta.reservedByFamily.end()) n -= rit->second;
        return n;
    }
};

// available: open pallets for the family with the most boxes in the aisle.
class LargestStockHeuristic : public RobotHeuristic {
public:
    float score(const Family& f, const Aisle::Metadata& meta) const override {
        auto cit = meta.countByFamily.find(f);
        int n = (cit != meta.countByFamily.end()) ? cit->second : 0;
        auto rit = meta.reservedByFamily.find(f);
        if (rit != meta.reservedByFamily.end()) n -= rit->second;
        return static_cast<float>(std::max(0, n));
    }
    std::string name() const override { return "largest_stock"; }
};

// 1 / (avgDist + 1): prefer families whose boxes are closest to the port.
class NearestHeuristic : public RobotHeuristic {
public:
    float score(const Family& f, const Aisle::Metadata& meta) const override {
        float avgDist = 1.0f;
        auto dit = meta.avgDistanceByFamily.find(f);
        if (dit != meta.avgDistanceByFamily.end() && dit->second > 0.0f)
            avgDist = dit->second;
        return 1.0f / (avgDist + 1.0f);
    }
    std::string name() const override { return "nearest"; }
};

// Uniform random score: baseline / lower bound.
class RandomHeuristic : public RobotHeuristic {
public:
    explicit RandomHeuristic(uint64_t seed = 42) : rng_(seed) {}
    float score(const Family&, const Aisle::Metadata&) const override {
        return dist_(rng_);
    }
    std::string name() const override { return "random"; }

private:
    mutable std::mt19937                          rng_;
    mutable std::uniform_real_distribution<float> dist_{0.0f, 1.0f};
};

// Factory: build a heuristic by name. seed is only used by RandomHeuristic.
inline std::shared_ptr<RobotHeuristic> makeHeuristic(const std::string& name,
                                                      uint64_t seed = 42) {
    if (name == "stock_proximity") return std::make_shared<StockProximityHeuristic>();
    if (name == "largest_stock")   return std::make_shared<LargestStockHeuristic>();
    if (name == "nearest")         return std::make_shared<NearestHeuristic>();
    if (name == "random")          return std::make_shared<RandomHeuristic>(seed);
    throw std::invalid_argument("makeHeuristic: unknown heuristic '" + name + "'");
}
