#include "prx/libSceAgc/DcbState/include/Init.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_EXPORT("23LRUSvYu1M", sceAgcInit);
int APS5_VABI sceAgcInit(uint32_t* state, uint32_t ver) {
    (void)state;
    (void)ver;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
