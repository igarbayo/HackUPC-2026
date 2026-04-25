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

    // ── stats ─────────────────────────────────────────────────────────────────
    int   totalPalletsSent()  const;  // all dispatched pallets (full + partial)
    int   filledPalletsSent() const;  // only fully packed pallets
    int   totalBoxesSent()    const;  // sum across all families
    int   boxesSentForFamily(const Family& f) const;
    const std::unordered_map<Family, int>& boxesSentByFamily() const;
    float avgFillRate() const;        // totalBoxesSent / (totalPalletsSent × CAPACITY)

    void    setEventLog(std::vector<Event>* log);
    void    setCurrentTick(Tick t);

private:
    std::array<std::optional<Pallet>, MAX_ACTIVE_PALLETS> pallets_{};
    Aisle::Metadata     lastMeta_{};           // snapshot from last tick, used in onBoxDelivered
    std::vector<Event>* eventLog_           = nullptr;
    Tick                currentTick_        = 0;
    int                 totalPallets_       = 0;
    int                 filledPallets_      = 0;
    int                 totalBoxes_         = 0;
    std::unordered_map<Family, int> boxesByFamily_;

    int   findPalletSlot(const Family& f) const;      // -1 if not found
    int   findEmptyPalletSlot() const;                 // -1 if all occupied
    int   countEmptyPalletSlots() const;
    int   findLeastCompletablePalletSlot() const;      // for forced dispatch
    int   getInFlight(const Aisle::Metadata& meta, const Family& f) const;
    float scoreFamilyForNewPallet(const Family& f, const Aisle::Metadata& meta) const;
};
