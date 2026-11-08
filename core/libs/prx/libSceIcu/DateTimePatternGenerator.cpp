#include "prx/libSceIcu/DateTimePatternGenerator.hpp"
#include "prx/libc/include/General.hpp"

extern "C" void APS5_VABI udatpg_close_nid_postfix(UDateTimePatternGenerator* dtpg) {
    if (!dtpg) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}
