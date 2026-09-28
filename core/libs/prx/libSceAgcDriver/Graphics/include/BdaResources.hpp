#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BDARESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BDARESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"

namespace AgcDriver::Graphics {

class BdaResources {
public:
    explicit BdaResources(const Context& context);
    BdaResources(const Context& context, const GuestBufferMemory& memory);
    VkDescriptorBufferInfo Table() const;
    VkDescriptorBufferInfo Fault() const;
    void CheckFault() const;
    // APS5_PROFILE_DRAW: page tables served from the per-device cache and built anew (cumulative),
    // and how many recent tables the cache holds right now.
    struct TableCacheStats {
        std::uint64_t hits = 0;
        std::uint64_t misses = 0;
        std::size_t held = 0;
        // Hits served by the cached address space's serial alone (no hash, no compare; see
        // GuestBufferMemory::CachedAddressTable), counted in `hits` too.
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
    std::shared_ptr<Buffer> table;
    std::unique_ptr<Buffer> fault;
    std::size_t tableBytes = 0;
};

}

#endif
