#include "prx/libSceAgc/DcbState/include/Init.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcInit(std::uint32_t version) {
    if (version >= 14) return 0;
    Agc::Command::GetRegisterDefaults(version, false, __func__);
    Agc::Command::GetRegisterDefaults(version, true, __func__);
    return 0;
}

int APS5_VABI sceAgcInit_0090(std::uint32_t* state, std::uint32_t version) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(state), alignof(std::uint32_t), __func__);
    return sceAgcInit(version);
}

}
