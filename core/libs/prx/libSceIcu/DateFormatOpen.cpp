#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t,
                                                        const char16_t*, std::int32_t, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}
