#include "prx/libSceVdecSavc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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
    if (version != nullptr) *version = 0u;
}

int APS5_VABI sceSdecQueryMemorySw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
