#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int accept(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

int bind(int s, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

int chmod(const char* path, int mode) {
 (void)path;
 (void)mode;
 return 0;
}

int clock_getres(int clock_id, KernelTimespec* res) {
 (void)clock_id;
 (void)res;
 return 0;
}

int clock_gettime(int clock_id, KernelTimespec* time) {
 (void)clock_id;
 (void)time;
 return 0;
}

int close(int d) {
 (void)d;
 return 0;
}

int connect(int s, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

void exit(int code) {
 (void)code;
}

int flock(int d, int operation) {
 (void)d;
 (void)operation;
 return 0;
}

int64_t fstat(int d, FileStat* sb) {
 (void)d;
 (void)sb;
 return 0;
}

int ftruncate(int d, int64_t length) {
 (void)d;
 (void)length;
 return 0;
}

int getargc(void) {
 return 0;
}

const char** getargv(void) {
 return nullptr;
}

int getpagesize(void) {
 return 0;
}

int getpid(void) {
 return 0;
}

int getsockname(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

int getsockopt(int s, int level, int optname, void* optval, uint32_t* optlen) {
 (void)s;
 (void)level;
 (void)optname;
 (void)optval;
 (void)optlen;
 return 0;
}

int gettimeofday(KernelTimeval* time, KernelTimezone* timezone) {
 (void)time;
 (void)timezone;
 return 0;
}

const char* inet_ntop(int af, const void* src, char* dst, uint32_t size) {
 (void)af;
 (void)src;
 (void)dst;
 (void)size;
 return nullptr;
}

int inet_pton(int af, const char* src, void* dst) {
 (void)af;
 (void)src;
 (void)dst;
 return 0;
}

int listen(int s, int backlog) {
 (void)s;
 (void)backlog;
 return 0;
}

int64_t lseek(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;
 return 0;
}

int mkdir(const char* path, uint16_t mode) {
 (void)path;
 (void)mode;
 return 0;
}

int nanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
 (void)rqtp;
 (void)rmtp;
 return 0;
}

int open(const char* path, int flags, int mode) {
 (void)path;
 (void)flags;
 (void)mode;
 return 0;
}

int64_t pread(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 return 0;
}

int pthread_attr_destroy(PthreadAttr* attr) {
 (void)attr;
 return 0;
}

int pthread_attr_get_np(Pthread thread, PthreadAttr* attr) {
 (void)thread;
 (void)attr;
 return 0;
}

int pthread_attr_getdetachstate(const PthreadAttr* attr, int* state) {
 (void)attr;
 (void)state;
 return 0;
}

int pthread_attr_getguardsize(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 return 0;
}

int pthread_attr_getschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
 (void)attr;
 (void)param;
 return 0;
}

int pthread_attr_getschedpolicy(const PthreadAttr* attr, int* policy) {
 (void)attr;
 (void)policy;
 return 0;
}

int pthread_attr_getstack(const PthreadAttr* __restrict attr, void** __restrict stack_addr, size_t* __restrict stack_size) {
 (void)attr;
 (void)stack_addr;
 (void)stack_size;
 return 0;
}

int pthread_attr_getstacksize(const PthreadAttr* attr, size_t* stack_size) {
 (void)attr;
 (void)stack_size;
 return 0;
}

int pthread_attr_init(PthreadAttr* attr) {
 (void)attr;
 return 0;
}

int pthread_attr_setdetachstate(PthreadAttr* attr, int state) {
 (void)attr;
 (void)state;
 return 0;
}

int pthread_attr_setguardsize(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 return 0;
}

int pthread_attr_setinheritsched(PthreadAttr* attr, int inherit_sched) {
 (void)attr;
 (void)inherit_sched;
 return 0;
}

int pthread_attr_setschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
 (void)attr;
 (void)param;
 return 0;
}

int pthread_attr_setschedpolicy(PthreadAttr* attr, int policy) {
 (void)attr;
 (void)policy;
 return 0;
}

