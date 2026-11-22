#include "prx/libSceAgc/Misc/include/Suspend.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAgcSuspendPoint(void) {
    AgcDriverSuspendPoint_nid_postfix();
    return 0;
}

int APS5_VABI sceAgcSetSubmitMode(std::uint32_t mode) {
    Agc::Command::Require(mode == 0, __func__, "invalid submit mode");
    return 0;
}

}
