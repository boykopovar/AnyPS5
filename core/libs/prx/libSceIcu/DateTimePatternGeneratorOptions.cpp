#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(
    UDateTimePatternGenerator*,
    const char16_t* skeleton,
    std::int32_t length,
    UDateTimePatternMatchOptions,
    char16_t*,
    std::int32_t,
    std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) {
        return 0;
    }
    if (!skeleton && length != 0) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
