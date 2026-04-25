#include "Aisle.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>

// Constructor
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

// Inserts new box in pendingInputBoxes and instruction on intructionQueue
void Aisle::input(Box newBox) {
    Instruction instr;
    instr.kind     = Instruction::Kind::Input;
    instr.box_id   = newBox.id();
    instr.family   = newBox.family();
    instr.issuedAt = currentTick_;
    instr.priority = 0;
    instr.seq      = nextSeq_++;
    pendingInputBoxes_.push(std::move(newBox));
    instructionQueue_.push_back(std::move(instr));
    updateMeta();
}

// Requests a box from family f
void Aisle::requestOutput(Family f) {
    Instruction instr;
    instr.kind         = Instruction::Kind::Output;
    instr.requestedFam = f;
    instr.family       = f;
    instr.issuedAt     = currentTick_;
    instr.priority     = 0;
    instr.seq          = nextSeq_++;
    instructionQueue_.push_back(std::move(instr));
    outputReservedByFamily_[f]++;
    updateMeta();
}

// Collects a box that was requested before
std::optional<Box> Aisle::collectReadyOutput(Family f) {
    auto it = readyOutputs_.find(f);
    if (it == readyOutputs_.end() || it->second.empty()) return std::nullopt;
    Box b = std::move(it->second.front());
    it->second.pop();
    return b;
}

// SSTF-inspired ordering (HDD disk scheduling analog): prioritize instructions
// whose estimated target X is closest to a free shuttle, with aging for anti-starvation.
void Aisle::ordenarInstrucciones() {
    constexpr int SEEK_WEIGHT = 3;

    for (auto& instr : instructionQueue_) {
        Tick waited = currentTick_ - instr.issuedAt;
        instr.priority = static_cast<int>(waited);

        // Stock bonus: high-stock families drain faster
        if (instr.kind == Instruction::Kind::Output && instr.requestedFam) { // guard: requestedFam is optional
            auto it = meta_.countByFamily.find(*instr.requestedFam);
            if (it != meta_.countByFamily.end() && it->second > 5)
                instr.priority += 2;
        }

        // SSTF: compute min seek cost across free shuttles.
        // Input target = port_.x (pickup always at port).
        // Output target = nearestByFamily[fam].x → port_.x (mirrors findBestBoxForShuttle cost).
        int min_seek = -1;
        for (const auto& shuttle : shuttles_) {
            if (!shuttle.isFree()) continue;
            int seek = 0;
            if (instr.kind == Instruction::Kind::Input) {
                seek = std::abs(shuttle.position().x - port_.x);
            } else if (instr.requestedFam) {
                auto it = meta_.nearestByFamily.find(*instr.requestedFam);
                if (it == meta_.nearestByFamily.end()) {
                    seek = length_; // family not stored yet — max penalty
                } else {
                    int bx = it->second.x;
                    seek = std::abs(shuttle.position().x - bx) + std::abs(bx - port_.x);
                }
            }
            if (min_seek < 0 || seek < min_seek) min_seek = seek;
        }

        // Apply normalized seek penalty; skip if no free shuttle (will wait for one)
        if (min_seek >= 0 && length_ > 0)
            instr.priority -= (min_seek * SEEK_WEIGHT) / length_;

        if (instr.priority < 0) instr.priority = 0;
    }

    std::stable_sort(instructionQueue_.begin(), instructionQueue_.end(),
                     [](const Instruction& a, const Instruction& b) {
                         return a.priority > b.priority;
                     });
}

// Composition pattern: sets the belt for de aisle
void Aisle::connectBelt(InputBelt& belt) { belt_ = &belt; }

// Getters
Aisle::Metadata Aisle::metadata() const { return meta_; }
Position        Aisle::port()     const { return port_; }

// Orchestrator function: updates aisle tick, processes boxes from the belt, order instructions and runs shuttles ticks
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

// Send an event to be registered
void Aisle::setEventLog(std::vector<Event>* log) { eventLog_ = log; }

// Notifies that a box was set in a slot by a shuttle
void Aisle::notifyBoxPlaced(const Box& b, Position where) {
    LevelKey key{where.side, where.y};
    if (where.z == 1) {
        freeZ1_[key].erase(where.x);
    }
    // z2 placement doesn't change freeZ1_ (z1 is still occupied at that x)

    auto it = oldestArrivalByFamily_.find(b.family());
    if (it == oldestArrivalByFamily_.end() || b.arrivalTick() < it->second)
        oldestArrivalByFamily_[b.family()] = b.arrivalTick();

    // Release slot claim (set by assignNextTo when the mission was issued)
    claimedInputSlots_[key].erase(where.x);

    if (eventLog_)
        eventLog_->push_back({"box_stored", currentTick_, b.id(), -1, b.family()});
}