int pthread_attr_setstacksize(PthreadAttr* attr, size_t stack_size) {
 (void)attr;
 (void)stack_size;
 return 0;
}

int pthread_cond_broadcast(PthreadCond* cond) {
 (void)cond;
 return 0;
}

int pthread_cond_init(PthreadCond* cond, const PthreadCondattr* attr) {
 (void)cond;
 (void)attr;
 return 0;
}

int pthread_cond_signal(PthreadCond* cond) {
 (void)cond;
 return 0;
}

int pthread_cond_timedwait(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)cond;
 (void)mutex;
 (void)abstime;
 return 0;
}

int pthread_cond_wait(PthreadCond* cond, PthreadMutex* mutex) {
 (void)cond;
 (void)mutex;
 return 0;
}

int pthread_condattr_destroy(PthreadCondattr* attr) {
 (void)attr;
 return 0;
}

int pthread_condattr_init(PthreadCondattr* attr) {
 (void)attr;
 return 0;
}

int pthread_condattr_setclock(PthreadCondattr* attr, KernelClockid clock_id) {
 (void)attr;
 (void)clock_id;
 return 0;
}

int pthread_create(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 return 0;
}

int pthread_create_name_np(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg, const char* name) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 (void)name;
 return 0;
}

int pthread_detach(Pthread thread) {
 (void)thread;
 return 0;
}

void pthread_exit(void* value) {
 (void)value;
}

int pthread_getschedparam(Pthread thread, int* policy, KernelSchedParam* param) {
 (void)thread;
 (void)policy;
 (void)param;
 return 0;
}

void* pthread_getspecific(PthreadKey key) {
 (void)key;
 return nullptr;
}

int pthread_join(Pthread thread, void** value) {
 (void)thread;
 (void)value;
 return 0;
}

int pthread_key_create(PthreadKey* key, pthread_key_destructor_func_t destructor) {
 (void)key;
 (void)destructor;
 return 0;
}

int pthread_key_delete(PthreadKey key) {
 (void)key;
 return 0;
}

int pthread_mutex_destroy(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int pthread_mutex_init(PthreadMutex* mutex, const PthreadMutexattr* attr) {
 (void)mutex;
 (void)attr;
 return 0;
}

int pthread_mutex_lock(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int pthread_mutex_timedlock(PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)mutex;
 (void)abstime;
 return 0;
}

int pthread_mutex_trylock(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int pthread_mutex_unlock(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int pthread_mutexattr_destroy(PthreadMutexattr* attr) {
 (void)attr;
 return 0;
}

int pthread_mutexattr_init(PthreadMutexattr* attr) {
 (void)attr;
 return 0;
}

int pthread_mutexattr_setprotocol(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 return 0;
}

int pthread_mutexattr_settype(PthreadMutexattr* attr, int type) {
 (void)attr;
 (void)type;
 return 0;
}

int pthread_once(void* once_control, void (*init_routine)()) {
 (void)once_control;
 (void)init_routine;
 return 0;
}

int pthread_rename_np(Pthread thread, const char* name) {
 (void)thread;
 (void)name;
 return 0;
}

int pthread_rwlock_destroy(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

int pthread_rwlock_init(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
 (void)rwlock;
 (void)attr;
 return 0;
}

int pthread_rwlock_wrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

Pthread pthread_self(void) {
 return {};
}

int pthread_setcancelstate(int state, int* old_state) {
 (void)state;
 (void)old_state;
 return 0;
}

int pthread_setprio(Pthread thread, int prio) {
 (void)thread;
 (void)prio;
 return 0;
}

int pthread_setschedparam(Pthread thread, int policy, const KernelSchedParam* param) {
 (void)thread;
 (void)policy;
 (void)param;
 return 0;
}

int pthread_setspecific(PthreadKey key, void* value) {
 (void)key;
 (void)value;
 return 0;
}

void pthread_yield(void) {
}

int64_t pwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 return 0;
}

int64_t read(int d, void* buf, uint64_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 return 0;
}

int64_t recv(int s, void* buf, uint64_t len, int flags) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 return 0;
}

int64_t recvfrom(int s, void* buf, uint64_t len, int flags, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 (void)addr;
 (void)addrlen;
 return 0;
}

int sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stack_size, uint64_t context) {
 (void)handler;
 (void)stack_size;
 (void)context;
 return 0;
}

int sceCoredumpUnregisterCoredumpHandler(void) {
 return 0;
}

int sceKernelAddAmprEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 return 0;
}

