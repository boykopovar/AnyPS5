#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

// FreeBSD 11 ABI used by the PS5 SDK: names start at byte 8.
struct GuestDirectoryEntry {
    std::uint32_t fileNumber;
    std::uint16_t recordLength;
    std::uint8_t type;
    std::uint8_t nameLength;
    char name[256];
};
static_assert(offsetof(GuestDirectoryEntry, name) == 8);
static_assert(sizeof(GuestDirectoryEntry) == 264);

extern "C" void* APS5_VABI OpenDescriptorDirectory_nid_no_patch(const char* hostPath, int descriptor, int (APS5_VABI* closeDescriptor)(int), std::uint64_t position,
    int (APS5_VABI* readDescriptor)(int, char*, int), std::int64_t (APS5_VABI* seekDescriptor)(int, std::int64_t, int));
extern "C" GuestDirectoryEntry* APS5_VABI ReadDirectoryUnlocked_nid_no_patch(void* handle, int skip);
extern "C" void APS5_VABI SeekDirectoryUnlocked_nid_no_patch(void* handle, std::int64_t location);
