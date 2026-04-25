#include "Aisle.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>

Aisle::Aisle(int length, int numY, int numSides, Position port)
    : length_(length), numY_(numY), numSides_(numSides), port_(port)
{
    // Build 3D slot grid: [side-1][y-1][x]
    slots_.resize(numSides_);
    for (int s = 1; s <= numSides_; ++s) {
        slots_[s-1].resize(numY_);
        for (int y = 1; y <= numY_; ++y) {
            slots_[s-1][y-1].reserve(length_);
            for (int x = 0; x < length_; ++x) {
                Position p;
                p.x = x; p.y = y; p.z = 1; p.side = s;
                slots_[s-1][y-1].emplace_back(p);
                freeZ1_[{s, y}].insert(x);
            }
        }
    }

    // One shuttle per Y level; starts at head (port.x) on its Y
    for (int y = 1; y <= numY_; ++y) {
        Position startPos;
        startPos.x = port_.x; startPos.y = y; startPos.z = 1; startPos.side = 1;
        shuttles_.emplace_back(startPos);
    }

    updateMeta();
}

void Aisle::input(Box newBox) {
    pendingInputBoxes_.push(std::move(newBox));
    Instruction instr;
    instr.kind     = Instruction::Kind::Input;
    instr.issuedAt = currentTick_;
    instr.priority = 0;
    instructionQueue_.push_back(std::move(instr));
    updateMeta();
}

void Aisle::requestOutput(Family f) {
    Instruction instr;
    instr.kind         = Instruction::Kind::Output;
    instr.requestedFam = f;
    instr.issuedAt     = currentTick_;
    instr.priority     = 0;
    instructionQueue_.push_back(std::move(instr));
    outputReservedByFamily_[f]++;
    updateMeta();
}

std::optional<Box> Aisle::collectReadyOutput(Family f) {
    auto it = readyOutputs_.find(f);
    if (it == readyOutputs_.end() || it->second.empty()) return std::nullopt;
    Box b = std::move(it->second.front());
    it->second.pop();
    return b;
}

void Aisle::ordenarInstrucciones() {
    for (auto& instr : instructionQueue_) {
        Tick waited = currentTick_ - instr.issuedAt;
        instr.priority = static_cast<int>(waited);
        if (instr.kind == Instruction::Kind::Output && instr.requestedFam) {
            auto it = meta_.countByFamily.find(*instr.requestedFam);
            if (it != meta_.countByFamily.end() && it->second > 5)
                instr.priority += 2;
        }
    }
    std::stable_sort(instructionQueue_.begin(), instructionQueue_.end(),
                     [](const Instruction& a, const Instruction& b) {
                         return a.priority > b.priority;
                     });
}

void Aisle::connectBelt(InputBelt& belt) { belt_ = &belt; }

Aisle::Metadata Aisle::metadata() const { return meta_; }
Position        Aisle::port()     const { return port_; }

void Aisle::tick() {
    ++currentTick_;
    if (belt_) {
        while (auto box = belt_->pop())
            input(std::move(*box));
    }
    ordenarInstrucciones();
    for (auto& shuttle : shuttles_) shuttle.tick(*this);
    assignInstructions();
    updateMeta();
}

void Aisle::setEventLog(std::vector<Event>* log) { eventLog_ = log; }

void Aisle::notifyBoxPlaced(const Box& b, Position where) {
    LevelKey key{where.side, where.y};
    if (where.z == 1) {
        freeZ1_[key].erase(where.x);
    }
    // z2 placement doesn't change freeZ1_ (z1 is still occupied at that x)
    if (eventLog_)
        eventLog_->push_back({"box_stored", currentTick_, b.id(), -1, b.family()});
}

void Aisle::notifyBoxTaken(const Box& b, Position where) {
    LevelKey key{where.side, where.y};
    if (where.z == 1) {
        freeZ1_[key].insert(where.x);
    }
    // z2 removal: z1 is still empty, so x was already in freeZ1_ (no change there)
    if (eventLog_)
        eventLog_->push_back({"box_retrieved", currentTick_, b.id(), -1, b.family()});
}

void Aisle::notifyOutputReady(Box b) {
    auto it = outputReservedByFamily_.find(b.family());
    if (it != outputReservedByFamily_.end() && it->second > 0)
        --it->second;
    readyOutputs_[b.family()].push(std::move(b));
}

std::optional<Box> Aisle::takeFromInputBuffer() {
    if (!pendingInputBoxes_.empty()) {
        Box b = std::move(pendingInputBoxes_.front());
        pendingInputBoxes_.pop();
        return b;
    }
    if (belt_) return belt_->pop();
    return std::nullopt;
}

Slot& Aisle::slotAt(Position pos) {
    if (pos.x < 0 || pos.x >= length_)
        throw std::out_of_range("Aisle::slotAt: x out of range");
    if (pos.y < 1 || pos.y > numY_)
        throw std::out_of_range("Aisle::slotAt: y out of range");
    if (pos.side < 1 || pos.side > numSides_)
        throw std::out_of_range("Aisle::slotAt: side out of range");
    return slots_[pos.side - 1][pos.y - 1][pos.x];
}

