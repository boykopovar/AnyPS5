#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

extern "C" int APS5_VABI scePthreadRename(Pthread thread, const char* name);
extern "C" Pthread APS5_VABI scePthreadSelf();
extern "C" int APS5_VABI scePthreadSetcancelstate(int state, int* oldState);
extern "C" int APS5_VABI scePthreadSetcanceltype(int type, int* oldType);


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
 (void)value;
 NotImplemented_nid_no_patch(__func__);
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
    return scePthreadRename(thread, name) == 0 ? 0 : EINVAL;
}

void APS5_VABI pthread_set_name_np_nid_postfix(Pthread thread, const char* name) {
    if (scePthreadRename(thread, name) != 0) errno = EINVAL;
}

Pthread APS5_VABI pthread_self_nid_postfix(void) {
    return scePthreadSelf();
}

int APS5_VABI pthread_setcancelstate_nid_postfix(int state, int* old_state) {
    return scePthreadSetcancelstate(state, old_state) == 0 ? 0 : EINVAL;
}

int APS5_VABI pthread_setcanceltype_nid_postfix(int type, int* old_type) {
    return scePthreadSetcanceltype(type, old_type) == 0 ? 0 : EINVAL;
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
