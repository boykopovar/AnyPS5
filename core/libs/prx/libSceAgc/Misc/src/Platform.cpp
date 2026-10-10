#include "prx/libSceAgc/Misc/include/Platform.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcGetIsTrinityMode(bool* isTrinityMode) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(isTrinityMode), alignof(bool), __func__);
    *isTrinityMode = false;
    return 0;
}

int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags) {
    if (flags != 0) NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation() {
    return 0;
}

}
