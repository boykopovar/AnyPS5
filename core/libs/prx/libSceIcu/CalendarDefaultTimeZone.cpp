#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t* ec) {
    if (!ec || *ec > 0) return 0;
    NotImplemented_nid_no_patch(__func__);
}

}
