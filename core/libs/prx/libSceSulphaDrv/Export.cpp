#include "prx/libSceSulphaDrv/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceSulphaGetClientInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaGetConnectedClientCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaGetDefaultConfig(void* config) {
    if (config == nullptr) return static_cast<int>(0x804D0003u);
    static constexpr std::uint64_t defaults[]{24, 0x400000, 0xFA100000003};
    std::memcpy(config, defaults, sizeof(defaults));
    return 0;
}

int APS5_VABI sceSulphaGetNeededMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaMessage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaSetBookmark(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaShutdown(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
