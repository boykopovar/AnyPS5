#include "prx/libSceIcu/ListFormatterStringsToResult.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter*, const char16_t* const[],
                                                          const std::int32_t*, std::int32_t, UFormattedList* uresult,
                                                          std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!uresult) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
