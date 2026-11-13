#include "prx/libSceVdecSavc2/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

namespace {

bool DestroyedWithoutCodec(void* decoder) {
    const int savedErrno = errno;
    std::uint64_t codec = 0;
    std::uint64_t field = 0;
    bool done = GuestCopyIn_nid_no_patch(&codec, static_cast<unsigned char*>(decoder) + 0x10, sizeof(codec)) &&
                codec == 0 &&
                GuestCopyIn_nid_no_patch(&field, static_cast<unsigned char*>(decoder) + 0x8, sizeof(field));
    if (done && field != 0) {
        const std::uint64_t zero = 0;
        done = GuestCopyOut_nid_no_patch(static_cast<unsigned char*>(decoder) + 0x8, &zero, sizeof(zero));
    }
    errno = savedErrno;
    return done;
}

} // namespace

extern "C" {

int APS5_VABI sceSdecCreateSw2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecDestroySw2(void* decoder) {
    if (decoder == nullptr || DestroyedWithoutCodec(decoder)) return 0;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSw2(std::uint32_t* version) {
    if (version == nullptr) return;
    const std::uint32_t value = 0x13590001u;
    const auto preserved = errno;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    errno = preserved;
}

int APS5_VABI sceSdecQueryMemorySw2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
