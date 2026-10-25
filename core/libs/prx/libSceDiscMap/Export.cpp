#include "prx/libSceDiscMap/include/Api.hpp"
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
std::int32_t APS5_VABI sceDiscMapUnknown_ioKMruft1ek(const char* path, std::int64_t offset, std::int64_t size, std::uint64_t* flags, std::uint64_t* out1, std::uint64_t* out2) {
    (void)path;
    (void)offset;
    (void)size;
    (void)flags;
    (void)out1;
    (void)out2;
    return SCE_DISC_MAP_ERROR_NO_BITMAP_INFO;
}

int APS5_VABI sceDiscMapGetPackageSize(std::int64_t fflags, int* ret1, int* ret2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