// Notifies thata  box that was requested and ready was taken
void Aisle::notifyBoxTaken(const Box& b, Position where) {
    LevelKey key{where.side, where.y};
    if (where.z == 1) {
        freeZ1_[key].insert(where.x);
    }
    // z2 removal: z1 is still empty, so x was already in freeZ1_ (no change there)

    // Recompute oldest arrival for this family (removed box may have been the oldest)
    // TO-DO: comprobar si oldestArrivalByFamily_se usa (actualmente no). Si no se usa eliminar el atributo y toda la logica relacionada
    oldestArrivalByFamily_.erase(b.family());
    for (int s = 1; s <= numSides_; ++s) {
        for (int y = 1; y <= numY_; ++y) {
            for (int x = 0; x < length_; ++x) {
                const Slot& sl = slots_[s-1][y-1][x];
                auto check = [&](const Box* bx) {
                    if (!bx || bx->family() != b.family()) return;
                    auto it = oldestArrivalByFamily_.find(b.family());
                    if (it == oldestArrivalByFamily_.end() || bx->arrivalTick() < it->second)
                        oldestArrivalByFamily_[b.family()] = bx->arrivalTick();
                };
                check(sl.peek());
                check(sl.peekZ2());
            }
        }
    }

    // Release any stale input claim on this slot (shouldn't normally be set for outputs)
    claimedInputSlots_[key].erase(where.x);

    if (eventLog_)
        eventLog_->push_back({"box_retrieved", currentTick_, b.id(), -1, b.family()});
}
// Notifies that a box that was requested by the robot is finally ready to pick up
void Aisle::notifyOutputReady(Box b) {
    auto it = outputReservedByFamily_.find(b.family());
    if (it != outputReservedByFamily_.end() && it->second > 0)
        --it->second;
    readyOutputs_[b.family()].push(std::move(b));
}

// Pops the box from the belt
std::optional<Box> Aisle::takeFromInputBuffer() {
    if (!pendingInputBoxes_.empty()) {
        Box b = std::move(pendingInputBoxes_.front());
        pendingInputBoxes_.pop();
        return b;
    }
    if (belt_) return belt_->pop();
    return std::nullopt;
}
// returns a mutable reference of the slot
Slot& Aisle::slotAt(Position pos) {
    if (pos.x < 0 || pos.x >= length_)
        throw std::out_of_range("Aisle::slotAt: x out of range");
    if (pos.y < 1 || pos.y > numY_)
        throw std::out_of_range("Aisle::slotAt: y out of range");
    if (pos.side < 1 || pos.side > numSides_)
        throw std::out_of_range("Aisle::slotAt: side out of range");
    return slots_[pos.side - 1][pos.y - 1][pos.x];
}
// returns a non-mutable reference of the slot
const Slot& Aisle::slotAt(Position pos) const {
    if (pos.x < 0 || pos.x >= length_)
        throw std::out_of_range("Aisle::slotAt: x out of range");
    if (pos.y < 1 || pos.y > numY_)
        throw std::out_of_range("Aisle::slotAt: y out of range");
    if (pos.side < 1 || pos.side > numSides_)
        throw std::out_of_range("Aisle::slotAt: side out of range");
    return slots_[pos.side - 1][pos.y - 1][pos.x];
}

// Returns the position of the best box of family f for the shuttle to pick up and carry to the port.
// Cost = |shuttle.x - box.x| + |box.x - port.x|. Tiebreaks: prefer full slots (picking z1 frees
// a blocked z2), then larger x. Returns nullopt if no box of family f exists at the shuttle's y level.
// Called by: Aisle::assignInstructions (Aisle.cpp:344, 401).
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

