#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void*, std::int32_t, const char*, char16_t*,
                                                               std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    NotImplemented_nid_no_patch(__func__);
}

}
