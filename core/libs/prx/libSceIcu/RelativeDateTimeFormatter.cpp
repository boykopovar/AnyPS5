#include "RelativeDateTimeFormatter.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt) {
    if (!reldatefmt) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
