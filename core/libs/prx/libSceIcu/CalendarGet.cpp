#include "CalendarGet.hpp"
#include "Status.hpp"
#include "prx/libc/include/General.hpp"

extern "C" std::int32_t APS5_VABI ucal_get_nid_postfix(const UCalendar*, UCalendarDateFields, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    NotImplemented_nid_no_patch(__func__);
}
