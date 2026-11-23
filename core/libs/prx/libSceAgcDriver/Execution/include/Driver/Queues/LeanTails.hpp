#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_LEANTAILS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_LEANTAILS_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/RegisterLists.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

using GuestRanges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

struct KeptTail {
    const std::uint32_t* control = nullptr;
    std::size_t words = 0;
    std::uint64_t tracked = 0;
    std::uint64_t forgets = 0;
    std::uint64_t stamp = 0;
    Submission copy{};
    GuestRanges sources;
};

class RegisterListCache {
public:
    static constexpr std::uint32_t PairsMode = 0x80000000u;
    static constexpr std::uint32_t MaxPairs = 256;
    static constexpr std::size_t Sets = 2048;
    static constexpr std::size_t Ways = 4;
    static constexpr std::size_t BlockSlots = 4096;
    static constexpr std::uint32_t VolatileDeclines = 2;
    static constexpr std::uint64_t DeclineWindow = 4096;

    struct Entry {
        std::uint64_t address = 0;
        std::uint32_t mode = 0;
        std::uint32_t count = 0;
        std::uint64_t tracked = 0;
        std::uint64_t epoch = 0;
        std::uint64_t used = 0;
        bool live = false;
        bool admitted = false;
        std::vector<std::uint32_t> pairs;

        bool Is(std::uint64_t at, std::uint32_t listMode, std::uint32_t listCount) const { return live && address == at && mode == listMode && count == listCount; }
    };

    static bool Cacheable(std::uint32_t count) { return count != 0 && count <= MaxPairs; }
    Entry* Find(std::uint64_t address, std::uint32_t mode, std::uint32_t count);
    Entry& Place(std::uint64_t address, std::uint32_t mode, std::uint32_t count, bool& evicted);
    bool Volatile(std::uint64_t address) const;
    void Decline(std::uint64_t address, std::uint32_t weight = 1);
    void Tick();
    void Clear();
    std::uint64_t Clock() const { return clock; }

    std::uint64_t stamp = 0;

private:
    struct Block {
        std::uint64_t block = ~std::uint64_t{0};
        std::uint32_t declines = 0;
    };
    static std::size_t setOf(std::uint64_t address, std::uint32_t mode, std::uint32_t count);
    std::vector<Entry> entries;
    std::vector<Block> blocks;
    std::uint64_t clock = 0;
    std::uint64_t declinesSince = 0;
};

struct PendingListHit {
    std::uint32_t ref = 0;
    std::uint32_t cursor = 0;
    std::uint64_t address = 0;
    std::uint32_t mode = 0;
    std::uint32_t count = 0;
    std::uint64_t tracked = 0;
    RegisterListCache::Entry* entry = nullptr;
};

class LeanTails {
public:
    KeptTail& Keep();
    void Unkeep();
    KeptTail* Front(const std::uint32_t* control);
    bool Kept() const { return keptFront != keptEnd; }
    void Pop();
    void ClearKept();
    void TrimKept();
    std::vector<std::uint32_t> TakeCommands();
    void GiveCommands(std::vector<std::uint32_t>&& commands);
    RegisterLists TakeLists();
    void GiveLists(RegisterLists&& lists);

    RegisterListCache cache;
    std::vector<PendingListHit> hits;
    std::vector<GuestMemory::UnchangedQuery> queries;
    std::vector<std::uint32_t> scratch;

private:
    std::vector<KeptTail> kept;
    std::size_t keptFront = 0;
    std::size_t keptEnd = 0;
    std::vector<std::vector<std::uint32_t>> spareCommands;
    std::vector<RegisterLists> spareLists;
    std::size_t spareBytes = 0;
};

}

#endif
