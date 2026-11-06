#include "prx/libSceVdecSavc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

extern "C" {

int APS5_VABI sceSdecCreateSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecDestroySw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSw(std::uint32_t* version) {
    if (version == nullptr) return;
    const std::uint32_t value = 0u;
    const auto preserved = errno;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    errno = preserved;
}

int APS5_VABI sceSdecQueryMemorySw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