// Heuristic slot selector for storing a box in the aisle at row y.
// Scores every free z1 slot with a weighted cost:
//   W1 * |x - xIdeal|          — prefer x near the "ideal" position, which shifts toward x=0
//                                 as the aisle fills up (more stock → place closer to port).
//   W2 * penZ                  — slot penalty: empty slot (+1000, wasteful), same-family z2 (-1000,
//                                 colocation bonus), different-family z2 (+500, mixed cell penalty).
//   W3 * |x - nextPickX|       — prefer slots near the next pending output pick, reducing travel.
//   W4 * shuttleLoad * 10      — heavy penalty when the shuttle is already busy (avoid contention).
// Returns nullopt if no free z1 slot exists at row y.
// Called by: Aisle::assignNextTo (Aisle.cpp:331).
std::optional<Position> Aisle::findBestInputSlot(const Box& box, int y) const {
    if (y < 1 || y > numY_) return std::nullopt;

    constexpr int W1 = 1, W2 = 1, W3 = 1, W4 = 10;
    constexpr int LOAD_CONSTANT = 10;

    const Family& fam = box.family();
    double completitud = meta_.countByFamily.count(fam)
                       ? meta_.countByFamily.at(fam) / 12.0 : 0.0;
    int xIdeal = static_cast<int>((1.0 - completitud) * (length_ - 1));

    int nextPickX = -1;
    for (const auto& instr : instructionQueue_) {
        if (instr.kind == Instruction::Kind::Output && instr.requestedFam) {
            auto pos = findNearestWithFamily(*instr.requestedFam, y);
            if (pos) { nextPickX = pos->x; break; }
        }
    }

    int shuttleLoad = shuttles_[y - 1].isFree() ? 0 : 1;

    int bestCost = INT_MAX;
    std::optional<Position> best;

    for (int side = 1; side <= numSides_; ++side) {
        LevelKey key{side, y};
        auto it = freeZ1_.find(key);
        if (it == freeZ1_.end()) continue;
        for (int x : it->second) {
            const Slot& slot = slots_[side - 1][y - 1][x];
            const Box* z2 = slot.peekZ2();

            int penZ;
            if      (!z2)                  penZ = +1000;
            else if (z2->family() == fam)  penZ = -1000;
            else                           penZ =  +500;

            int cost = W1 * std::abs(x - xIdeal)
                     + W2 * penZ
                     + W3 * (nextPickX >= 0 ? std::abs(x - nextPickX) : 0)
                     + W4 * shuttleLoad * LOAD_CONSTANT;

            if (cost < bestCost) {
                bestCost = cost;
                best = Position{x, y, 1, side};
            }
        }
    }
    return best;
}

