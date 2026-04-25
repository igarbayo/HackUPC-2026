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
        while (belt_->peek() && belt_->peek()->arrivalTick() <= currentTick_)
            input(std::move(*belt_->pop()));
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

std::optional<Position> Aisle::findBestBoxForShuttle(const Family& f, Position shuttlePos) const {
    const int yLevel = shuttlePos.y;
    if (yLevel < 1 || yLevel > numY_) return std::nullopt;

    int bestCost   = INT_MAX;
    bool bestFrees = false;  // does best candidate free a blocked z2 box?
    int bestX      = -1;
    std::optional<Position> best;

    for (int s = 1; s <= numSides_; ++s) {
        for (int x = 0; x < length_; ++x) {
            const Slot& slot = slots_[s-1][yLevel-1][x];
            int cost = std::abs(shuttlePos.x - x) + std::abs(x - port_.x);

            // freesBlocked: picking z1 from a full slot (z1+z2 both occupied) makes
            // the z2 box immediately accessible — prioritised over lone z1 boxes.
            auto tryBox = [&](int z, const Box* b, bool freesBlocked) {
                if (!b || b->family() != f) return;
                // Tiebreak order: 1) lower cost  2) full-cell preference  3) larger x
                bool better = cost < bestCost
                    || (cost == bestCost && freesBlocked && !bestFrees)
                    || (cost == bestCost && freesBlocked == bestFrees && x > bestX);
                if (better) {
                    bestCost  = cost;
                    bestFrees = freesBlocked;
                    bestX     = x;
                    best      = Position{x, yLevel, z, s};
                }
            };
            tryBox(1, slot.peek(),    slot.isFull());  // z1: accessible if occupied
            tryBox(2, slot.peekZ2(), false);           // z2: only accessible when z1 empty
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

            if (pendingInputBoxes_.empty()) continue;
            const Family& fam = pendingInputBoxes_.front().family();

            // X_ideal: near port; X_pick: x of nearest existing box of same family
            const int x_ideal = port_.x;
            int x_pick = port_.x;
            auto nearestFam = findNearestWithFamily(fam, shuttleY);
            if (nearestFam) x_pick = nearestFam->x;

            // Weights from hackupc2.pdf
            constexpr int W1 = 10, W2 = 1, W3 = 10;

            std::optional<Position> bestPos;
            int bestCost = INT_MAX;

            for (int side = 1; side <= numSides_; ++side) {
                LevelKey key{side, shuttleY};
                auto fit = freeZ1_.find(key);
                if (fit == freeZ1_.end()) continue;
                for (int xs : fit->second) {
                    if (xs == port_.x) continue;
                    const Slot& slot = slots_[side-1][shuttleY-1][xs];
                    int penZ;
                    const Box* z2 = slot.peekZ2();
                    if      (!z2)                      penZ =  1000;
                    else if (z2->family() == fam)      penZ = -1000;
                    else                               penZ =   500;
                    int cost = W1 * std::abs(xs - x_ideal)
                             + W2 * penZ
                             + W3 * std::abs(xs - x_pick);
                    if (cost < bestCost) {
                        bestCost = cost;
                        bestPos  = Position{xs, shuttleY, 1, side};
                    }
                }
            }
            if (!bestPos) continue;


            Position pickupPos;
            pickupPos.x = port_.x; pickupPos.y = shuttleY;
            pickupPos.z = 1;       pickupPos.side = 1;
            s.assignInputMission(pickupPos, *bestPos);
            instructionQueue_.erase(it);
            return;

        } else {
            if (!it->requestedFam) continue;
            auto boxPos = findBestBoxForShuttle(*it->requestedFam, s.position());
            if (!boxPos) continue;

            Position dropPos;
            dropPos.x = port_.x; dropPos.y = shuttleY;
            dropPos.z = 1;        dropPos.side = 1;
            s.assignOutputMission(*boxPos, dropPos);
            instructionQueue_.erase(it);
            return;
        }
    }
}

void Aisle::assignInstructions() {
    // Output pass: for each output instruction (sorted by priority), find the
    // globally best (shuttle, box) pair and assign it.
    for (auto instrIt = instructionQueue_.begin(); instrIt != instructionQueue_.end(); ) {
        if (instrIt->kind != Instruction::Kind::Output || !instrIt->requestedFam) {
            ++instrIt;
            continue;
        }

        int bestCost = INT_MAX;
        int bestX    = -1;
        int bestIdx  = -1;
        std::optional<Position> bestBox;

        for (int i = 0; i < (int)shuttles_.size(); ++i) {
            if (!shuttles_[i].isFree()) continue;
            auto boxPos = findBestBoxForShuttle(*instrIt->requestedFam, shuttles_[i].position());
            if (!boxPos) continue;
            int cost = std::abs(shuttles_[i].position().x - boxPos->x)
                     + std::abs(boxPos->x - port_.x);
            if (cost < bestCost || (cost == bestCost && boxPos->x > bestX)) {
                bestCost = cost;
                bestX    = boxPos->x;
                bestIdx  = i;
                bestBox  = boxPos;
            }
        }

        if (bestIdx != -1) {
            Position dropPos;
            dropPos.x = port_.x; dropPos.y = shuttles_[bestIdx].yLevel();
            dropPos.z = 1;        dropPos.side = 1;
            shuttles_[bestIdx].assignOutputMission(*bestBox, dropPos);
            instrIt = instructionQueue_.erase(instrIt);
        } else {
            ++instrIt;
        }
    }

    // Input pass: assign to any free shuttle.
    for (auto& shuttle : shuttles_) {
        if (!shuttle.isFree() || pendingInputBoxes_.empty()) continue;
        const Family& fam = pendingInputBoxes_.front().family();
        bool isHot = outputReservedByFamily_.count(fam) &&
                     outputReservedByFamily_.at(fam) > 0;
        std::optional<Position> freePos;
        for (int side = 1; side <= numSides_ && !freePos; ++side)
            freePos = findFreeSlot(side, shuttle.yLevel(), isHot);
        if (!freePos) continue;
        for (auto it = instructionQueue_.begin(); it != instructionQueue_.end(); ++it) {
            if (it->kind == Instruction::Kind::Input) {
                Position pickupPos;
                pickupPos.x = port_.x; pickupPos.y = shuttle.yLevel();
                pickupPos.z = 1;        pickupPos.side = 1;
                shuttle.assignInputMission(pickupPos, *freePos);
                instructionQueue_.erase(it);
                break;
            }
        }
    }
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
