#pragma once
#include "Aisle.h"
#include "Box.h"
#include "types.h"
#include <optional>
#include <vector>

class AisleContainer {
public:
    using Metadata = Aisle::Metadata;

    AisleContainer(int numAisles, int length, int numY, int numSides, Position port);

    void               placeAt(int aisle_idx, Box box, Position pos);
    void               input(Box box);
    void               requestOutput(const Family& f);
    std::optional<Box> collectReadyOutput(const Family& f);

    Metadata                metadata() const;
    std::vector<AisleSnap>  snapshot() const;

    long long               totalMovesX() const;
    long long               totalMovesZ() const;

    void               tick();
    void               setEventLog(std::vector<Event>* log);

private:
    std::vector<Aisle> aisles_;

    // Returns index of best aisle to store this box.
    int selectAisleForInput(const Box& box) const;
    // Returns index of aisle with the most available boxes of family f.
    int selectAisleForOutput(const Family& f) const;

    Metadata aggregateMeta() const;
};