// Assigns the first viable pending instruction to shuttle s.
// Input: picks next box from belt, reserves best slot, sends store mission.
// Output: locates requested family box, sends retrieve mission.
// Called by: Shuttle::onMissionComplete (Shuttle.cpp:96), Aisle::tick (Aisle.cpp:395).
void Aisle::assignNextTo(Shuttle& s) {
    if (!s.isFree() || instructionQueue_.empty()) return;
    const int shuttleY = s.yLevel();

    for (auto it = instructionQueue_.begin(); it != instructionQueue_.end(); ++it) {
        if (it->kind == Instruction::Kind::Input) {

            if (pendingInputBoxes_.empty()) continue;
            auto freePos = findBestInputSlot(pendingInputBoxes_.front(), shuttleY);
            if (!freePos) continue;

            claimedInputSlots_[{freePos->side, freePos->y}].insert(freePos->x);

            Position pickupPos;
            pickupPos.x = port_.x; pickupPos.y = shuttleY;
            pickupPos.z = 1;       pickupPos.side = 1;
            s.assignInputMission(pickupPos, *freePos);
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

// Returns the slot (on level y) closest to the port that holds a box of family f,
// skipping slots already claimed. Returns nullopt if none found.
// Called by: findBestBoxForShuttle (Aisle.cpp:296).
std::optional<Position> Aisle::findNearestWithFamily(const Family& f, int y) const {
    if (y < 1 || y > numY_) return std::nullopt;
    int bestDist = INT_MAX;
    std::optional<Position> best;
    for (int s = 1; s <= numSides_; ++s) {
        LevelKey key{s, y};
        auto cit = claimedInputSlots_.find(key);
        for (int x = 0; x < length_; ++x) {
            if (cit != claimedInputSlots_.end() && cit->second.count(x)) continue;
            const Slot& slot = slots_[s-1][y-1][x];
            auto check = [&](const Box* b, int z) {
                if (!b || b->family() != f) return;
                int dist = std::abs(x - port_.x);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = Position{x, y, z, s};
                }
            };
            check(slot.peek(), 1);
            check(slot.peekZ2(), 2);
        }
    }
    return best;
}

void Aisle::assignInstructions() {
    // Input
    for (auto& shuttle : shuttles_) assignNextTo(shuttle);

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
        auto freePos = findBestInputSlot(pendingInputBoxes_.front(), shuttle.yLevel());
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
    meta_.avgDistanceByFamily.clear();
    meta_.freeSlots = 0;

    std::unordered_map<Family, int> distSum;
    std::unordered_map<Family, int> distCount;

    for (int s = 1; s <= numSides_; ++s) {
        for (int y = 1; y <= numY_; ++y) {
            for (int x = 0; x < length_; ++x) {
                const Slot& slot = slots_[s-1][y-1][x];

                if (const Box* b = slot.peek()) {
                    const Family& f = b->family();
                    ++meta_.countByFamily[f];
                    int dist = std::abs(x - port_.x);
                    distSum[f]   += dist;
                    distCount[f] += 1;
                    auto nearIt = meta_.nearestByFamily.find(f);
                    Position p{x, y, 1, s};
                    if (nearIt == meta_.nearestByFamily.end()) {
                        meta_.nearestByFamily[f] = p;
                    } else {
                        if (dist < std::abs(nearIt->second.x - port_.x))
                            nearIt->second = p;
                    }
                }
                if (const Box* b = slot.peekZ2()) {
                    const Family& f = b->family();
                    ++meta_.countByFamily[f];
                    distSum[f]   += std::abs(x - port_.x);
                    distCount[f] += 1;
                }
                if (slot.isEmpty()) ++meta_.freeSlots;
            }
        }
    }

    for (const auto& [f, sum] : distSum)
        meta_.avgDistanceByFamily[f] = static_cast<float>(sum) / static_cast<float>(distCount[f]);

    meta_.oldestArrivalByFamily = oldestArrivalByFamily_;

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

const std::vector<Shuttle>& Aisle::shuttles() const { return shuttles_; }

AisleSnap Aisle::snapshot() const {
    AisleSnap snap;
    snap.aisle_id = 0;

    for (const auto& shuttle : shuttles_) {
        ShuttleSnap ss;
        ss.y_level  = shuttle.yLevel();
        ss.position = shuttle.position();
        switch (shuttle.phase()) {
            case Shuttle::Phase::Idle:           ss.phase = "Idle";           break;
            case Shuttle::Phase::MovingToPickup: ss.phase = "MovingToPickup"; break;
            case Shuttle::Phase::Loading:        ss.phase = "Loading";        break;
            case Shuttle::Phase::MovingToDrop:   ss.phase = "MovingToDrop";   break;
            case Shuttle::Phase::Unloading:      ss.phase = "Unloading";      break;
        }
        const Box* cb = shuttle.carriedBox();
        ss.is_carrying = (cb != nullptr);
        if (cb) {
            ss.carried_box_id     = cb->id();
            ss.carried_box_family = cb->family();
        }
        int y = shuttle.yLevel();
        for (int side = 1; side <= numSides_; ++side) {
            for (int x = 0; x < length_; ++x) {
                const Slot& slot = slots_[side-1][y-1][x];
                if (const Box* b = slot.peek())
                    ss.floor_boxes.push_back({b->id(), b->family(), b->arrivalTick(), {x, y, 1, side}});
                if (const Box* b = slot.peekZ2())
                    ss.floor_boxes.push_back({b->id(), b->family(), b->arrivalTick(), {x, y, 2, side}});
            }
        }
        snap.shuttles.push_back(std::move(ss));
    }

    // pending_sorted: instructionQueue_ is already sorted by ordenarInstrucciones()
    for (const auto& instr : instructionQueue_) {
        InstructionSnap is;
        is.kind      = (instr.kind == Instruction::Kind::Input) ? "Input" : "Output";
        is.family    = instr.family;
        is.box_id    = instr.box_id;
        is.issued_at = instr.issuedAt;
        is.priority  = instr.priority;
        is.seq       = instr.seq;
        if (instr.kind == Instruction::Kind::Input) {
            is.target = port_;
        } else if (instr.requestedFam) {
            auto it = meta_.nearestByFamily.find(*instr.requestedFam);
            is.target = (it != meta_.nearestByFamily.end()) ? it->second : port_;
        } else {
            is.target = port_;
        }
        snap.pending_sorted.push_back(is);
    }

    // pending_fifo: same set, sorted by arrival sequence
    snap.pending_fifo = snap.pending_sorted;
    std::sort(snap.pending_fifo.begin(), snap.pending_fifo.end(),
              [](const InstructionSnap& a, const InstructionSnap& b) { return a.seq < b.seq; });

    return snap;
}
