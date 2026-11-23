#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

int APS5_VABI timer_create_nid_postfix(int clockId, void* event, void* timer) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI timer_delete_nid_postfix(void* timer) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI timer_settime_nid_postfix(void* timer, int flags, const void* value, void* previous) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI timer_gettime_nid_postfix(void* timer, void* value) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI timer_getoverrun_nid_postfix(void* timer) {
    NotImplemented_nid_no_patch(__func__);
}

}
