#pragma once
#include "types.h"
#include "Box.h"
#include "Pallet.h"
#include "Aisle.h"
#include <array>
#include <optional>
#include <unordered_map>

class Robot {
public:
    static constexpr int MAX_ACTIVE_PALLETS = 4;
    using Request = std::unordered_map<Family, int>;

    // Reads metadata, decides which families to request output for
    Request decide(const Aisle::Metadata& meta);
    // Full tick: collect ready outputs, decide, make requests
    void    tick(Aisle& aisle);

    const std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS>& pallets() const;

    // Called when a box is delivered from aisle to robot
    // Contains the logic: open pallet / place in existing / force dispatch
    void    onBoxDelivered(Box b);
    // Close and remove a pallet (full or forced)
    Pallet  dispatchPallet(int slotIndex);

private:
    std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS> pallets_{};

    int findPalletSlot(const Family& f) const;  // -1 if not found
    int findEmptyPalletSlot() const;             // -1 if all occupied
    int findLeastFullPalletSlot() const;         // for forced dispatch
};
