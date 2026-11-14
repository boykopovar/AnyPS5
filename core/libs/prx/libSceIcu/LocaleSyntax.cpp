#include "prx/libSceIcu/LocaleSyntax.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uloc.h"

extern "C" {

std::int32_t APS5_VABI uloc_getVariant_nid_postfix(const char* locale, char* result,
                                                 std::int32_t capacity, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    if (*status > 0) return 0;
    if (!locale) NotImplemented_nid_no_patch(__func__);
    if (!result && capacity > 0) APS5_INVALID_ARG_EX;
    return uloc_getVariant(locale, result, capacity, reinterpret_cast<UErrorCode*>(status));
}

std::int32_t APS5_VABI uloc_setKeywordValue_nid_postfix(const char* keyword, const char* value,
                                                      char* buffer, std::int32_t capacity,
                                                      std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    if (*status > 0) return 0;
    if (keyword && *keyword && capacity > 1 && !buffer) APS5_INVALID_ARG_EX;
    return uloc_setKeywordValue(keyword, value, buffer, capacity, reinterpret_cast<UErrorCode*>(status));
}

}
