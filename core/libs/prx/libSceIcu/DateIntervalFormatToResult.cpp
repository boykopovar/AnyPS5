#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat*, double, double, UFormattedDateInterval* result, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!result) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