int sceKernelAddHRTimerEvent(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
 (void)eq;
 (void)id;
 (void)ts;
 (void)udata;
 return 0;
}

int sceKernelAddUserEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 return 0;
}

int sceKernelAddUserEventEdge(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 return 0;
}

int sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
 (void)id;
 (void)ret;
 return 0;
}

int sceKernelAioInitializeImpl(void* param, int32_t size) {
 (void)param;
 (void)size;
 return 0;
}

void sceKernelAioInitializeParam(void* param) {
 (void)param;
}

int sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
 (void)req;
 (void)size;
 (void)prio;
 (void)id;
 return 0;
}

int sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
 (void)req;
 (void)size;
 (void)prio;
 (void)id;
 return 0;
}

int sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
 (void)id;
 (void)state;
 (void)usec;
 return 0;
}

int sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 (void)search_start;
 (void)search_end;
 (void)len;
 (void)alignment;
 (void)memory_type;
 (void)phys_addr_out;
 return 0;
}

int sceKernelAllocateMainDirectMemory(size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 (void)len;
 (void)alignment;
 (void)memory_type;
 (void)phys_addr_out;
 return 0;
}

int sceKernelAvailableDirectMemorySize(int64_t search_start, int64_t search_end, size_t alignment, int64_t* phys_addr_out, size_t* size_out) {
 (void)search_start;
 (void)search_end;
 (void)alignment;
 (void)phys_addr_out;
 (void)size_out;
 return 0;
}

int sceKernelAvailableFlexibleMemorySize(size_t* size) {
 (void)size;
 return 0;
}

int sceKernelBatchMap(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 return 0;
}

int sceKernelBatchMap2(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out, int flags) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 (void)flags;
 return 0;
}

int sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
 (void)ef;
 (void)set_pattern;
 (void)num_wait_threads;
 return 0;
}

int sceKernelCancelSema(KernelSema sem, int count, int* threads) {
 (void)sem;
 (void)count;
 (void)threads;
 return 0;
}

int sceKernelCheckedReleaseDirectMemory(int64_t start, size_t len) {
 (void)start;
 (void)len;
 return 0;
}

int sceKernelCheckReachability(const char* path) {
 (void)path;
 return 0;
}

int sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
 (void)ef;
 (void)bit_pattern;
 return 0;
}

int sceKernelClockGetres(KernelClockid clock_id, KernelTimespec* tp) {
 (void)clock_id;
 (void)tp;
 return 0;
}

int sceKernelClockGettime(KernelClockid clock_id, KernelTimespec* tp) {
 (void)clock_id;
 (void)tp;
 return 0;
}

int sceKernelClose(int d) {
 (void)d;
 return 0;
}

int sceKernelConfiguredFlexibleMemorySize(size_t* size) {
 (void)size;
 return 0;
}

int sceKernelConvertLocaltimeToUtc(int64_t local_time, int64_t reserved, int64_t* utc_time, KernelTimezone* timezone, int32_t* dst_seconds) {
 (void)local_time;
 (void)reserved;
 (void)utc_time;
 (void)timezone;
 (void)dst_seconds;
 return 0;
}

int sceKernelConvertUtcToLocaltime(int64_t utc_time, int64_t* local_time, KernelTimesec* st, uint64_t* dst_sec) {
 (void)utc_time;
 (void)local_time;
 (void)st;
 (void)dst_sec;
 return 0;
}

