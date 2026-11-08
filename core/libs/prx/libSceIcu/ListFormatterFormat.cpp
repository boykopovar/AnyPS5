#include "prx/libSceIcu/ListFormatterFormat.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter*, const char16_t* const[],
                                                   const std::int32_t*, std::int32_t, char16_t* result,
                                                   std::int32_t resultCapacity, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if ((result == nullptr) ? resultCapacity != 0 : resultCapacity < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
