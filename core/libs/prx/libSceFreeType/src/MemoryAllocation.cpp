#include "prx/libSceFreeType/include/MemoryAllocation.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace {

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void RequireWritable(void* destination, std::size_t bytes, const char* caller) {
    if (!GuestWritable_nid_no_patch(destination, bytes)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

FreeType::GuestMemoryRec Record(FreeType::GuestMemoryRec* memory, const char* caller) {
    FreeType::GuestMemoryRec record{};
    CopyIn(&record, memory, sizeof(record), caller);
    return record;
}

}

extern "C" std::int32_t APS5_VABI FT_Alloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** block) {
    if (!memory || !block) throw std::runtime_error("FT_Alloc: null memory record or output is not implemented");
    RequireWritable(block, sizeof(*block), __func__);
    void* allocated = nullptr;
    std::int32_t error = 0;
    if (size > 0) {
        allocated = Record(memory, __func__).alloc(memory, size);
        if (allocated) std::memset(allocated, 0, static_cast<std::size_t>(size));
        else error = FT_Err_Out_Of_Memory;
    } else if (size < 0) {
        error = FT_Err_Invalid_Argument;
    }
    CopyOut(block, &allocated, sizeof(allocated), __func__);
    return error;
}

extern "C" std::int32_t APS5_VABI FT_QAlloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** block) {
    if (!memory || !block) throw std::runtime_error("FT_QAlloc: null memory record or output is not implemented");
    RequireWritable(block, sizeof(*block), __func__);
    void* allocated = nullptr;
    std::int32_t error = 0;
    if (size > 0) {
        allocated = Record(memory, __func__).alloc(memory, size);
        if (!allocated) error = FT_Err_Out_Of_Memory;
    } else if (size < 0) {
        error = FT_Err_Invalid_Argument;
    }
    CopyOut(block, &allocated, sizeof(allocated), __func__);
    return error;
}

extern "C" void APS5_VABI FT_Free_nid_postfix(FreeType::GuestMemoryRec* memory, void** block) {
    if (!memory || !block) return;
    void* existing = nullptr;
    CopyIn(&existing, block, sizeof(existing), __func__);
    if (!existing) return;
    RequireWritable(block, sizeof(*block), __func__);
    Record(memory, __func__).free(memory, existing);
    void* const cleared = nullptr;
    CopyOut(block, &cleared, sizeof(cleared), __func__);
}

namespace {

std::int32_t Reallocate(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void** block, bool zeroGrowth, const char* caller) {
    void* existing = nullptr;
    CopyIn(&existing, block, sizeof(existing), caller);
    RequireWritable(block, sizeof(*block), caller);
    void* result = existing;
    std::int32_t error = 0;
    if (current < 0 || size < 0) {
        error = FT_Err_Invalid_Argument;
    } else if (size == 0) {
        if (existing) Record(memory, caller).free(memory, existing);
        result = nullptr;
    } else if (size > 0x7fffffffll) {
        error = FT_Err_Array_Too_Large;
    } else if (current == 0) {
        result = Record(memory, caller).alloc(memory, size);
        if (result) std::memset(result, 0, static_cast<std::size_t>(size));
        else error = FT_Err_Out_Of_Memory;
    } else {
        void* const moved = Record(memory, caller).realloc(memory, current, size, existing);
        if (moved) result = moved;
        else error = FT_Err_Out_Of_Memory;
    }
    if (!error && zeroGrowth && size > current && result)
        std::memset(static_cast<char*>(result) + current, 0, static_cast<std::size_t>(size - current));
    CopyOut(block, &result, sizeof(result), caller);
    return error;
}

}

extern "C" std::int32_t APS5_VABI FT_Realloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void** block) {
    if (!memory || !block) throw std::runtime_error("FT_Realloc: null memory record or output is not implemented");
    return Reallocate(memory, current, size, block, true, __func__);
}

extern "C" std::int32_t APS5_VABI FT_QRealloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void** block) {
    if (!memory || !block) throw std::runtime_error("FT_QRealloc: null memory record or output is not implemented");
    return Reallocate(memory, current, size, block, false, __func__);
}
