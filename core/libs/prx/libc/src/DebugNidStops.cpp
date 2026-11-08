#include "prx/libc/include/DebugNidStops.hpp"

#include <algorithm>

namespace Debug {

std::uint64_t NidStopRegistry::Hit(const NidBreakpointTable& table, std::uint64_t nid, const std::uint64_t* args, std::size_t argCount) {
    if (!table.IsArmed(nid))
        return 0;

    const std::size_t copied = std::min(argCount, kMaxArgs);
    Slot* slot = nullptr;
    std::uint64_t id = 0;
    {
        std::lock_guard lock(mutex_);
        if (count_ == kCapacity) {
            ++skipped_;
            return 0;
        }
        slot = FindFreeSlot();
        id = nextId_++;
        slot->stop.id = id;
        slot->stop.nid = nid;
        slot->stop.argCount = copied;
        for (std::size_t i = 0; i < copied; ++i)
            slot->stop.args[i] = args[i];
        slot->released.store(false, std::memory_order_relaxed);
        slot->inUse = true;
        ++count_;
    }

    slot->released.wait(false, std::memory_order_acquire);

    std::lock_guard lock(mutex_);
    slot->inUse = false;
    slot->stop = Stop{};
    --count_;
    return id;
}

bool NidStopRegistry::Continue(std::uint64_t id) {
    std::lock_guard lock(mutex_);
    for (Slot& slot : slots_) {
        if (slot.inUse && slot.stop.id == id && !slot.released.load(std::memory_order_acquire)) {
            slot.released.store(true, std::memory_order_release);
            slot.released.notify_all();
            return true;
        }
    }
    return false;
}

std::size_t NidStopRegistry::Size() const {
    std::lock_guard lock(mutex_);
    return count_;
}

std::size_t NidStopRegistry::List(Stop* out, std::size_t capacity) const {
    std::lock_guard lock(mutex_);
    std::size_t written = 0;
    for (const Slot& slot : slots_) {
        if (written == capacity)
            break;
        if (!slot.inUse)
            continue;
        out[written++] = slot.stop;
    }
    return written;
}

std::uint64_t NidStopRegistry::SkippedCount() const {
    std::lock_guard lock(mutex_);
    return skipped_;
}

NidStopRegistry::Slot* NidStopRegistry::FindFreeSlot() {
    for (Slot& slot : slots_) {
        if (!slot.inUse)
            return &slot;
    }
    return nullptr;
}

NidStopRegistry& NidStops() {
    static NidStopRegistry registry;
    return registry;
}

}
