#include "DateIntervalFormat.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter) {
    if (!formatter) return;
    NotImplemented_nid_no_patch(__func__);
}

}
