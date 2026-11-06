#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/System/include/ProcessWait.hpp"

extern "C" {

int APS5_VABI wait4_nid_postfix(int pid, int* status, int options, void* usage) {
    return WaitProcess_nid_no_patch(pid, status, options, static_cast<GuestResourceUsage*>(usage));
}

int APS5_VABI wait3_nid_postfix(int* status, int options, void* usage) {
    return wait4_nid_postfix(-1, status, options, usage);
}

}