int sceKernelCreateEqueue(KernelEqueue* eq, const char* name) {
 (void)eq;
 (void)name;
 return 0;
}

int sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, uint32_t attr, uint64_t init_pattern, const void* param) {
 (void)ef;
 (void)name;
 (void)attr;
 (void)init_pattern;
 (void)param;
 return 0;
}

int sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)sem;
 (void)name;
 (void)attr;
 (void)init;
 (void)max;
 (void)opt;
 return 0;
}

void sceKernelDebugRaiseException(int c1, int c2) {
 (void)c1;
 (void)c2;
}

void sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
 (void)c1;
 (void)c2;
}

int sceKernelDeleteAmprEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 return 0;
}

int sceKernelDeleteEqueue(KernelEqueue eq) {
 (void)eq;
 return 0;
}

int sceKernelDeleteEventFlag(KernelEventFlag ef) {
 (void)ef;
 return 0;
}

int sceKernelDeleteHRTimerEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 return 0;
}

int sceKernelDeleteSema(KernelSema sem) {
 (void)sem;
 return 0;
}

int sceKernelDeleteUserEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 return 0;
}

int sceKernelDirectMemoryQuery(int64_t offset, int flags, void* info, size_t info_size) {
 (void)offset;
 (void)flags;
 (void)info;
 (void)info_size;
 return 0;
}

int sceKernelDlsym(KernelModule handle, const char* symbol, void** addr) {
 (void)handle;
 (void)symbol;
 (void)addr;
 return 0;
}

int sceKernelFstat(int d, FileStat* sb) {
 (void)d;
 (void)sb;
 return 0;
}

int sceKernelFsync(int fd) {
 (void)fd;
 return 0;
}

int sceKernelGetCurrentCpu(void) {
 return 0;
}

int sceKernelGetdents(int fd, char* buf, int nbytes) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 return 0;
}

size_t sceKernelGetDirectMemorySize(void) {
 return 0;
}

int sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 (void)basep;
 return 0;
}

intptr_t sceKernelGetEventData(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

int sceKernelGetEventError(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

intptr_t sceKernelGetEventFflags(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

int sceKernelGetEventFilter(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

uintptr_t sceKernelGetEventId(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

void* sceKernelGetEventUserData(const KernelEvent* ev) {
 (void)ev;
 return nullptr;
}

uint64_t sceKernelGetGPI(void) {
 return 0;
}

int sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
 (void)addr;
 (void)flags;
 (void)info;
 return 0;
}

int sceKernelGetModuleInfoFromAddr(uint64_t addr, int n, ModuleInfo* r) {
 (void)addr;
 (void)n;
 (void)r;
 return 0;
}

int sceKernelGetOpenPsId(void* open_ps_id) {
 (void)open_ps_id;
 return 0;
}

int sceKernelGetPageTableStats(int* cpu_total, int* cpu_available, int* gpu_total, int* gpu_available) {
 (void)cpu_total;
 (void)cpu_available;
 (void)gpu_total;
 (void)gpu_available;
 return 0;
}

uint64_t sceKernelGetProcessTime(void) {
 return 0;
}

uint64_t sceKernelGetProcessTimeCounter(void) {
 return 0;
}

uint64_t sceKernelGetProcessTimeCounterFrequency(void) {
 return 0;
}

void* sceKernelGetProcParam(void) {
 return nullptr;
}

int sceKernelGetPrtAperture(int index, void** addr, size_t* len) {
 (void)index;
 (void)addr;
 (void)len;
 return 0;
}

MallocReplace* sceKernelGetSanitizerMallocReplaceExternal(void) {
 return nullptr;
}

NewReplace* sceKernelGetSanitizerNewReplaceExternal(void) {
 return nullptr;
}

int sceKernelGettimeofday(KernelTimeval* tp) {
 (void)tp;
 return 0;
}

int sceKernelGettimezone(KernelTimezone* tz) {
 (void)tz;
 return 0;
}

uint64_t sceKernelGetTscFrequency(void) {
 return 0;
}

int sceKernelInstallExceptionHandler(int signum, void* handler) {
 (void)signum;
 (void)handler;
 return 0;
}

int sceKernelIsAddressSanitizerEnabled(void) {
 return 0;
}

int sceKernelIsStack(void* addr, void** start, void** end) {
 (void)addr;
 (void)start;
 (void)end;
 return 0;
}

KernelModule sceKernelLoadStartModule(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
 (void)module_file_name;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 return {};
}

int64_t sceKernelLseek(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;
 return 0;
}

int sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
 (void)addr;
 (void)len;
 (void)prot;
 (void)flags;
 (void)direct_memory_start;
 (void)alignment;
 return 0;
}

int sceKernelMapDirectMemory2(void** addr, size_t len, int type, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 (void)flags;
 (void)direct_memory_start;
 (void)alignment;
 return 0;
}

int sceKernelMapFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags) {
 (void)addr_in_out;
 (void)len;
 (void)prot;
 (void)flags;
 return 0;
}

int sceKernelMapNamedDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment, const char* name) {
 (void)addr;
 (void)len;
 (void)prot;
 (void)flags;
 (void)direct_memory_start;
 (void)alignment;
 (void)name;
 return 0;
}

int32_t sceKernelMapNamedFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
 (void)addr_in_out;
 (void)len;
 (void)prot;
 (void)flags;
 (void)name;
 return 0;
}

