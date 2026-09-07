#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "DirectMemory/DirectMemory.hpp"

extern "C" {

int accept_nid_postfix(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int bind_nid_postfix(int s, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int chmod_nid_postfix(const char* path, int mode) {
 (void)path;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int clock_getres_nid_postfix(int clock_id, KernelTimespec* res) {
 (void)clock_id;
 (void)res;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int clock_gettime_nid_postfix(int clock_id, KernelTimespec* time) {
 (void)clock_id;
 (void)time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int close_nid_postfix(int d) {
 (void)d;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int connect_nid_postfix(int s, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void exit_nid_postfix(int code) {
 (void)code;
 NotImplemented_nid_no_patch(__func__);
}

int flock_nid_postfix(int d, int operation) {
 (void)d;
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
 (void)d;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int ftruncate_nid_postfix(int d, int64_t length) {
 (void)d;
 (void)length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int getargc_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char** getargv_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int getpagesize_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int getpid_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int getsockname_nid_postfix(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int getsockopt_nid_postfix(int s, int level, int optname, void* optval, uint32_t* optlen) {
 (void)s;
 (void)level;
 (void)optname;
 (void)optval;
 (void)optlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int gettimeofday_nid_postfix(KernelTimeval* time, KernelTimezone* timezone) {
 (void)time;
 (void)timezone;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char* inet_ntop_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
 (void)af;
 (void)src;
 (void)dst;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int inet_pton_nid_postfix(int af, const char* src, void* dst) {
 (void)af;
 (void)src;
 (void)dst;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int listen_nid_postfix(int s, int backlog) {
 (void)s;
 (void)backlog;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t lseek_nid_postfix(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int mkdir_nid_postfix(const char* path, uint16_t mode) {
 (void)path;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
 (void)rqtp;
 (void)rmtp;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int open_nid_postfix(const char* path, int flags, int mode) {
 (void)path;
 (void)flags;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t pread_nid_disambig1_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_destroy_nid_postfix(PthreadAttr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr) {
 (void)thread;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_getdetachstate_nid_postfix(const PthreadAttr* attr, int* state) {
 (void)attr;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_getschedparam_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
 (void)attr;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_getschedpolicy_nid_postfix(const PthreadAttr* attr, int* policy) {
 (void)attr;
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_getstack_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr, size_t* __restrict stack_size) {
 (void)attr;
 (void)stack_addr;
 (void)stack_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_getstacksize_nid_postfix(const PthreadAttr* attr, size_t* stack_size) {
 (void)attr;
 (void)stack_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_init_nid_postfix(PthreadAttr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_setdetachstate_nid_postfix(PthreadAttr* attr, int state) {
 (void)attr;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_setguardsize_nid_postfix(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_setinheritsched_nid_postfix(PthreadAttr* attr, int inherit_sched) {
 (void)attr;
 (void)inherit_sched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_setschedparam_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
 (void)attr;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_setschedpolicy_nid_postfix(PthreadAttr* attr, int policy) {
 (void)attr;
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, size_t stack_size) {
 (void)attr;
 (void)stack_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
 (void)cond;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
 (void)cond;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_cond_signal_nid_postfix(PthreadCond* cond) {
 (void)cond;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)cond;
 (void)mutex;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
 (void)cond;
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
 (void)attr;
 (void)clock_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_create_name_np_nid_postfix(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg, const char* name) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_detach_nid_postfix(Pthread thread) {
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void pthread_exit_nid_postfix(void* value) {
 (void)value;
 NotImplemented_nid_no_patch(__func__);
}

int pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
 (void)thread;
 (void)policy;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* pthread_getspecific_nid_postfix(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int pthread_join_nid_postfix(Pthread thread, void** value) {
 (void)thread;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_key_create_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
 (void)key;
 (void)destructor;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_key_delete_nid_postfix(PthreadKey key) {
 (void)key;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
 (void)mutex;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)mutex;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutex_trylock_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex) {
 (void)mutex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) {
 (void)attr;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_once_nid_postfix(void* once_control, void (*init_routine)()) {
 (void)once_control;
 (void)init_routine;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_rename_np_nid_postfix(Pthread thread, const char* name) {
 (void)thread;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
 (void)rwlock;
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Pthread pthread_self_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int pthread_setcancelstate_nid_postfix(int state, int* old_state) {
 (void)state;
 (void)old_state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_setprio_nid_postfix(Pthread thread, int prio) {
 (void)thread;
 (void)prio;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
 (void)thread;
 (void)policy;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int pthread_setspecific_nid_postfix(PthreadKey key, void* value) {
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void pthread_yield_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
}

int64_t pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t read_nid_postfix(int d, void* buf, uint64_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t recv_nid_postfix(int s, void* buf, uint64_t len, int flags) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t recvfrom_nid_postfix(int s, void* buf, uint64_t len, int flags, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stack_size, uint64_t context) {
 (void)handler;
 (void)stack_size;
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceCoredumpUnregisterCoredumpHandler(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAddAmprEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAddHRTimerEvent(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
 (void)eq;
 (void)id;
 (void)ts;
 (void)udata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAddUserEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAddUserEventEdge(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
 (void)id;
 (void)ret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAioInitializeImpl(void* param, int32_t size) {
 (void)param;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceKernelAioInitializeParam(void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
}

int sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
 (void)req;
 (void)size;
 (void)prio;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
 (void)req;
 (void)size;
 (void)prio;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
 (void)id;
 (void)state;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}




int sceKernelAvailableFlexibleMemorySize(size_t* size) {
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelBatchMap(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelBatchMap2(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out, int flags) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
 (void)ef;
 (void)set_pattern;
 (void)num_wait_threads;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCancelSema(KernelSema sem, int count, int* threads) {
 (void)sem;
 (void)count;
 (void)threads;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCheckedReleaseDirectMemory(int64_t start, size_t len) {
 (void)start;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCheckReachability(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
 (void)ef;
 (void)bit_pattern;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelClockGetres(KernelClockid clock_id, KernelTimespec* tp) {
 (void)clock_id;
 (void)tp;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelClockGettime(KernelClockid clock_id, KernelTimespec* tp) {
 (void)clock_id;
 (void)tp;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelClose(int d) {
 (void)d;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelConfiguredFlexibleMemorySize(size_t* size) {
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelConvertLocaltimeToUtc(int64_t local_time, int64_t reserved, int64_t* utc_time, KernelTimezone* timezone, int32_t* dst_seconds) {
 (void)local_time;
 (void)reserved;
 (void)utc_time;
 (void)timezone;
 (void)dst_seconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelConvertUtcToLocaltime(int64_t utc_time, int64_t* local_time, KernelTimesec* st, uint64_t* dst_sec) {
 (void)utc_time;
 (void)local_time;
 (void)st;
 (void)dst_sec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCreateEqueue(KernelEqueue* eq, const char* name) {
 (void)eq;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, uint32_t attr, uint64_t init_pattern, const void* param) {
 (void)ef;
 (void)name;
 (void)attr;
 (void)init_pattern;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)sem;
 (void)name;
 (void)attr;
 (void)init;
 (void)max;
 (void)opt;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceKernelDebugRaiseException(int c1, int c2) {
 (void)c1;
 (void)c2;
 NotImplemented_nid_no_patch(__func__);
}

void sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
 (void)c1;
 (void)c2;
 NotImplemented_nid_no_patch(__func__);
}

int sceKernelDeleteAmprEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelDeleteEqueue(KernelEqueue eq) {
 (void)eq;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelDeleteEventFlag(KernelEventFlag ef) {
 (void)ef;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelDeleteHRTimerEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelDeleteSema(KernelSema sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelDeleteUserEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelDlsym(KernelModule handle, const char* symbol, void** addr) {
 (void)handle;
 (void)symbol;
 (void)addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelFstat(int d, FileStat* sb) {
 (void)d;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelFsync(int fd) {
 (void)fd;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetCurrentCpu(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetdents(int fd, char* buf, int nbytes) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 (void)basep;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

intptr_t sceKernelGetEventData(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetEventError(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

intptr_t sceKernelGetEventFflags(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetEventFilter(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uintptr_t sceKernelGetEventId(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* sceKernelGetEventUserData(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint64_t sceKernelGetGPI(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
 (void)addr;
 (void)flags;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetModuleInfoFromAddr(uint64_t addr, int n, ModuleInfo* r) {
 (void)addr;
 (void)n;
 (void)r;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetOpenPsId(void* open_ps_id) {
 (void)open_ps_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGetPageTableStats(int* cpu_total, int* cpu_available, int* gpu_total, int* gpu_available) {
 (void)cpu_total;
 (void)cpu_available;
 (void)gpu_total;
 (void)gpu_available;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t sceKernelGetProcessTime(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t sceKernelGetProcessTimeCounter(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t sceKernelGetProcessTimeCounterFrequency(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* sceKernelGetProcParam(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int sceKernelGetPrtAperture(int index, void** addr, size_t* len) {
 (void)index;
 (void)addr;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

MallocReplace* sceKernelGetSanitizerMallocReplaceExternal(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

NewReplace* sceKernelGetSanitizerNewReplaceExternal(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int sceKernelGettimeofday(KernelTimeval* tp) {
 (void)tp;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelGettimezone(KernelTimezone* tz) {
 (void)tz;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t sceKernelGetTscFrequency(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelInstallExceptionHandler(int signum, void* handler) {
 (void)signum;
 (void)handler;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelIsAddressSanitizerEnabled(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelIsStack(void* addr, void** start, void** end) {
 (void)addr;
 (void)start;
 (void)end;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

KernelModule sceKernelLoadStartModule(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
 (void)module_file_name;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int64_t sceKernelLseek(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}






int sceKernelMemoryPoolBatch(const KernelMemoryPoolBatchEntry* entries, int num_entries, int* num_entries_out, int flags) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelMemoryPoolCommit(void* addr, size_t len, int type, int prot, int flags) {
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelMemoryPoolDecommit(void* addr, size_t len, int flags) {
 (void)addr;
 (void)len;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelMemoryPoolExpand(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int64_t* phys_addr_out) {
 (void)search_start;
 (void)search_end;
 (void)len;
 (void)alignment;
 (void)phys_addr_out;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelMemoryPoolGetBlockStats(KernelMemoryPoolBlockStats* output, size_t output_size) {
 (void)output;
 (void)output_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelMemoryPoolReserve(void* addr_in, size_t len, size_t alignment, int flags, void** addr_out) {
 (void)addr_in;
 (void)len;
 (void)alignment;
 (void)flags;
 (void)addr_out;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelMkdir(const char* path, uint16_t mode) {
 (void)path;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelMtypeprotect(const void* addr, size_t len, int type, int prot) {
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
 (void)rqtp;
 (void)rmtp;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelOpen(const char* path, int flags, uint16_t mode) {
 (void)path;
 (void)flags;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
 (void)ef;
 (void)bit_pattern;
 (void)wait_mode;
 (void)result_pat;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelPollSema(KernelSema sem, int need) {
 (void)sem;
 (void)need;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int* prot) {
 (void)addr;
 (void)start;
 (void)end;
 (void)prot;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelRaiseException(Pthread thread, int signum) {
 (void)thread;
 (void)signum;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t sceKernelRead(int d, void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t sceKernelReadTsc(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelRemoveExceptionHandler(int signum) {
 (void)signum;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelRename(const char* from, const char* to) {
 (void)from;
 (void)to;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelRmdir(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceKernelRtldSetApplicationHeapAPI(void* api[]) {
 (void)api;
 NotImplemented_nid_no_patch(__func__);
}

int sceKernelRtldThreadAtexitDecrement(uint64_t* c) {
 (void)c;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelRtldThreadAtexitIncrement(uint64_t* c) {
 (void)c;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
 (void)ef;
 (void)bit_pattern;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceKernelSetGPO(uint32_t bits) {
 (void)bits;
 NotImplemented_nid_no_patch(__func__);
}

int sceKernelSetPrtAperture(int index, void* addr, size_t len) {
 (void)index;
 (void)addr;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceKernelSetThreadAtexitCount(get_thread_atexit_count_func_t func) {
 (void)func;
 NotImplemented_nid_no_patch(__func__);
}

void sceKernelSetThreadAtexitReport(thread_atexit_report_func_t func) {
 (void)func;
 NotImplemented_nid_no_patch(__func__);
}

void sceKernelSetThreadDtors(thread_dtors_func_t dtors) {
 (void)dtors;
 NotImplemented_nid_no_patch(__func__);
}

int sceKernelSetVirtualRangeName(const void* addr, uint64_t len, const char* name) {
 (void)addr;
 (void)len;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelSignalSema(KernelSema sem, int count) {
 (void)sem;
 (void)count;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

unsigned int sceKernelSleep(unsigned int seconds) {
 (void)seconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelStat(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelStopUnloadModule(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
 (void)handle;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceKernelSync(void) {
 NotImplemented_nid_no_patch(__func__);
}

int sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelUnlink(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelUsleep(KernelUseconds microseconds) {
 (void)microseconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelUuidCreate(uint32_t* uuid) {
 (void)uuid;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
 (void)eq;
 (void)ev;
 (void)num;
 (void)out;
 (void)timo;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelWaitEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
 (void)ef;
 (void)bit_pattern;
 (void)wait_mode;
 (void)result_pat;
 (void)timeout;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time) {
 (void)sem;
 (void)need;
 (void)time;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t sceKernelWrite(int d, const void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}















































































int sched_get_priority_max_nid_postfix(int policy) {
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sched_get_priority_min_nid_postfix(int policy) {
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
 (void)nfds;
 (void)readfds;
 (void)writefds;
 (void)exceptfds;
 (void)timeout;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_destroy_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_getvalue_nid_postfix(void* sem, int* value) {
 (void)sem;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_init_nid_postfix(void* sem, int pshared, unsigned int value) {
 (void)sem;
 (void)pshared;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_post_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_reltimedwait_np_nid_postfix(void* sem, uint32_t usec) {
 (void)sem;
 (void)usec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_timedwait_nid_postfix(void* sem, const KernelTimespec* abstime) {
 (void)sem;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_trywait_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sem_wait_nid_postfix(void* sem) {
 (void)sem;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t send_nid_postfix(int s, const void* buf, uint64_t len, int flags) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t sendto_nid_postfix(int s, const void* buf, uint64_t len, int flags, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int setsockopt_nid_postfix(int s, int level, int optname, const void* optval, uint32_t optlen) {
 (void)s;
 (void)level;
 (void)optname;
 (void)optval;
 (void)optlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sigprocmask_nid_postfix(int how, const void* set, void* oset) {
 (void)how;
 (void)set;
 (void)oset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int socket_nid_postfix(int family, int type, int protocol) {
 (void)family;
 (void)type;
 (void)protocol;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int stat_nid_postfix(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t write_nid_postfix(int d, const char* str, int64_t size) {
 (void)d;
 (void)str;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

#include "DirectMemory/Export.cpp"

}
