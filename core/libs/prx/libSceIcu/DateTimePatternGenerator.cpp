#include "prx/libSceIcu/DateTimePatternGenerator.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI udatpg_getBestPattern_nid_postfix(UDateTimePatternGenerator*, const char16_t* skeleton, std::int32_t length,
                                                         char16_t*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) {
        return 0;
    }
    if (!skeleton && length != 0) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
