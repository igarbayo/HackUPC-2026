#include "Aisle.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <climits>

// Constructor
Aisle::Aisle(int length, int numShuttles, Position port)
    : length_(length), port_(port)
{
    slots_.reserve(length);
    for (int i = 0; i < length; ++i) {
        slots_.emplace_back(Position{i, 0});
        freeSlotXs_.insert(i);
    }
    // Shuttles start at head (port)
    for (int i = 0; i < numShuttles; ++i) {
        shuttles_.emplace_back(port_);
    }
    updateMeta();
}

// Función que recoje una caja y decide donde meterla
void Aisle::input(Box newBox) {
    pendingInputBoxes_.push(std::move(newBox));
    Instruction instr;
    instr.kind      = Instruction::Kind::Input;
    instr.issuedAt  = currentTick_;
    instr.priority  = 0;
    instructionQueue_.push_back(std::move(instr));
    updateMeta();
}

// Pide una caja de una familia concreta
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

// Función para recoger una caja lista (primera hay que pedirla)
std::optional<Box> Aisle::collectReadyOutput(Family f) {
    auto it = readyOutputs_.find(f);
    if (it == readyOutputs_.end() || it->second.empty()) return std::nullopt;
    Box b = std::move(it->second.front());
    it->second.pop();
    return b;
}

// Función que ordena las instrucciones por prioridad dinámica
void Aisle::ordenarInstrucciones() {
    for (auto& instr : instructionQueue_) {
        Tick waited = currentTick_ - instr.issuedAt;
        instr.priority = static_cast<int>(waited);
        // Output instructions get a small boost if family is abundant
        if (instr.kind == Instruction::Kind::Output && instr.requestedFam) {
            auto it = meta_.countByFamily.find(*instr.requestedFam);
            if (it != meta_.countByFamily.end() && it->second > 5) {
                instr.priority += 2;
            }
        }
    }
    std::stable_sort(instructionQueue_.begin(), instructionQueue_.end(),
                     [](const Instruction& a, const Instruction& b) {
                         return a.priority > b.priority;
                     });
}

// Getters
Aisle::Metadata Aisle::metadata() const { return meta_; }
Position        Aisle::port()     const { return port_; }

// Función que orchestra el flujo de Aisle
void Aisle::tick() {
    ++currentTick_;
    ordenarInstrucciones();
    for (auto& shuttle : shuttles_) shuttle.tick(*this);
    assignInstructions();  // bootstrap: catch idle shuttles with new instructions this tick
    updateMeta();
}

void Aisle::setEventLog(std::vector<Event>* log) { eventLog_ = log; }

void Aisle::notifyBoxPlaced(const Box& b, Position where) {
    freeSlotXs_.erase(where.x);
    if (eventLog_)
        eventLog_->push_back({"box_stored", currentTick_, b.id(), -1, b.family()});
}

void Aisle::notifyBoxTaken(const Box& b, Position where) {
    freeSlotXs_.insert(where.x);
    if (eventLog_)
        eventLog_->push_back({"box_retrieved", currentTick_, b.id(), -1, b.family()});
}

void Aisle::notifyOutputReady(Box b) {
    // Decrement in-flight reservation
    auto it = outputReservedByFamily_.find(b.family());
    if (it != outputReservedByFamily_.end() && it->second > 0) {
        --it->second;
    }
    readyOutputs_[b.family()].push(std::move(b));
}

std::optional<Box> Aisle::takeFromInputBuffer() {
    if (pendingInputBoxes_.empty()) return std::nullopt;
    Box b = std::move(pendingInputBoxes_.front());
    pendingInputBoxes_.pop();
    return b;
}

Slot& Aisle::slotAt(Position pos) {
    if (pos.x < 0 || pos.x >= length_)
        throw std::out_of_range("Aisle::slotAt: position out of range");
    return slots_[pos.x];
}

const Slot& Aisle::slotAt(Position pos) const {
    if (pos.x < 0 || pos.x >= length_)
        throw std::out_of_range("Aisle::slotAt: position out of range");
    return slots_[pos.x];
}

std::optional<Position> Aisle::findNearestWithFamily(const Family& f, Position reference) const {
    int bestDist = INT_MAX;
    std::optional<Position> best;
    for (const auto& slot : slots_) {
        if (!slot.isEmpty() && slot.peek()->family() == f) {
            int dist = std::abs(slot.position().x - reference.x);
            if (dist < bestDist) {
                bestDist = dist;
                best = slot.position();
            }
        }
    }
    return best;
}

// preferNear=true  → smallest x (hot family, fast retrieval near port)
// preferNear=false → largest x  (cold family, out of the way)
std::optional<Position> Aisle::findFreeSlot(bool preferNear) const {
    if (freeSlotXs_.empty()) return std::nullopt;
    int x = preferNear ? *freeSlotXs_.begin() : *freeSlotXs_.rbegin();
    return Position{x, 0};
}

void Aisle::assignNextTo(Shuttle& s) {
    if (!s.isFree() || instructionQueue_.empty()) return;

    for (auto it = instructionQueue_.begin(); it != instructionQueue_.end(); ++it) {
        if (it->kind == Instruction::Kind::Input) {
            if (pendingInputBoxes_.empty()) continue;
            const Family& fam = pendingInputBoxes_.front().family();
            bool isHot = outputReservedByFamily_.count(fam) &&
                         outputReservedByFamily_.at(fam) > 0;
            auto freePos = findFreeSlot(isHot);
            if (!freePos) continue;
            s.assignInputMission(port_, *freePos);
            instructionQueue_.erase(it);
            return;
        } else {
            if (!it->requestedFam) continue;
            auto nearestPos = findNearestWithFamily(*it->requestedFam, port_);
            if (!nearestPos) continue;
            if (slotAt(*nearestPos).isEmpty()) continue;
            s.assignOutputMission(*nearestPos, port_);
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
    meta_.freeSlots = static_cast<int>(freeSlotXs_.size());

    for (const auto& slot : slots_) {
        if (!slot.isEmpty()) {
            const Box* b = slot.peek();
            const Family& f = b->family();
            ++meta_.countByFamily[f];

            auto nearIt = meta_.nearestByFamily.find(f);
            if (nearIt == meta_.nearestByFamily.end()) {
                meta_.nearestByFamily[f] = slot.position();
            } else {
                int curDist = std::abs(nearIt->second.x - port_.x);
                int newDist = std::abs(slot.position().x - port_.x);
                if (newDist < curDist) {
                    nearIt->second = slot.position();
                }
            }
        }
    }

    meta_.pendingInputs = 0;
    meta_.pendingOutputs = 0;
    for (const auto& instr : instructionQueue_) {
        if (instr.kind == Instruction::Kind::Input) ++meta_.pendingInputs;
        else ++meta_.pendingOutputs;
    }

    meta_.readyOutputCount = 0;
    for (const auto& [f, q] : readyOutputs_)
        meta_.readyOutputCount += static_cast<int>(q.size());

    meta_.activeShuttles = 0;
    for (const auto& s : shuttles_)
        if (!s.isFree()) ++meta_.activeShuttles;

    meta_.reservedByFamily = outputReservedByFamily_;
}
