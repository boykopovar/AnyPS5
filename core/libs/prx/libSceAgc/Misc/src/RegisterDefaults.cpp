#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Misc/include/RegisterDefaults.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void* APS5_VABI sceAgcGetRegisterDefaults() {
    // The legacy API has no version argument; use the baseline public table.
    return Agc::Command::GetRegisterDefaults(0, false, __func__);
}

void* APS5_VABI sceAgcGetRegisterDefaultsInternal() {
    return Agc::Command::GetRegisterDefaults(0, true, __func__);
}

void* APS5_VABI sceAgcGetRegisterDefaults2(std::uint32_t version) {
    return Agc::Command::GetRegisterDefaults(version, false, __func__);
}

void* APS5_VABI sceAgcGetRegisterDefaults2Internal(std::uint32_t version) {
    return Agc::Command::GetRegisterDefaults(version, true, __func__);
}

}
