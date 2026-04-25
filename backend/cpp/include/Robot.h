#pragma once
#include "types.h"
#include "Box.h"
#include "Pallet.h"
#include "Aisle.h"
#include "Heuristic.h"
#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

class Robot {
public:
    static constexpr int MAX_ACTIVE_PALLETS   = 4;
    // Minimum available boxes required to open a new pallet for a family.
    static constexpr int OPEN_THRESHOLD       = Pallet::CAPACITY / 2;  // 6

    using Request = std::unordered_map<Family, int>;

    // Reads metadata, decides which families to request output for and how many.
    Request decide(const Aisle::Metadata& meta);
    // Full tick: collect ready outputs, decide, make requests.
    void    tick(Aisle& aisle);

    const std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS>& pallets() const;

    void    onBoxDelivered(Box b);
    Pallet  dispatchPallet(int slotIndex);

    void    setEventLog(std::vector<Event>* log);
    void    setCurrentTick(Tick t);
    void    setRobotId(int id);
    void    setHeuristic(std::shared_ptr<RobotHeuristic> h);

private:
    std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS> pallets_{};
    Aisle::Metadata                lastMeta_{};
    std::vector<Event>*            eventLog_   = nullptr;
    Tick                           currentTick_ = 0;
    int                            robotId_    = 0;
    std::shared_ptr<RobotHeuristic> heuristic_ = std::make_shared<StockProximityHeuristic>();

    int   findPalletSlot(const Family& f) const;
    int   findEmptyPalletSlot() const;
    int   countEmptyPalletSlots() const;
    int   findLeastCompletablePalletSlot() const;
    int   getInFlight(const Aisle::Metadata& meta, const Family& f) const;
};
