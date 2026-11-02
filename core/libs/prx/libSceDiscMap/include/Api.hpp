#pragma once

#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI sceDiscMapIsRequestOnHDD(const char* path, std::int64_t offset, std::int64_t size, std::int32_t* result);
std::int32_t APS5_VABI sceDiscMapUnknown_ioKMruft1ek(const char* path, std::int64_t offset, std::int64_t size, std::uint64_t* flags, std::uint64_t* out1, std::uint64_t* out2);
int APS5_VABI sceDiscMapGetPackageSize(std::int64_t fflags, int* ret1, int* ret2);

}
