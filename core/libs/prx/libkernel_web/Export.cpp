#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
 (void)attr;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
 (void)attr;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
 (void)attr;
 (void)stack_addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
 (void)attr;
 (void)stack_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
 (void)attr;
 (void)inherit_sched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
 (void)attr;
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
 (void)attr;
 (void)addr;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
 (void)attr;
 (void)addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadCancel(Pthread thread) {
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
 (void)cond;
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
 (void)cond;
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadEqual(Pthread thread1, Pthread thread2) {
 (void)thread1;
 (void)thread2;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
 (void)thread;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadGetname(Pthread thread, char* name) {
 (void)thread;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadGetprio(Pthread thread, int* prio) {
 (void)thread;
 (void)prio;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* scePthreadGetspecific(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int scePthreadGetthreadid(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
 (void)key;
 (void)destructor;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadKeyDelete(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
 (void)mutex;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadMutexTrylock(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRename(Pthread thread, const char* name) {
 (void)thread;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
 (void)attr;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
 (void)rwlock;
 (void)attr;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockRdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemDestroy(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemGetvalue(void* sem, int* value) {
 (void)sem;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemInit(void* sem, int flag, unsigned int value, const char* name) {
 (void)sem;
 (void)flag;
 (void)value;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemPost(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemTimedwait(void* sem, KernelUseconds usec) {
 (void)sem;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemTrywait(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSemWait(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
 (void)thread;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSetcancelstate(int state, int* old_state) {
 (void)state;
 (void)old_state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSetcanceltype(int type, int* old_type) {
 (void)type;
 (void)old_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSetprio(Pthread thread, int prio) {
 (void)thread;
 (void)prio;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePthreadSetspecific(PthreadKey key, void* value) {
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
