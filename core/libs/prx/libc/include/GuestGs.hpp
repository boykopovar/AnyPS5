#pragma once

#include <cstdint>

extern "C" std::uint64_t GuestGsGet_nid_no_patch();
extern "C" void GuestGsSet_nid_no_patch(std::uint64_t base);

#ifdef _WIN32
struct _EXCEPTION_POINTERS;

extern "C" bool GuestGsIsReplay_nid_no_patch(std::uintptr_t address);
extern "C" bool GuestGsRegisterImage_nid_no_patch(const void* base);

bool GuestGsHandleException_nid_no_patch(_EXCEPTION_POINTERS* info);
#endif
