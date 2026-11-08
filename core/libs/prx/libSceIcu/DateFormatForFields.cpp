#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, void* fpositer, std::int32_t* status) {
    (void)format;
    (void)dateToFormat;
    (void)fpositer;
    if (Icu::Failure(status)) return -1;
    if (!result ? resultLength != 0 : resultLength < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
