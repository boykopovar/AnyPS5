#include "prx/libSceIcu/DateTimePatternGenerator.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(UDateTimePatternGenerator*, const char16_t* pattern, std::int32_t length,
                                                     char16_t*, std::int32_t, std::int32_t* errorCode) {
    if (Icu::Failure(errorCode)) return 0;
    if (!pattern && length != 0) {
        *errorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
