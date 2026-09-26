#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "prx/libc/include/General.hpp"


extern "C" {

int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_create_name_np_nid_postfix(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg, const char* name) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_detach_nid_postfix(Pthread thread) {
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI pthread_exit_nid_postfix(void* value) {
    scePthreadExit(value);
}

int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
 (void)thread;
 (void)policy;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** value) {
 (void)thread;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name) {
 (void)thread;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Pthread APS5_VABI pthread_self_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int APS5_VABI pthread_setcancelstate_nid_postfix(int state, int* old_state) {
 (void)state;
 (void)old_state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_setprio_nid_postfix(Pthread thread, int prio) {
 (void)thread;
 (void)prio;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
 (void)thread;
 (void)policy;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI pthread_yield_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
}

}
