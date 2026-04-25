#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "types.h"

class SnapshotQueue {
public:
    void push(TickSnapshot snap) {
        { std::lock_guard<std::mutex> lk(mutex_); queue_.push(std::move(snap)); }
        cv_.notify_one();
    }

    // Blocks until an item is available OR done+empty. Returns nullopt when exhausted.
    std::optional<TickSnapshot> pop() {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this]{ return !queue_.empty() || done_; });
        if (queue_.empty()) return std::nullopt;
        auto snap = std::move(queue_.front());
        queue_.pop();
        return snap;
    }

    void markDone() {
        { std::lock_guard<std::mutex> lk(mutex_); done_ = true; }
        cv_.notify_all();
    }

    bool isDone() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return done_;
    }

private:
    mutable std::mutex       mutex_;
    std::condition_variable  cv_;
    std::queue<TickSnapshot> queue_;
    bool                     done_ = false;
};
