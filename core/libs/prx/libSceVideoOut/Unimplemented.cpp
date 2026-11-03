#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include <stdexcept>

extern "C" {

int APS5_VABI sceDbgVideoOutAddOutputModeVr60Privilege() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