int sceKernelMemoryPoolBatch(const KernelMemoryPoolBatchEntry* entries, int num_entries, int* num_entries_out, int flags) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 (void)flags;
 return 0;
}

int sceKernelMemoryPoolCommit(void* addr, size_t len, int type, int prot, int flags) {
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 (void)flags;
 return 0;
}

int sceKernelMemoryPoolDecommit(void* addr, size_t len, int flags) {
 (void)addr;
 (void)len;
 (void)flags;
 return 0;
}

int sceKernelMemoryPoolExpand(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int64_t* phys_addr_out) {
 (void)search_start;
 (void)search_end;
 (void)len;
 (void)alignment;
 (void)phys_addr_out;
 return 0;
}

int sceKernelMemoryPoolGetBlockStats(KernelMemoryPoolBlockStats* output, size_t output_size) {
 (void)output;
 (void)output_size;
 return 0;
}

int sceKernelMemoryPoolReserve(void* addr_in, size_t len, size_t alignment, int flags, void** addr_out) {
 (void)addr_in;
 (void)len;
 (void)alignment;
 (void)flags;
 (void)addr_out;
 return 0;
}

int sceKernelMkdir(const char* path, uint16_t mode) {
 (void)path;
 (void)mode;
 return 0;
}

int sceKernelMprotect(const void* addr, size_t len, int prot) {
 (void)addr;
 (void)len;
 (void)prot;
 return 0;
}

int sceKernelMtypeprotect(const void* addr, size_t len, int type, int prot) {
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 return 0;
}

int sceKernelMunmap(uint64_t vaddr, size_t len) {
 (void)vaddr;
 (void)len;
 return 0;
}

int sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
 (void)rqtp;
 (void)rmtp;
 return 0;
}

int sceKernelOpen(const char* path, int flags, uint16_t mode) {
 (void)path;
 (void)flags;
 (void)mode;
 return 0;
}

int sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
 (void)ef;
 (void)bit_pattern;
 (void)wait_mode;
 (void)result_pat;
 return 0;
}

int sceKernelPollSema(KernelSema sem, int need) {
 (void)sem;
 (void)need;
 return 0;
}

int64_t sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 return 0;
}

int64_t sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 return 0;
}

int sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int* prot) {
 (void)addr;
 (void)start;
 (void)end;
 (void)prot;
 return 0;
}

int sceKernelRaiseException(Pthread thread, int signum) {
 (void)thread;
 (void)signum;
 return 0;
}

int64_t sceKernelRead(int d, void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 return 0;
}

