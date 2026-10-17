#include <cstdint>
#include "prx/libc/include/General.hpp"

extern "C" {

std::int32_t APS5_VABI sceRegMgrNonSysGetInt(std::int64_t key, std::int32_t unk, std::int32_t* data) {
    (void)key;
    (void)unk;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
