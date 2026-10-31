#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BDARESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BDARESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "Recompiler.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

bool LoopGuardTripped();

class BdaResources {
public:
    explicit BdaResources(const Context& context);
    BdaResources(const Context& context, const GuestBufferMemory& memory);
    VkDescriptorBufferInfo Table() const;
    VkDescriptorBufferInfo Fault() const;
    void CheckFault() const;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> TakeWrittenPages() const;
    std::vector<GuestBufferMemory::StorePage> TakeStorePages() const;
    bool StorePages() const { return pages != nullptr; }
    void RecordStorePageGather(Recorder& recorder) const;
    static std::uint32_t StorePageSlots();
    struct ImageTableFaults {
        std::uint32_t stage = 0;
        std::uint64_t shader = 0;
        std::vector<ShaderRecompiler::ImageTableEntryPoison> poison;
        std::vector<std::shared_ptr<const std::string>> messages;
    };
    void SetImageTables(std::vector<ImageTableFaults> tables, std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges);
    struct SrtGuardFaults {
        std::uint32_t stage = 0;
        std::vector<std::uint32_t> flattenedSrt;
    };
    void SetSrtGuards(std::vector<SrtGuardFaults> guards);
    // APS5_PROFILE_DRAW: page tables served from the per-device cache and built anew (cumulative),
    // and how many recent tables the cache holds right now.
    struct TableCacheStats {
        std::uint64_t hits = 0;
        std::uint64_t misses = 0;
        std::size_t held = 0;
        std::uint64_t spaceTables = 0;
        // Why the most recently used table did not serve a build ([bda-table] first-differing entry
        // class): its owners were gone (expired weak reference), or its word hash differed, split by
        // whether the first differing range begins below 0x10000000 (a low mapping: the exe image,
        // the system heaps) or in the guest heap above; `sameHash` is a hash match whose ranges
        // still differed, `empty` a lookup with no entry held.
        std::uint64_t firstExpired = 0;
        std::uint64_t firstDiffersLow = 0;
        std::uint64_t firstDiffersHeap = 0;
        std::uint64_t firstSameHash = 0;
        std::uint64_t firstEmpty = 0;
    };
    static TableCacheStats TableCacheCounters();

private:
    // The page table is read-only to the shader, so consecutive builds mapping the same ranges to the
    // same device addresses share one buffer while one of them is alive (see the table cache in
    // BdaResources.cpp, which refers to it weakly).
    void markWrittenPages() const;
    void collectStorePages() const;
    void attachStorePages() const;
    void checkSrtGuard() const;
    std::shared_ptr<Buffer> table;
    std::unique_ptr<Buffer> fault;
    mutable std::unique_ptr<Buffer> pages;
    VkDevice poolDevice = VK_NULL_HANDLE;
    std::unique_ptr<Buffer> targets;
    std::uint32_t pageSlots = 0;
    std::uint32_t targetCount = 0;
    std::uint64_t pageBase = 0;
    mutable bool pagesOverflowed = false;
    mutable std::vector<GuestBufferMemory::StorePage> storePages;
    std::size_t tableBytes = 0;
    std::vector<ImageTableFaults> imageTables;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> imageTableRanges;
    std::vector<SrtGuardFaults> srtGuards;
    mutable std::vector<std::pair<std::uint64_t, std::uint64_t>> writtenPages;
};

}

#endif