uint64_t sceKernelReadTsc(void) {
 return 0;
}

int sceKernelReleaseDirectMemory(int64_t start, size_t len) {
 (void)start;
 (void)len;
 return 0;
}

int sceKernelRemoveExceptionHandler(int signum) {
 (void)signum;
 return 0;
}

int sceKernelRename(const char* from, const char* to) {
 (void)from;
 (void)to;
 return 0;
}

int sceKernelReserveVirtualRange(void** addr, size_t len, int flags, size_t alignment) {
 (void)addr;
 (void)len;
 (void)flags;
 (void)alignment;
 return 0;
}

int sceKernelRmdir(const char* path) {
 (void)path;
 return 0;
}

void sceKernelRtldSetApplicationHeapAPI(void* api[]) {
 (void)api;
}

int sceKernelRtldThreadAtexitDecrement(uint64_t* c) {
 (void)c;
 return 0;
}

int sceKernelRtldThreadAtexitIncrement(uint64_t* c) {
 (void)c;
 return 0;
}

int sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
 (void)ef;
 (void)bit_pattern;
 return 0;
}

void sceKernelSetGPO(uint32_t bits) {
 (void)bits;
}

int sceKernelSetPrtAperture(int index, void* addr, size_t len) {
 (void)index;
 (void)addr;
 (void)len;
 return 0;
}

void sceKernelSetThreadAtexitCount(get_thread_atexit_count_func_t func) {
 (void)func;
}

void sceKernelSetThreadAtexitReport(thread_atexit_report_func_t func) {
 (void)func;
}

void sceKernelSetThreadDtors(thread_dtors_func_t dtors) {
 (void)dtors;
}

int sceKernelSetVirtualRangeName(const void* addr, uint64_t len, const char* name) {
 (void)addr;
 (void)len;
 (void)name;
 return 0;
}

int sceKernelSignalSema(KernelSema sem, int count) {
 (void)sem;
 (void)count;
 return 0;
}

unsigned int sceKernelSleep(unsigned int seconds) {
 (void)seconds;
 return 0;
}

int sceKernelStat(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 return 0;
}

int sceKernelStopUnloadModule(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
 (void)handle;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 return 0;
}

void sceKernelSync(void) {
}

int sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 return 0;
}

int sceKernelUnlink(const char* path) {
 (void)path;
 return 0;
}

int sceKernelUsleep(KernelUseconds microseconds) {
 (void)microseconds;
 return 0;
}

int sceKernelUuidCreate(uint32_t* uuid) {
 (void)uuid;
 return 0;
}

int sceKernelVirtualQuery(const void* addr, int flags, VirtualQueryInfo* info, uint64_t info_size) {
 (void)addr;
 (void)flags;
 (void)info;
 (void)info_size;
 return 0;
}

int sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
 (void)eq;
 (void)ev;
 (void)num;
 (void)out;
 (void)timo;
 return 0;
}

int sceKernelWaitEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
 (void)ef;
 (void)bit_pattern;
 (void)wait_mode;
 (void)result_pat;
 (void)timeout;
 return 0;
}

int sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time) {
 (void)sem;
 (void)need;
 (void)time;
 return 0;
}

int64_t sceKernelWrite(int d, const void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 return 0;
}

int scePthreadAttrDestroy(PthreadAttr* attr) {
 (void)attr;
 return 0;
}

int scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
 (void)thread;
 (void)attr;
 return 0;
}

int scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
 (void)attr;
 (void)mask;
 return 0;
}

int scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
 (void)attr;
 (void)state;
 return 0;
}

int scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 return 0;
}

int scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
 (void)attr;
 (void)param;
 return 0;
}

int scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
 (void)attr;
 (void)solosched;
 return 0;
}

int scePthreadAttrGetstack(const PthreadAttr* __restrict attr, void** __restrict stack_addr, size_t* __restrict stack_size) {
 (void)attr;
 (void)stack_addr;
 (void)stack_size;
 return 0;
}

int scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
 (void)attr;
 (void)stack_addr;
 return 0;
}

int scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
 (void)attr;
 (void)stack_size;
 return 0;
}

int scePthreadAttrInit(PthreadAttr* attr) {
 (void)attr;
 return 0;
}

int scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
 (void)attr;
 (void)mask;
 return 0;
}

int scePthreadAttrSetdetachstate(PthreadAttr* attr, int state) {
 (void)attr;
 (void)state;
 return 0;
}

int scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 return 0;
}

int scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
 (void)attr;
 (void)inherit_sched;
 return 0;
}

int scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
 (void)attr;
 (void)param;
 return 0;
}

int scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
 (void)attr;
 (void)policy;
 return 0;
}

int scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
 (void)attr;
 (void)solosched;
 return 0;
}

int scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
 (void)attr;
 (void)addr;
 (void)size;
 return 0;
}

int scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
 (void)attr;
 (void)addr;
 return 0;
}

int scePthreadAttrSetstacksize(PthreadAttr* attr, size_t stack_size) {
 (void)attr;
 (void)stack_size;
 return 0;
}

int scePthreadCancel(Pthread thread) {
 (void)thread;
 return 0;
}

int scePthreadCondattrDestroy(PthreadCondattr* attr) {
 (void)attr;
 return 0;
}

int scePthreadCondattrInit(PthreadCondattr* attr) {
 (void)attr;
 return 0;
}

int scePthreadCondBroadcast(PthreadCond* cond) {
 (void)cond;
 return 0;
}

int scePthreadCondDestroy(PthreadCond* cond) {
 (void)cond;
 return 0;
}

int scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name) {
 (void)cond;
 (void)attr;
 (void)name;
 return 0;
}

int scePthreadCondSignal(PthreadCond* cond) {
 (void)cond;
 return 0;
}

int scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
 (void)cond;
 (void)thread;
 return 0;
}

int scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
 (void)cond;
 (void)mutex;
 (void)usec;
 return 0;
}

int scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
 (void)cond;
 (void)mutex;
 return 0;
}

int scePthreadCreate(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg, const char* name) {
 (void)thread;
 (void)attr;
 (void)entry;
 (void)arg;
 (void)name;
 return 0;
}

int scePthreadDetach(Pthread thread) {
 (void)thread;
 return 0;
}

int scePthreadEqual(Pthread thread1, Pthread thread2) {
 (void)thread1;
 (void)thread2;
 return 0;
}

void scePthreadExit(void* value) {
 (void)value;
}

int scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
 (void)thread;
 (void)mask;
 return 0;
}

int scePthreadGetname(Pthread thread, char* name) {
 (void)thread;
 (void)name;
 return 0;
}

int scePthreadGetprio(Pthread thread, int* prio) {
 (void)thread;
 (void)prio;
 return 0;
}

void* scePthreadGetspecific(PthreadKey key) {
 (void)key;
 return nullptr;
}

int scePthreadGetthreadid(void) {
 return 0;
}

int scePthreadJoin(Pthread thread, void** value) {
 (void)thread;
 (void)value;
 return 0;
}

int scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
 (void)key;
 (void)destructor;
 return 0;
}

int scePthreadKeyDelete(PthreadKey key) {
 (void)key;
 return 0;
}

int scePthreadMutexattrDestroy(PthreadMutexattr* attr) {
 (void)attr;
 return 0;
}

int scePthreadMutexattrInit(PthreadMutexattr* attr) {
 (void)attr;
 return 0;
}

int scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 return 0;
}

int scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
 (void)attr;
 (void)type;
 return 0;
}

int scePthreadMutexDestroy(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name) {
 (void)mutex;
 (void)attr;
 (void)name;
 return 0;
}

int scePthreadMutexLock(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
 (void)mutex;
 (void)usec;
 return 0;
}

