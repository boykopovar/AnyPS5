#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_DISC_MAP_ERROR_NO_BITMAP_INFO = static_cast<std::int32_t>(0x81100004);

}

extern "C" {

std::int32_t APS5_VABI sceDiscMapIsRequestOnHDD(const char* path, std::int64_t offset, std::int64_t size, std::int32_t* result) {
    (void)path;
    (void)offset;
    (void)size;
    (void)result;
    return SCE_DISC_MAP_ERROR_NO_BITMAP_INFO;
}

APS5_EXPORT("ioKMruft1ek", sceDiscMapUnknown_ioKMruft1ek);
std::int32_t APS5_VABI sceDiscMapUnknown_ioKMruft1ek(const char* path, std::int64_t offset, std::int64_t size, void* flags, void* out1, void* out2) {
    (void)path;
    (void)offset;
    (void)size;
    (void)flags;
    (void)out1;
    (void)out2;
    return SCE_DISC_MAP_ERROR_NO_BITMAP_INFO;
}

}
