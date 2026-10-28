#include "prx/libSceFreeType/include/MemoryAllocation.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>
#include <stdexcept>

extern "C" std::int32_t APS5_VABI FT_Alloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** block) {
    if (!memory || !block) throw std::runtime_error("FT_Alloc: null memory record or output is not implemented");
    void* allocated = nullptr;
    std::int32_t error = 0;
    if (size > 0) {
        allocated = memory->alloc(memory, size);
        if (allocated) std::memset(allocated, 0, static_cast<std::size_t>(size));
        else error = FT_Err_Out_Of_Memory;
    } else if (size < 0) {
        error = FT_Err_Invalid_Argument;
    }
    *block = allocated;
    return error;
}

extern "C" std::int32_t APS5_VABI FT_QAlloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** block) {
    if (!memory || !block) throw std::runtime_error("FT_QAlloc: null memory record or output is not implemented");
    void* allocated = nullptr;
    std::int32_t error = 0;
    if (size > 0) {
        allocated = memory->alloc(memory, size);
        if (!allocated) error = FT_Err_Out_Of_Memory;
    } else if (size < 0) {
        error = FT_Err_Invalid_Argument;
    }
    *block = allocated;
    return error;
}

extern "C" void APS5_VABI FT_Free_nid_postfix(FreeType::GuestMemoryRec* memory, void** block) {
    if (!memory || !block || !*block) return;
    memory->free(memory, *block);
    *block = nullptr;
}