int scePthreadMutexTrylock(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int scePthreadMutexUnlock(PthreadMutex* mutex) {
 (void)mutex;
 return 0;
}

int scePthreadRename(Pthread thread, const char* name) {
 (void)thread;
 (void)name;
 return 0;
}

int scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
 (void)attr;
 return 0;
}

int scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
 (void)attr;
 return 0;
}

int scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
 (void)attr;
 (void)type;
 return 0;
}

int scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

int scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
 (void)rwlock;
 (void)attr;
 (void)name;
 return 0;
}

int scePthreadRwlockRdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

int scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

int scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

int scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

int scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 return 0;
}

Pthread scePthreadSelf(void) {
 return {};
}

int scePthreadSemDestroy(void* sem) {
 (void)sem;
 return 0;
}

int scePthreadSemGetvalue(void* sem, int* value) {
 (void)sem;
 (void)value;
 return 0;
}

int scePthreadSemInit(void* sem, int flag, unsigned int value, const char* name) {
 (void)sem;
 (void)flag;
 (void)value;
 (void)name;
 return 0;
}

int scePthreadSemPost(void* sem) {
 (void)sem;
 return 0;
}

int scePthreadSemTimedwait(void* sem, KernelUseconds usec) {
 (void)sem;
 (void)usec;
 return 0;
}

int scePthreadSemTrywait(void* sem) {
 (void)sem;
 return 0;
}

int scePthreadSemWait(void* sem) {
 (void)sem;
 return 0;
}

int scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
 (void)thread;
 (void)mask;
 return 0;
}

int scePthreadSetcancelstate(int state, int* old_state) {
 (void)state;
 (void)old_state;
 return 0;
}

int scePthreadSetcanceltype(int type, int* old_type) {
 (void)type;
 (void)old_type;
 return 0;
}

int scePthreadSetprio(Pthread thread, int prio) {
 (void)thread;
 (void)prio;
 return 0;
}

int scePthreadSetspecific(PthreadKey key, void* value) {
 (void)key;
 (void)value;
 return 0;
}

void scePthreadYield(void) {
}

int sched_get_priority_max(int policy) {
 (void)policy;
 return 0;
}

int sched_get_priority_min(int policy) {
 (void)policy;
 return 0;
}

int select(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
 (void)nfds;
 (void)readfds;
 (void)writefds;
 (void)exceptfds;
 (void)timeout;
 return 0;
}

int sem_destroy(void* sem) {
 (void)sem;
 return 0;
}

int sem_getvalue(void* sem, int* value) {
 (void)sem;
 (void)value;
 return 0;
}

int sem_init(void* sem, int pshared, unsigned int value) {
 (void)sem;
 (void)pshared;
 (void)value;
 return 0;
}

int sem_post(void* sem) {
 (void)sem;
 return 0;
}

int sem_reltimedwait_np(void* sem, uint32_t usec) {
 (void)sem;
 (void)usec;
 return 0;
}

int sem_timedwait(void* sem, const KernelTimespec* abstime) {
 (void)sem;
 (void)abstime;
 return 0;
}

int sem_trywait(void* sem) {
 (void)sem;
 return 0;
}

int sem_wait(void* sem) {
 (void)sem;
 return 0;
}

int64_t send(int s, const void* buf, uint64_t len, int flags) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 return 0;
}

int64_t sendto(int s, const void* buf, uint64_t len, int flags, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 (void)addr;
 (void)addrlen;
 return 0;
}

int setsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
 (void)s;
 (void)level;
 (void)optname;
 (void)optval;
 (void)optlen;
 return 0;
}

int sigprocmask(int how, const void* set, void* oset) {
 (void)how;
 (void)set;
 (void)oset;
 return 0;
}

int socket(int family, int type, int protocol) {
 (void)family;
 (void)type;
 (void)protocol;
 return 0;
}

int stat(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 return 0;
}

int64_t write(int d, const char* str, int64_t size) {
 (void)d;
 (void)str;
 (void)size;
 return 0;
}

}
