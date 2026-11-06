#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Misc/include/RegisterDefaults.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void* APS5_VABI sceAgcGetRegisterDefaults() {
    return Agc::Command::GetRegisterDefaults(13, false, __func__, Agc::Command::RegisterDefaultsTypeLayout::KeyBankPairs);
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

int APS5_VABI sceAgcGetDefaultCxStateFlat(void* out_state, std::uint32_t size) {
    return Agc::Command::GetDefaultCxStateFlat(out_state, size);
}

}
