#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDSTOPS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDSTOPS_HPP

#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace Debug {

class NidStopRegistry {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr std::size_t kMaxArgs = 6;

    struct Stop {
        std::uint64_t id = 0;
        std::uint64_t nid = 0;
        std::size_t argCount = 0;
        std::uint64_t args[kMaxArgs] = {};
    };

    std::uint64_t Hit(const NidBreakpointTable& table, std::uint64_t nid, const std::uint64_t* args, std::size_t argCount);
    bool Continue(std::uint64_t id);
    std::size_t Size() const;
    std::size_t List(Stop* out, std::size_t capacity) const;
    std::uint64_t SkippedCount() const;

private:
    struct Slot {
        Stop stop;
        std::atomic<bool> released{false};
        bool inUse = false;
    };

    Slot* FindFreeSlot();

    mutable std::mutex mutex_;
    std::array<Slot, kCapacity> slots_;
    std::size_t count_ = 0;
    std::uint64_t nextId_ = 1;
    std::uint64_t skipped_ = 0;
};

NidStopRegistry& NidStops();

}

#endif
