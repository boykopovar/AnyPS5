#include "prx/libSceIcu/Calendar.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI ucal_close_nid_postfix(UCalendar* cal) {
    if (!cal) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
