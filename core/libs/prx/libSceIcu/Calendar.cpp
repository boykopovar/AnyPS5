#include "prx/libSceIcu/Calendar.hpp"
#include "prx/libSceIcu/Status.hpp"

extern "C" {

void APS5_VABI ucal_setGregorianChange_nid_postfix(UCalendar* cal, UDate, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!cal) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
