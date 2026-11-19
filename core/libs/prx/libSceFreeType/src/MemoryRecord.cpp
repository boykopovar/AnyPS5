#include "prx/libSceFreeType/include/MemoryRecord.hpp"

#include <cstdlib>

namespace {

void* APS5_VABI HostAlloc(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI HostFree(FreeType::GuestMemoryRec* memory, void* block) {
    std::free(block);
}

void* APS5_VABI HostRealloc(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

}

extern "C" FreeType::GuestMemoryRec* APS5_VABI FT_New_Memory_nid_postfix() {
    const auto memory = static_cast<FreeType::GuestMemoryRec*>(std::malloc(sizeof(FreeType::GuestMemoryRec)));
    if (memory) *memory = {nullptr, HostAlloc, HostFree, HostRealloc};
    return memory;
}

extern "C" void APS5_VABI FT_Done_Memory_nid_postfix(FreeType::GuestMemoryRec* memory) {
    std::free(memory);
}
