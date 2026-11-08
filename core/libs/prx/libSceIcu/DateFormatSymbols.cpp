#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*) {
    if (!fmt) return -1;
    NotImplemented_nid_no_patch(__func__);
}

}
