#include "prx/libSceIcu/NumberFormatDecimal.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const UNumberFormat*, const char*, std::int32_t,
                                                      char16_t* result, std::int32_t resultLength, UFieldPosition*,
                                                      std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if ((result == nullptr && resultLength != 0) || resultLength < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
