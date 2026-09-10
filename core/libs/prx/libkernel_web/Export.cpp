#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
 (void)attr;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
 (void)attr;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
 (void)attr;
 (void)stack_addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
 (void)attr;
 (void)stack_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
 (void)attr;
 (void)inherit_sched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
 (void)attr;
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
 (void)attr;
 (void)addr;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
 (void)attr;
 (void)addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadCancel(Pthread thread) {
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
 (void)cond;
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
 (void)cond;
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2) {
 (void)thread1;
 (void)thread2;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
 (void)thread;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadGetname(Pthread thread, char* name) {
 (void)thread;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadGetprio(Pthread thread, int* prio) {
 (void)thread;
 (void)prio;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI scePthreadGetthreadid(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
 (void)key;
 (void)destructor;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadKeyDelete(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
 (void)mutex;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRename(Pthread thread, const char* name) {
 (void)thread;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
 (void)attr;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
 (void)rwlock;
 (void)attr;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemDestroy(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemGetvalue(void* sem, int* value) {
 (void)sem;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemInit(void* sem, int flag, unsigned int value, const char* name) {
 (void)sem;
 (void)flag;
 (void)value;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemPost(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemTimedwait(void* sem, KernelUseconds usec) {
 (void)sem;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemTrywait(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSemWait(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
 (void)thread;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
 (void)state;
 (void)old_state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
 (void)type;
 (void)old_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSetprio(Pthread thread, int prio) {
 (void)thread;
 (void)prio;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
