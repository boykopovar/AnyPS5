#include "prx/libkernel_sys/include/Api.hpp"

extern "C" {


int APS5_VABI wait4_nid_postfix(int pid, int* status, int options, void*) {
    return waitpid_nid_postfix(pid, status, options);
}

int APS5_VABI wait3_nid_postfix(int* status, int options, void* usage) {
    return wait4_nid_postfix(-1, status, options, usage);
}

}
