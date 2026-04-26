#pragma once
#include "Aisle.h"
#include "types.h"
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>

// Per-call context carrying inter-robot coordination info for cooperative heuristics.
struct RobotContext {
    std::unordered_set<Family> ownClaims;    // families this robot has a pallet open for
    std::unordered_set<Family> otherClaims;  // families any other robot has a pallet open for
};

// Abstract base: every heuristic scores a candidate family for a new pallet slot.
// Higher score = higher priority to open.
class RobotHeuristic {
public:
    virtual ~RobotHeuristic() = default;
    virtual float       score(const Family& f, const Aisle::Metadata& meta,
                              const RobotContext& ctx) const = 0;
    virtual std::string name()                               const = 0;
};

// available / (avgDist + 1): balance stock count against shuttle travel cost.
class StockProximityHeuristic : public RobotHeuristic {
public:
    float score(const Family& f, const Aisle::Metadata& meta,
                const RobotContext&) const override {
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
    float score(const Family& f, const Aisle::Metadata& meta,
                const RobotContext&) const override {
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
    float score(const Family& f, const Aisle::Metadata& meta,
                const RobotContext&) const override {
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
    float score(const Family&, const Aisle::Metadata&,
                const RobotContext&) const override {
        return dist_(rng_);
    }
    std::string name() const override { return "random"; }

private:
    mutable std::mt19937                          rng_;
    mutable std::uniform_real_distribution<float> dist_{0.0f, 1.0f};
};

// Cooperative multi-robot heuristic (S_coop).
// Base score = available / (avgDist + 1), multiplied by a coordination factor C
// and reduced by a congestion penalty P derived from active shuttles in the family's levels.
//
// C = 0.1  if another robot already has a pallet for this family (soft lock)
// C = 1.5  if this robot already has a pallet for this family (encourage finishing)
// C = 1.0  otherwise (unclaimed)
//
// P = sum of active shuttles across levels that store family f, × CONGESTION_WEIGHT
class CoopHeuristic : public RobotHeuristic {
public:
    static constexpr float CONGESTION_WEIGHT = 0.5f;

    float score(const Family& f, const Aisle::Metadata& meta,
                const RobotContext& ctx) const override {
        int available = countAvailable(f, meta);
        if (available <= 0) return 0.0f;

        float avgDist = 1.0f;
        auto dit = meta.avgDistanceByFamily.find(f);
        if (dit != meta.avgDistanceByFamily.end() && dit->second > 0.0f)
            avgDist = dit->second;

        float base = static_cast<float>(available) / (avgDist + 1.0f);

        float C = 1.0f;
        if (ctx.otherClaims.count(f))     C = 0.1f;
        else if (ctx.ownClaims.count(f))  C = 1.5f;

        float P = 0.0f;
        auto lit = meta.levelsByFamily.find(f);
        if (lit != meta.levelsByFamily.end()) {
            for (int level : lit->second) {
                auto sit = meta.activeShuttlesByLevel.find(level);
                if (sit != meta.activeShuttlesByLevel.end())
                    P += static_cast<float>(sit->second);
            }
        }
        P *= CONGESTION_WEIGHT;

        return base * C - P;
    }
    std::string name() const override { return "coop"; }

private:
    static int countAvailable(const Family& f, const Aisle::Metadata& meta) {
        auto cit = meta.countByFamily.find(f);
        int n = (cit != meta.countByFamily.end()) ? cit->second : 0;
        auto rit = meta.reservedByFamily.find(f);
        if (rit != meta.reservedByFamily.end()) n -= rit->second;
        return n;
    }
};

// Factory: build a heuristic by name. seed is only used by RandomHeuristic.
inline std::shared_ptr<RobotHeuristic> makeHeuristic(const std::string& name,
                                                      uint64_t seed = 42) {
    if (name == "stock_proximity") return std::make_shared<StockProximityHeuristic>();
    if (name == "largest_stock")   return std::make_shared<LargestStockHeuristic>();
    if (name == "nearest")         return std::make_shared<NearestHeuristic>();
    if (name == "random")          return std::make_shared<RandomHeuristic>(seed);
    if (name == "coop")            return std::make_shared<CoopHeuristic>();
    throw std::invalid_argument("makeHeuristic: unknown heuristic '" + name + "'");
}
