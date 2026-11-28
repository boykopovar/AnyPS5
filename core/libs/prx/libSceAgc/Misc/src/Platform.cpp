#include "prx/libSceAgc/Misc/include/Platform.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcGetIsTrinityMode(bool* isTrinityMode) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(isTrinityMode), alignof(bool), __func__);
    const bool trinity = false;
    Agc::Command::Require(GuestCopyOut_nid_no_patch(isTrinityMode, &trinity, sizeof(trinity)), __func__, "destination pointer is not writable");
    return 0;
}

}
