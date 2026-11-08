#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI ucal_setMillis_nid_postfix(UCalendar*, UDate, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    NotImplemented_nid_no_patch(__func__);
}

}
