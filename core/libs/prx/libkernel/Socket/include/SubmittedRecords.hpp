#pragma once
#include <deque>
#include <mutex>

namespace KernelSocketPeek {

template <typename Record>
class SubmittedRecords {
public:
    struct Slot {
        Record record;
        bool inUse = false;
        bool submitted = false;
    };

    template <typename Completed>
    Slot* Acquire(Completed completed) {
        std::lock_guard lock(m_mutex);
        for (auto& slot : m_slots) {
            if (!slot.inUse && (!slot.submitted || completed(slot.record))) {
                slot.inUse = true;
                slot.submitted = false;
                return &slot;
            }
        }
        auto& slot = m_slots.emplace_back();
        slot.inUse = true;
        return &slot;
    }

    void Release(Slot* slot, bool submitted) {
        std::lock_guard lock(m_mutex);
        slot->submitted = submitted;
        slot->inUse = false;
    }

private:
    std::mutex m_mutex;
    std::deque<Slot> m_slots;
};

}
