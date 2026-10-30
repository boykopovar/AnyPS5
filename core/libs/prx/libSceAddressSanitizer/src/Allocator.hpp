#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace AsanRuntime::Heap {

enum class Access {
    Unknown,
    Inside,
    Before,
    After,
};

struct ChunkView {
    std::uintptr_t begin;
    std::size_t size;
    bool quarantined;
    std::uintptr_t allocPc;
    std::uintptr_t freePc;
    std::uint32_t allocTid;
    std::uint32_t freeTid;
};

struct Stats {
    std::uint64_t mallocs;
    std::uint64_t malloced;
    std::uint64_t mallocedRedzones;
    std::uint64_t frees;
    std::uint64_t freed;
    std::uint64_t realFrees;
    std::uint64_t reallyFreed;
    std::uint64_t reallocs;
    std::uint64_t realloced;
    std::uint64_t mmaps;
    std::uint64_t mmaped;
    std::uint64_t munmaps;
    std::uint64_t munmaped;
    std::uint64_t mallocLarge;
    std::uint64_t mallocedBySize[54];
};

void* Allocate_nid_no_patch(std::size_t size, std::size_t alignment, std::uintptr_t pc, bool fill);
void Deallocate_nid_no_patch(void* pointer, std::uintptr_t pc);
void* Calloc_nid_no_patch(std::size_t count, std::size_t size, std::uintptr_t pc);
void* Reallocate_nid_no_patch(void* pointer, std::size_t size, std::uintptr_t pc);
void* Memalign_nid_no_patch(std::size_t alignment, std::size_t size, std::uintptr_t pc);
void* AlignedAlloc_nid_no_patch(std::size_t alignment, std::size_t size, std::uintptr_t pc);
int PosixMemalign_nid_no_patch(void** memptr, std::size_t alignment, std::size_t size, std::uintptr_t pc);
std::size_t UsableSize_nid_no_patch(const void* pointer, std::uintptr_t pc);
std::size_t AllocationSize_nid_no_patch(std::uintptr_t address);
std::size_t AllocatedSizeChecked_nid_no_patch(const void* pointer, std::uintptr_t pc);
const void* AllocationBegin_nid_no_patch(const void* pointer);
bool UpdateContext_nid_no_patch(std::uintptr_t address, std::uintptr_t pc);
bool FindChunk_nid_no_patch(std::uintptr_t address, ChunkView& view);
Access Classify_nid_no_patch(const ChunkView& view, std::uintptr_t address, std::size_t accessSize, std::intptr_t& offset);
void Purge_nid_no_patch();
Stats GetStats_nid_no_patch();
std::uint64_t MaxMalloced_nid_no_patch();
std::string DescribeChunk_nid_no_patch(std::uintptr_t address, std::size_t accessSize);
std::uint32_t CurrentTid_nid_no_patch();
std::size_t CurrentAllocatedBytes_nid_no_patch();
std::size_t HeapSize_nid_no_patch();
std::size_t FreeBytes_nid_no_patch();
std::string FormatStats_nid_no_patch();

}  // namespace AsanRuntime::Heap
