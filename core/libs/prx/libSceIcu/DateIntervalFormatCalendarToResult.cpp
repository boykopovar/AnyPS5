#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"

extern "C" void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval* result, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!result) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}