const Slot& Aisle::slotAt(Position pos) const {
    if (pos.x < 0 || pos.x >= length_)
        throw std::out_of_range("Aisle::slotAt: x out of range");
    if (pos.y < 1 || pos.y > numY_)
        throw std::out_of_range("Aisle::slotAt: y out of range");
    if (pos.side < 1 || pos.side > numSides_)
        throw std::out_of_range("Aisle::slotAt: side out of range");
    return slots_[pos.side - 1][pos.y - 1][pos.x];
}

std::optional<Position> Aisle::findNearestWithFamily(const Family& f, int yLevel) const {
    if (yLevel < 1 || yLevel > numY_) return std::nullopt;
    int bestX = INT_MAX;
    std::optional<Position> best;
    for (int s = 1; s <= numSides_; ++s) {
        for (int x = 0; x < length_; ++x) {
            const Slot& slot = slots_[s-1][yLevel-1][x];
            // z1 (always accessible if occupied)
            if (const Box* b = slot.peek(); b && b->family() == f) {
                if (x < bestX) {
                    bestX = x;
                    best = Position{x, yLevel, 1, s};
                }
            }
            // z2 (accessible only when z1 is empty)
            if (const Box* b = slot.peekZ2(); b && b->family() == f) {
                if (x < bestX) {
                    bestX = x;
                    best = Position{x, yLevel, 2, s};
                }
            }
        }
    }
    return best;
}

std::optional<Position> Aisle::findFreeSlot(int side, int y, bool preferNear) const {
    if (side < 1 || side > numSides_ || y < 1 || y > numY_) return std::nullopt;
    LevelKey key{side, y};
    auto it = freeZ1_.find(key);
    if (it == freeZ1_.end() || it->second.empty()) return std::nullopt;
    int x = preferNear ? *it->second.begin() : *it->second.rbegin();
    return Position{x, y, 1, side};
}

void Aisle::assignNextTo(Shuttle& s) {
    if (!s.isFree() || instructionQueue_.empty()) return;
    const int shuttleY = s.yLevel();

    for (auto it = instructionQueue_.begin(); it != instructionQueue_.end(); ++it) {
        if (it->kind == Instruction::Kind::Input) {
            Family fam;
            if (!pendingInputBoxes_.empty()) {
                fam = pendingInputBoxes_.front().family();
            } else if (belt_ && !belt_->empty()) {
                fam = belt_->peek()->family();
            } else {
                continue;
            }
            bool isHot = outputReservedByFamily_.count(fam) &&
                         outputReservedByFamily_.at(fam) > 0;
            // Try sides in order; hot→near port, cold→far
            std::optional<Position> freePos;
            for (int side = 1; side <= numSides_ && !freePos; ++side)
                freePos = findFreeSlot(side, shuttleY, isHot);
            if (!freePos) continue;

            Position pickupPos;
            pickupPos.x = port_.x; pickupPos.y = shuttleY;
            pickupPos.z = 1;       pickupPos.side = 1;
            s.assignInputMission(pickupPos, *freePos);
            instructionQueue_.erase(it);
            return;

        } else {
            if (!it->requestedFam) continue;
            auto nearestPos = findNearestWithFamily(*it->requestedFam, shuttleY);
            if (!nearestPos) continue;

            Position dropPos;
            dropPos.x = port_.x; dropPos.y = shuttleY;
            dropPos.z = 1;        dropPos.side = 1;
            s.assignOutputMission(*nearestPos, dropPos);
            instructionQueue_.erase(it);
            return;
        }
    }
}

void Aisle::assignInstructions() {
    for (auto& shuttle : shuttles_) assignNextTo(shuttle);
}

void Aisle::updateMeta() {
    meta_.countByFamily.clear();
    meta_.nearestByFamily.clear();
    meta_.freeSlots = 0;

    for (int s = 1; s <= numSides_; ++s) {
        for (int y = 1; y <= numY_; ++y) {
            for (int x = 0; x < length_; ++x) {
                const Slot& slot = slots_[s-1][y-1][x];

                if (const Box* b = slot.peek()) {
                    const Family& f = b->family();
                    ++meta_.countByFamily[f];
                    auto nearIt = meta_.nearestByFamily.find(f);
                    Position p{x, y, 1, s};
                    if (nearIt == meta_.nearestByFamily.end()) {
                        meta_.nearestByFamily[f] = p;
                    } else {
                        int curDist = std::abs(nearIt->second.x - port_.x);
                        int newDist = std::abs(x - port_.x);
                        if (newDist < curDist) nearIt->second = p;
                    }
                }
                if (const Box* b = slot.peekZ2()) {
                    ++meta_.countByFamily[b->family()];
                }
                if (slot.isEmpty()) ++meta_.freeSlots;
            }
        }
    }

    meta_.pendingInputs  = 0;
    meta_.pendingOutputs = 0;
    for (const auto& instr : instructionQueue_) {
        if (instr.kind == Instruction::Kind::Input) ++meta_.pendingInputs;
        else                                         ++meta_.pendingOutputs;
    }

    meta_.readyOutputCount = 0;
    for (const auto& [f, q] : readyOutputs_)
        meta_.readyOutputCount += static_cast<int>(q.size());

    meta_.activeShuttles = 0;
    for (const auto& shuttle : shuttles_)
        if (!shuttle.isFree()) ++meta_.activeShuttles;

    meta_.reservedByFamily = outputReservedByFamily_;
}
