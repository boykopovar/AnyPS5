#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {
using MemoryAllocate = void* (APS5_VABI*)(const void*, std::size_t);
using MemoryReallocate = void* (APS5_VABI*)(const void*, void*, std::size_t);
using MemoryFree = void (APS5_VABI*)(const void*, void*);
} // namespace Icu

extern "C" {
void APS5_VABI u_setMutexFunctions_nid_postfix(const void*, const void*, const void*, const void*, const void*, std::int32_t*);
void APS5_VABI u_setAtomicIncDecFunctions_nid_postfix(const void*, const void*, const void*, std::int32_t*);
void APS5_VABI u_setMemoryFunctions_nid_postfix(const void*, Icu::MemoryAllocate, Icu::MemoryReallocate, Icu::MemoryFree, std::int32_t*);
const char* APS5_VABI u_getDataDirectory_nid_postfix();
void APS5_VABI u_setDataDirectory_nid_postfix(const char*);
const char* APS5_VABI u_getTimeZoneFilesDirectory_nid_postfix(std::int32_t*);
void APS5_VABI u_setTimeZoneFilesDirectory_nid_postfix(const char*, std::int32_t*);
void APS5_VABI u_cleanup_nid_postfix();
void* APS5_VABI uprv_malloc_nid_postfix(std::size_t);
void* APS5_VABI uprv_realloc_nid_postfix(void*, std::size_t);
char* APS5_VABI uprv_strdup_nid_postfix(const char*);
char* APS5_VABI uprv_strndup_nid_postfix(const char*, std::int32_t);
}
