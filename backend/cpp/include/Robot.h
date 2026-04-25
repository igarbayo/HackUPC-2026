#pragma once
#include "types.h"
#include "Box.h"
#include "Pallet.h"
#include "Aisle.h"
#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

class Robot {
public:
    static constexpr int MAX_ACTIVE_PALLETS = 4;
    using Request = std::unordered_map<Family, int>;

    // Reads metadata, decides which families to request output for
    Request decide(const Aisle::Metadata& meta);
    // Full tick: collect ready outputs, decide, make requests
    void    tick(Aisle& aisle);

    const std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS>& pallets() const;

    void    onBoxDelivered(Box b);
    Pallet  dispatchPallet(int slotIndex);

    int     dispatchedPallets() const;
    int     dispatchedBoxes()   const;

    void    setEventLog(std::vector<Event>* log);
    void    setCurrentTick(Tick t);

private:
    std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS> pallets_{};
    std::vector<Event>* eventLog_      = nullptr;
    Tick                currentTick_   = 0;
    int         dispatchedFullPallets_ = 0;
    int             dispatchedBoxes_   = 0;

    int findPalletSlot(const Family& f) const;  // -1 if not found
    int findEmptyPalletSlot() const;             // -1 if all occupied
    int findLeastFullPalletSlot() const;         // for forced dispatch
};
