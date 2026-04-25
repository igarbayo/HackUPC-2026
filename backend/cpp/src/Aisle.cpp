#include "Aisle.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <climits>

Aisle::Aisle(int length, int numShuttles, Position inputPort, Position outputPort)
    : length_(length), inputPort_(inputPort), outputPort_(outputPort)
{
    slots_.reserve(length);
    for (int i = 0; i < length; ++i) {
        slots_.emplace_back(Position{i, 0});
    }
    // Shuttles start at middle of aisle
    int midX = length / 2;
    for (int i = 0; i < numShuttles; ++i) {
        shuttles_.emplace_back(Position{midX + i, 0});
    }
    updateMeta();
}

void Aisle::input(Box newBox) {
    pendingInputBoxes_.push(std::move(newBox));
    Instruction instr;
    instr.kind      = Instruction::Kind::Input;
    instr.issuedAt  = currentTick_;
    instr.priority  = 0;
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
    // Recalculate priorities based on wait time and type
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

Aisle::Metadata Aisle::metadata() const { return meta_; }
Position        Aisle::inputPort()  const { return inputPort_; }
Position        Aisle::outputPort() const { return outputPort_; }

void Aisle::tick() {
    ++currentTick_;
    ordenarInstrucciones();
    assignInstructions();
    for (auto& shuttle : shuttles_) {
        shuttle.tick(*this);
    }
    updateMeta();
}

void Aisle::setEventLog(std::vector<Event>* log) { eventLog_ = log; }

void Aisle::notifyBoxPlaced(const Box& b, Position where) {
    (void)where;
    if (eventLog_)
        eventLog_->push_back({"box_stored", currentTick_, b.id(), -1, b.family()});
}

void Aisle::notifyBoxTaken(const Box& b, Position where) {
    (void)where;
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

std::optional<Position> Aisle::findFreeSlot() const {
    // Find free slot, preferring the one farthest from output port (leave near slots for fast retrieval)
    int bestDist = -1;
    std::optional<Position> best;
    for (const auto& slot : slots_) {
        if (slot.isEmpty()) {
            int dist = std::abs(slot.position().x - outputPort_.x);
            if (dist > bestDist) {
                bestDist = dist;
                best = slot.position();
            }
        }
    }
    return best;
}

void Aisle::assignInstructions() {
    for (auto& shuttle : shuttles_) {
        if (!shuttle.isFree() || instructionQueue_.empty()) continue;

        for (auto it = instructionQueue_.begin(); it != instructionQueue_.end(); ++it) {
            if (it->kind == Instruction::Kind::Input) {
                if (pendingInputBoxes_.empty()) continue;
                auto freePos = findFreeSlot();
                if (!freePos) continue;
                shuttle.assignInputMission(inputPort_, *freePos);
                instructionQueue_.erase(it);
                break;
            } else { // Output
                if (!it->requestedFam) continue;
                auto nearestPos = findNearestWithFamily(*it->requestedFam, outputPort_);
                if (!nearestPos) {
                    // Family not available yet; skip this instruction for now
                    continue;
                }
                // Check if another shuttle is already heading to this slot
                bool alreadyTargeted = false;
                // Simple check: skip if slot is empty (another shuttle may have taken it)
                if (slotAt(*nearestPos).isEmpty()) { alreadyTargeted = true; }
                if (alreadyTargeted) continue;

                shuttle.assignOutputMission(*nearestPos, outputPort_);
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

    for (const auto& slot : slots_) {
        if (slot.isEmpty()) {
            ++meta_.freeSlots;
        } else {
            const Box* b = slot.peek();
            const Family& f = b->family();
            ++meta_.countByFamily[f];

            auto nearIt = meta_.nearestByFamily.find(f);
            if (nearIt == meta_.nearestByFamily.end()) {
                meta_.nearestByFamily[f] = slot.position();
            } else {
                int curDist = std::abs(nearIt->second.x - outputPort_.x);
                int newDist = std::abs(slot.position().x - outputPort_.x);
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
