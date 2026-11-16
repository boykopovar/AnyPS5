#include "prx/libc/include/Api.hpp"

#include "prx/libc/include/GuestErrno.hpp"

#include "prx/libkernel/File/include/File.hpp"

#include "prx/libkernel/Pthread/include/Atfork.hpp"

#include "prx/libkernel/Pthread/include/Cancel.hpp"

#include "prx/libkernel/Pthread/include/Cond.hpp"

#include "prx/libkernel/Pthread/include/Mutex.hpp"

#include "prx/libkernel/Pthread/include/Rwlock.hpp"

#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"

#include "prx/libkernel/Semaphore/include/Semaphore.hpp"

#include "prx/libkernel/Time/include/Time.hpp"

#include "prx/libkernel/include/Api.hpp"

#include "prx/libkernel/include/LibKernelSysProviders.hpp"



extern "C" {

int APS5_VABI __Ux86_64_setcontext_nid_no_patch(void) {
    return __Ux86_64_setcontext();
}

int APS5_VABI __elf_phdr_match_addr_nid_no_patch(dl_phdr_info* phdrInfo, void* addr) {
    return __elf_phdr_match_addr_nid_postfix(phdrInfo, addr);
}

int* APS5_VABI __error_nid_no_patch(void) {
    return GuestError_nid_no_patch();
}

const char* APS5_VABI __inet_ntop_nid_no_patch(int family, const void* source, char* destination, std::uint32_t capacity) {
    return __inet_ntop_nid_postfix(family, source, destination, capacity);
}

int APS5_VABI __inet_pton_nid_no_patch(int family, const char* text, void* destination) {
    return __inet_pton_nid_postfix(family, text, destination);
}

void APS5_VABI __pthread_cleanup_pop_imp_nid_no_patch(int execute) {
    __pthread_cleanup_pop_imp_nid_postfix(execute);
}

void APS5_VABI __pthread_cleanup_push_imp_nid_no_patch(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    __pthread_cleanup_push_imp_nid_postfix(routine, argument, info);
}

void APS5_VABI __pthread_cxa_finalize_nid_no_patch(void* argument) {
    __pthread_cxa_finalize_nid_postfix(argument);
}

[[noreturn]] void APS5_VABI __stack_chk_fail_nid_no_patch(void) {
    GuestStackChkFail_nid_no_patch();
}

int APS5_VABI __sys_socketex_nid_no_patch(const char* name, int family, int type, int protocol) {
    return __sys_socketex_nid_postfix(name, family, type, protocol);
}

void* APS5_VABI __tls_get_addr_nid_no_patch(const KernelTlsIndex* index) {
    return __tls_get_addr_nid_postfix(index);
}

int APS5_VABI _accept_nid_no_patch(int descriptor, void* address, std::uint32_t* length) {
    return _accept_nid_postfix(descriptor, address, length);
}

int APS5_VABI _bind_nid_no_patch(int descriptor, const void* address, std::uint32_t length) {
    return _bind_nid_postfix(descriptor, address, length);
}

int APS5_VABI _close_nid_no_patch(int descriptor) {
    return _close_nid_postfix(descriptor);
}

int APS5_VABI _connect_nid_no_patch(int descriptor, const void* address, std::uint32_t length) {
    return _connect_nid_postfix(descriptor, address, length);
}

void APS5_VABI _exit_nid_no_patch(int status) {
    _exit_nid_postfix(status);
}

int APS5_VABI _fcntl_nid_no_patch(int descriptor, int command, std::uint64_t v0, std::uint64_t v1) {
    return _fcntl_nid_postfix(descriptor, command, v0, v1);
}

int64_t APS5_VABI _fstat_nid_no_patch(int d, FileStat* sb) {
    return _fstat_nid_postfix(d, sb);
}

int APS5_VABI _fstatfs_nid_no_patch(int d, KernelStatfs* buf) {
    return _fstatfs_nid_postfix(d, buf);
}

int APS5_VABI _getdirentries_nid_no_patch(int fd, char* buf, int nbytes, int64_t* basep) {
    return _getdirentries_nid_postfix(fd, buf, nbytes, basep);
}

int APS5_VABI _getpeername_nid_no_patch(int descriptor, void* address, std::uint32_t* length) {
    return _getpeername_nid_postfix(descriptor, address, length);
}

int APS5_VABI _getsockname_nid_no_patch(int descriptor, void* address, std::uint32_t* length) {
    return _getsockname_nid_postfix(descriptor, address, length);
}

int APS5_VABI _getsockopt_nid_no_patch(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return _getsockopt_nid_postfix(descriptor, level, option, value, length);
}

int APS5_VABI _ioctl_nid_no_patch(int descriptor, std::uint64_t request, void* argument) {
    return _ioctl_nid_postfix(descriptor, request, argument);
}

int APS5_VABI _is_signal_return_nid_no_patch(std::uint64_t programCounter) {
    return _is_signal_return_nid_postfix(programCounter);
}

int APS5_VABI _listen_nid_no_patch(int descriptor, int backlog) {
    return _listen_nid_postfix(descriptor, backlog);
}

int APS5_VABI _nanosleep_nid_no_patch(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return _nanosleep_nid_postfix(rqtp, rmtp);
}

int APS5_VABI _open_nid_no_patch(const char* path, int flags, std::uint64_t v0, std::uint64_t v1) {
    return _open_nid_postfix(path, flags, v0, v1);
}

std::int64_t APS5_VABI _read_nid_no_patch(int descriptor, void* buffer, std::size_t count) {
    return _read_nid_postfix(descriptor, buffer, count);
}

int64_t APS5_VABI _readv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return _readv_nid_postfix(d, iov, iovcnt);
}

std::int64_t APS5_VABI _recvfrom_nid_no_patch(int descriptor, void* buffer, std::uint64_t length, int flags, void* address, std::uint32_t* addressLength) {
    return _recvfrom_nid_postfix(descriptor, buffer, length, flags, address, addressLength);
}

std::int64_t APS5_VABI _recvmsg_nid_no_patch(int descriptor, GuestMsghdr* message, int flags) {
    return _recvmsg_nid_postfix(descriptor, message, flags);
}

void APS5_VABI _sceKernelRtldSetApplicationHeapAPI_nid_no_patch(void* api[]) {
    _sceKernelRtldSetApplicationHeapAPI_nid_postfix(api);
}

int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_no_patch(const void* dsoSymbol) {
    return _sceKernelRtldThreadAtexitDecrement_nid_postfix(dsoSymbol);
}

int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_no_patch(const void* dsoSymbol) {
    return _sceKernelRtldThreadAtexitIncrement_nid_postfix(dsoSymbol);
}

void APS5_VABI _sceKernelSetThreadDtors_nid_no_patch(thread_dtors_func_t callback) {
    _sceKernelSetThreadDtors_nid_postfix(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitCount_nid_no_patch(get_thread_atexit_count_func_t callback) {
    _sceKernelSetThreadAtexitCount_nid_postfix(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitReport_nid_no_patch(thread_atexit_report_func_t callback) {
    _sceKernelSetThreadAtexitReport_nid_postfix(callback);
}

std::int64_t APS5_VABI _sendmsg_nid_no_patch(int descriptor, const GuestMsghdr* message, int flags) {
    return _sendmsg_nid_postfix(descriptor, message, flags);
}

std::int64_t APS5_VABI _sendto_nid_no_patch(int descriptor, const void* buffer, std::uint64_t length, int flags, const void* address, std::uint32_t addressLength) {
    return _sendto_nid_postfix(descriptor, buffer, length, flags, address, addressLength);
}

int APS5_VABI _setsockopt_nid_no_patch(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return _setsockopt_nid_postfix(descriptor, level, option, value, length);
}

int APS5_VABI _sigaction_nid_no_patch(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return _sigaction_nid_postfix(guest, action, previous);
}

int APS5_VABI _sigprocmask_nid_no_patch(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return _sigprocmask_nid_postfix(how, set, previousSet);
}

int APS5_VABI _sigsuspend_nid_no_patch(const GuestSignalSet* set) {
    return _sigsuspend_nid_postfix(set);
}

int APS5_VABI _umtx_op_nid_no_patch(void* object, int operation, std::uint64_t value, void* sizeArgument, void* timeArgument) {
    return _umtx_op_nid_postfix(object, operation, value, sizeArgument, timeArgument);
}

std::int64_t APS5_VABI _write_nid_no_patch(int descriptor, const void* buffer, std::size_t count) {
    return _write_nid_postfix(descriptor, buffer, count);
}

int64_t APS5_VABI _writev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return _writev_nid_postfix(d, iov, iovcnt);
}

int APS5_VABI accept_nid_no_patch(int descriptor, void* address, std::uint32_t* length) {
    return accept_nid_postfix(descriptor, address, length);
}

int APS5_VABI access_nid_no_patch(const char* path, int mode) {
    return GuestAccess_nid_no_patch(path, mode);
}

int APS5_VABI bind_nid_no_patch(int descriptor, const void* address, std::uint32_t length) {
    return bind_nid_postfix(descriptor, address, length);
}

int APS5_VABI cfsetspeed_nid_no_patch(void* attributes, std::uint32_t speed) {
    return cfsetspeed_nid_postfix(attributes, speed);
}

int APS5_VABI chdir_nid_no_patch(const char* path) {
    return GuestChdir_nid_no_patch(path);
}

int APS5_VABI chflags_nid_no_patch(const char* path, std::uint64_t flags) {
    return chflags_nid_postfix(path, flags);
}

int APS5_VABI chmod_nid_no_patch(const char* path, int mode) {
    return chmod_nid_postfix(path, mode);
}

int APS5_VABI clock_getres_nid_no_patch(int clockId, KernelTimespec* res) {
    return clock_getres_nid_postfix(clockId, res);
}

int APS5_VABI clock_gettime_nid_no_patch(int clockId, KernelTimespec* tp) {
    return clock_gettime_nid_postfix(clockId, tp);
}

int APS5_VABI clock_settime_nid_no_patch(int clockId, const KernelTimespec* tp) {
    return clock_settime_nid_postfix(clockId, tp);
}

int APS5_VABI close_nid_no_patch(int d) {
    return close_nid_postfix(d);
}

int APS5_VABI connect_nid_no_patch(int descriptor, const void* address, std::uint32_t length) {
    return connect_nid_postfix(descriptor, address, length);
}

int APS5_VABI cpuset_getaffinity_nid_no_patch(int level, int which, std::int64_t id, std::size_t size, void* mask) {
    return cpuset_getaffinity_nid_postfix(level, which, id, size, mask);
}

int APS5_VABI cpuset_getid_nid_no_patch(int level, int which, std::int64_t id, int* setid) {
    return cpuset_getid_nid_postfix(level, which, id, setid);
}

int APS5_VABI cpuset_setaffinity_nid_no_patch(int level, int which, std::int64_t id, std::size_t size, const void* mask) {
    return cpuset_setaffinity_nid_postfix(level, which, id, size, mask);
}

int APS5_VABI creat_nid_no_patch(const char* path, int mode) {
    return creat_nid_postfix(path, mode);
}

int APS5_VABI dlclose_nid_no_patch(void* handle) {
    return dlclose_nid_postfix(handle);
}

char* APS5_VABI dlerror_nid_no_patch(void) {
    return dlerror_nid_postfix();
}

void* APS5_VABI dlopen_nid_no_patch(const char* path, int flags) {
    return dlopen_nid_postfix(path, flags);
}

void* APS5_VABI dlsym_nid_no_patch(void* handle, const char* name) {
    return dlsym_nid_postfix(handle, name);
}

int APS5_VABI dup_nid_no_patch(int d) {
    return dup_nid_postfix(d);
}

int APS5_VABI dup2_nid_no_patch(int from, int to) {
    return dup2_nid_postfix(from, to);
}

int APS5_VABI execv_nid_no_patch(const char* path, char* const* arguments) {
    return execv_nid_postfix(path, arguments);
}

int APS5_VABI execve_nid_no_patch(const char* path, char* const* arguments, char* const* environment) {
    return execve_nid_postfix(path, arguments, environment);
}

int APS5_VABI execvp_nid_no_patch(const char* file, char* const* arguments) {
    return execvp_nid_postfix(file, arguments);
}

int APS5_VABI fchflags_nid_no_patch(int d, std::uint64_t flags) {
    return fchflags_nid_postfix(d, flags);
}

int APS5_VABI fchmod_nid_no_patch(int d, int mode) {
    return fchmod_nid_postfix(d, mode);
}

int APS5_VABI fcntl_nid_no_patch(int descriptor, int command, std::uint64_t v0, std::uint64_t v1) {
    return fcntl_nid_postfix(descriptor, command, v0, v1);
}

int APS5_VABI flock_nid_no_patch(int d, int operation) {
    return flock_nid_postfix(d, operation);
}

int APS5_VABI fsync_nid_no_patch(int fd) {
    return fsync_nid_postfix(fd);
}

int APS5_VABI fdatasync_nid_no_patch(int fd) {
    return fdatasync_nid_postfix(fd);
}

int APS5_VABI ftruncate_nid_no_patch(int d, int64_t length) {
    return ftruncate_nid_postfix(d, length);
}

int APS5_VABI futimes_nid_no_patch(int d, const KernelTimeval* times) {
    return futimes_nid_postfix(d, times);
}

int APS5_VABI getargc_nid_no_patch(void) {
    return getargc_nid_postfix();
}

const char** APS5_VABI getargv_nid_no_patch(void) {
    return getargv_nid_postfix();
}int APS5_VABI getdents_nid_no_patch(int fd, char* buf, int nbytes) {
    return getdents_nid_postfix(fd, buf, nbytes);
}

int APS5_VABI getdirentries_nid_no_patch(int fd, char* buf, int nbytes, int64_t* basep) {
    return getdirentries_nid_postfix(fd, buf, nbytes, basep);
}

int APS5_VABI getdtablesize_nid_no_patch(void) {
    return getdtablesize_nid_postfix();
}

int APS5_VABI getegid_nid_no_patch(void) {
    return getegid_nid_postfix();
}

int APS5_VABI geteuid_nid_no_patch(void) {
    return geteuid_nid_postfix();
}

int APS5_VABI getgid_nid_no_patch(void) {
    return getgid_nid_postfix();
}

int APS5_VABI getgroups_nid_no_patch(int capacity, std::uint32_t* groups) {
    return getgroups_nid_postfix(capacity, groups);
}

int APS5_VABI getitimer_nid_no_patch(int which, GuestItimerval* output) {
    return getitimer_nid_postfix(which, output);
}

char* APS5_VABI getlogin_nid_no_patch(void) {
    return getlogin_nid_postfix();
}

int APS5_VABI getlogin_r_nid_no_patch(char* output, int size) {
    return getlogin_r_nid_postfix(output, size);
}

int APS5_VABI getpagesize_nid_no_patch(void) {
    return getpagesize_nid_postfix();
}

int APS5_VABI getpeername_nid_no_patch(int descriptor, void* address, std::uint32_t* length) {
    return getpeername_nid_postfix(descriptor, address, length);
}

int APS5_VABI getpid_nid_no_patch(void) {
    return getpid_nid_postfix();
}

int APS5_VABI getppid_nid_no_patch(void) {
    return getppid_nid_postfix();
}

int APS5_VABI getpriority_nid_no_patch(int which, int who) {
    return getpriority_nid_postfix(which, who);
}

int APS5_VABI getrlimit_nid_no_patch(int resource, GuestResourceLimit* limit) {
    return getrlimit_nid_postfix(resource, limit);
}

int APS5_VABI getrusage_nid_no_patch(int who, GuestResourceUsage* usage) {
    return getrusage_nid_postfix(who, usage);
}

int APS5_VABI getsid_nid_no_patch(int pid) {
    return getsid_nid_postfix(pid);
}

int APS5_VABI getsockname_nid_no_patch(int descriptor, void* address, std::uint32_t* length) {
    return getsockname_nid_postfix(descriptor, address, length);
}

int APS5_VABI getsockopt_nid_no_patch(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return getsockopt_nid_postfix(descriptor, level, option, value, length);
}

int APS5_VABI gettimeofday_nid_no_patch(KernelTimeval* tv, KernelTimezone* tz) {
    return gettimeofday_nid_postfix(tv, tz);
}

int APS5_VABI getuid_nid_no_patch(void) {
    return getuid_nid_postfix();
}

std::uint32_t APS5_VABI htonl_nid_no_patch(std::uint32_t value) {
    return htonl_nid_postfix(value);
}

std::uint16_t APS5_VABI htons_nid_no_patch(std::uint16_t value) {
    return htons_nid_postfix(value);
}

const char* APS5_VABI inet_ntop_nid_no_patch(int af, const void* src, char* dst, uint32_t size) {
    return inet_ntop_nid_postfix(af, src, dst, size);
}

int APS5_VABI inet_pton_nid_no_patch(int af, const char* src, void* dst) {
    return inet_pton_nid_postfix(af, src, dst);
}

int APS5_VABI ioctl_nid_no_patch(int descriptor, std::uint64_t request, void* argument) {
    return ioctl_nid_postfix(descriptor, request, argument);
}

int APS5_VABI issetugid_nid_no_patch(void) {
    return issetugid_nid_postfix();
}

int APS5_VABI kevent_nid_no_patch(int descriptor, const KernelEvent* changelist, int nchanges, KernelEvent* eventlist, int nevents, const KernelTimespec* timeout) {
    return kevent_nid_postfix(descriptor, changelist, nchanges, eventlist, nevents, timeout);
}

int APS5_VABI kill_nid_no_patch(int pid, int guest) {
    return kill_nid_postfix(pid, guest);
}

int APS5_VABI kqueue_nid_no_patch(void) {
    return kqueue_nid_postfix();
}

int APS5_VABI ksem_destroy_nid_no_patch(std::intptr_t id) {
    return ksem_destroy_nid_postfix(id);
}

int APS5_VABI ksem_getvalue_nid_no_patch(std::intptr_t id, int* value) {
    return ksem_getvalue_nid_postfix(id, value);
}

int APS5_VABI ksem_init_nid_no_patch(std::intptr_t* idp, unsigned int value) {
    return ksem_init_nid_postfix(idp, value);
}

int APS5_VABI ksem_post_nid_no_patch(std::intptr_t id) {
    return ksem_post_nid_postfix(id);
}

int APS5_VABI ksem_timedwait_nid_no_patch(std::intptr_t id, const KernelTimespec* abstime) {
    return ksem_timedwait_nid_postfix(id, abstime);
}

int APS5_VABI ksem_trywait_nid_no_patch(std::intptr_t id) {
    return ksem_trywait_nid_postfix(id);
}

int APS5_VABI ksem_wait_nid_no_patch(std::intptr_t id) {
    return ksem_wait_nid_postfix(id);
}

int APS5_VABI ktimer_create_nid_no_patch(int clock, const GuestSigevent* event, int* timerid) {
    return ktimer_create_nid_postfix(clock, event, timerid);
}

int APS5_VABI ktimer_delete_nid_no_patch(int id) {
    return ktimer_delete_nid_postfix(id);
}

int APS5_VABI ktimer_getoverrun_nid_no_patch(int id) {
    return ktimer_getoverrun_nid_postfix(id);
}

int APS5_VABI ktimer_gettime_nid_no_patch(int id, GuestItimerspec* value) {
    return ktimer_gettime_nid_postfix(id, value);
}

int APS5_VABI ktimer_settime_nid_no_patch(int id, int flags, const GuestItimerspec* value, GuestItimerspec* oldValue) {
    return ktimer_settime_nid_postfix(id, flags, value, oldValue);
}

int APS5_VABI listen_nid_no_patch(int descriptor, int backlog) {
    return listen_nid_postfix(descriptor, backlog);
}

int64_t APS5_VABI lseek_nid_no_patch(int d, int64_t offset, int whence) {
    return lseek_nid_postfix(d, offset, whence);
}

int APS5_VABI lstat_nid_no_patch(const char* path, FileStat* sb) {
    return lstat_nid_postfix(path, sb);
}

int APS5_VABI madvise_nid_no_patch(void* address, std::size_t length, int advice) {
    return madvise_nid_postfix(address, length, advice);
}

int APS5_VABI mincore_nid_no_patch(const void* address, std::size_t length, char* vec) {
    return mincore_nid_postfix(address, length, vec);
}

int APS5_VABI mkdir_nid_no_patch(const char* path, uint16_t mode) {
    return mkdir_nid_postfix(path, mode);
}

int APS5_VABI mlock_nid_no_patch(const void* address, std::size_t length) {
    return mlock_nid_postfix(address, length);
}

int APS5_VABI mlockall_nid_no_patch(int flags) {
    return mlockall_nid_postfix(flags);
}

void* APS5_VABI mmap_nid_no_patch(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) {
    return mmap_nid_postfix(address, length, protection, flags, descriptor, offset);
}

int APS5_VABI mprotect_nid_no_patch(void* address, std::size_t length, int protection) {
    return mprotect_nid_postfix(address, length, protection);
}

int APS5_VABI msync_nid_no_patch(void* address, std::size_t length, int flags) {
    return msync_nid_postfix(address, length, flags);
}

int APS5_VABI munlock_nid_no_patch(const void* address, std::size_t length) {
    return munlock_nid_postfix(address, length);
}

int APS5_VABI munlockall_nid_no_patch(void) {
    return munlockall_nid_postfix();
}

int APS5_VABI munmap_nid_no_patch(void* address, std::size_t length) {
    return munmap_nid_postfix(address, length);
}

int APS5_VABI nanosleep_nid_no_patch(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return nanosleep_nid_postfix(rqtp, rmtp);
}

std::uint32_t APS5_VABI ntohl_nid_no_patch(std::uint32_t value) {
    return ntohl_nid_postfix(value);
}

std::uint16_t APS5_VABI ntohs_nid_no_patch(std::uint16_t value) {
    return ntohs_nid_postfix(value);
}

int APS5_VABI open_nid_no_patch(const char* path, int flags, int mode) {
    return open_nid_postfix(path, flags, mode);
}

int APS5_VABI pause_nid_no_patch(void) {
    return pause_nid_postfix();
}

int APS5_VABI physhm_open_nid_no_patch(void) {
    return physhm_open_nid_postfix();
}

int APS5_VABI physhm_unlink_nid_no_patch(void) {
    return physhm_unlink_nid_postfix();
}

int APS5_VABI pipe_nid_no_patch(int* descriptors) {
    return pipe_nid_postfix(descriptors);
}

int APS5_VABI poll_nid_no_patch(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    return poll_nid_postfix(descriptors, count, timeout);
}

int64_t APS5_VABI pread_nid_no_patch(int d, void* buf, size_t nbytes, int64_t offset) {
    return pread_nid_postfix(d, buf, nbytes, offset);
}

int64_t APS5_VABI preadv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return preadv_nid_postfix(d, iov, iovcnt, offset);
}

int APS5_VABI pselect_nid_no_patch(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask) {
    return pselect_nid_postfix(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

int APS5_VABI pthread_atfork_nid_no_patch(PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child) {
    return pthread_atfork_nid_postfix(prepare, parent, child);
}

int APS5_VABI pthread_attr_destroy_nid_no_patch(PthreadAttr* attr) {
    return pthread_attr_destroy_nid_postfix(attr);
}

int APS5_VABI pthread_attr_get_np_nid_no_patch(Pthread thread, PthreadAttr* attr) {
    return pthread_attr_get_np_nid_postfix(thread, attr);
}

int APS5_VABI pthread_attr_getdetachstate_nid_no_patch(const PthreadAttr* attr, int* state) {
    return pthread_attr_getdetachstate_nid_postfix(attr, state);
}

int APS5_VABI pthread_attr_getguardsize_nid_no_patch(const PthreadAttr* attr, size_t* guardSize) {
    return pthread_attr_getguardsize_nid_postfix(attr, guardSize);
}

int APS5_VABI pthread_attr_getinheritsched_nid_no_patch(const PthreadAttr* attr, int* inheritSched) {
    return pthread_attr_getinheritsched_nid_postfix(attr, inheritSched);
}

int APS5_VABI pthread_attr_getschedparam_nid_no_patch(const PthreadAttr* attr, KernelSchedParam* param) {
    return pthread_attr_getschedparam_nid_postfix(attr, param);
}

int APS5_VABI pthread_attr_getschedpolicy_nid_no_patch(const PthreadAttr* attr, int* policy) {
    return pthread_attr_getschedpolicy_nid_postfix(attr, policy);
}

int APS5_VABI pthread_attr_getscope_nid_no_patch(const PthreadAttr* attr, int* scope) {
    return pthread_attr_getscope_nid_postfix(attr, scope);
}

int APS5_VABI pthread_attr_getstack_nid_no_patch(const PthreadAttr* __restrict attr, void** __restrict stackAddr, size_t* __restrict stackSize) {
    return pthread_attr_getstack_nid_postfix(attr, stackAddr, stackSize);
}

int APS5_VABI pthread_attr_getstackaddr_nid_no_patch(const PthreadAttr* __restrict attr, void** __restrict stackAddr) {
    return pthread_attr_getstackaddr_nid_postfix(attr, stackAddr);
}

int APS5_VABI pthread_attr_getstacksize_nid_no_patch(const PthreadAttr* attr, size_t* stackSize) {
    return pthread_attr_getstacksize_nid_postfix(attr, stackSize);
}

int APS5_VABI pthread_attr_init_nid_no_patch(PthreadAttr* attr) {
    return pthread_attr_init_nid_postfix(attr);
}

int APS5_VABI pthread_attr_setdetachstate_nid_no_patch(PthreadAttr* attr, int state) {
    return pthread_attr_setdetachstate_nid_postfix(attr, state);
}

int APS5_VABI pthread_attr_setguardsize_nid_no_patch(PthreadAttr* attr, size_t guardSize) {
    return pthread_attr_setguardsize_nid_postfix(attr, guardSize);
}

int APS5_VABI pthread_attr_setinheritsched_nid_no_patch(PthreadAttr* attr, int inheritSched) {
    return pthread_attr_setinheritsched_nid_postfix(attr, inheritSched);
}

int APS5_VABI pthread_attr_setschedparam_nid_no_patch(PthreadAttr* attr, const KernelSchedParam* param) {
    return pthread_attr_setschedparam_nid_postfix(attr, param);
}

int APS5_VABI pthread_attr_setschedpolicy_nid_no_patch(PthreadAttr* attr, int policy) {
    return pthread_attr_setschedpolicy_nid_postfix(attr, policy);
}

int APS5_VABI pthread_attr_setscope_nid_no_patch(PthreadAttr* attr, int scope) {
    return pthread_attr_setscope_nid_postfix(attr, scope);
}

int APS5_VABI pthread_attr_setsolosched_np_nid_no_patch(PthreadAttr* attr, int solosched) {
    return pthread_attr_setsolosched_np_nid_postfix(attr, solosched);
}

int APS5_VABI pthread_attr_setstack_nid_no_patch(PthreadAttr* attr, void* stackAddr, size_t stackSize) {
    return pthread_attr_setstack_nid_postfix(attr, stackAddr, stackSize);
}

int APS5_VABI pthread_attr_setstacksize_nid_no_patch(PthreadAttr* attr, size_t stackSize) {
    return pthread_attr_setstacksize_nid_postfix(attr, stackSize);
}

int APS5_VABI pthread_barrier_destroy_nid_no_patch(PthreadBarrierPrivate** barrier) {
    return pthread_barrier_destroy_nid_postfix(barrier);
}

int APS5_VABI pthread_barrier_init_nid_no_patch(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count) {
    return pthread_barrier_init_nid_postfix(barrier, attr, count);
}

int APS5_VABI pthread_barrier_setname_np_nid_no_patch(PthreadBarrierPrivate** barrier, const char* name) {
    return pthread_barrier_setname_np_nid_postfix(barrier, name);
}

int APS5_VABI pthread_barrier_wait_nid_no_patch(PthreadBarrierPrivate** barrier) {
    return pthread_barrier_wait_nid_postfix(barrier);
}

int APS5_VABI pthread_barrierattr_destroy_nid_no_patch(PthreadBarrierattrPrivate** attr) {
    return pthread_barrierattr_destroy_nid_postfix(attr);
}

int APS5_VABI pthread_barrierattr_getpshared_nid_no_patch(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    return pthread_barrierattr_getpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_barrierattr_init_nid_no_patch(PthreadBarrierattrPrivate** attr) {
    return pthread_barrierattr_init_nid_postfix(attr);
}

int APS5_VABI pthread_barrierattr_setpshared_nid_no_patch(PthreadBarrierattrPrivate** attr, int pshared) {
    return pthread_barrierattr_setpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_cancel_nid_no_patch(Pthread thread) {
    return pthread_cancel_nid_postfix(thread);
}

void APS5_VABI pthread_cleanup_pop_nid_no_patch(int execute) {
    pthread_cleanup_pop_nid_postfix(execute);
}

void APS5_VABI pthread_cleanup_push_nid_no_patch(void (APS5_VABI* routine)(void*), void* argument) {
    pthread_cleanup_push_nid_postfix(routine, argument);
}

int APS5_VABI pthread_cond_broadcast_nid_no_patch(PthreadCond* cond) {
    return pthread_cond_broadcast_nid_postfix(cond);
}

int APS5_VABI pthread_cond_destroy_nid_no_patch(PthreadCond* cond) {
    return pthread_cond_destroy_nid_postfix(cond);
}

int APS5_VABI pthread_cond_init_nid_no_patch(PthreadCond* cond, const PthreadCondattr* attr) {
    return pthread_cond_init_nid_postfix(cond, attr);
}

int APS5_VABI pthread_cond_reltimedwait_np_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return pthread_cond_reltimedwait_np_nid_postfix(cond, mutex, usec);
}

int APS5_VABI pthread_cond_setname_np_nid_no_patch(PthreadCond* cond, const char* name) {
    return pthread_cond_setname_np_nid_postfix(cond, name);
}

int APS5_VABI pthread_cond_signal_nid_no_patch(PthreadCond* cond) {
    return pthread_cond_signal_nid_postfix(cond);
}

int APS5_VABI pthread_cond_signalto_np_nid_no_patch(PthreadCond* cond, Pthread thread) {
    return pthread_cond_signalto_np_nid_postfix(cond, thread);
}

int APS5_VABI pthread_cond_timedwait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    return pthread_cond_timedwait_nid_postfix(cond, mutex, abstime);
}

int APS5_VABI pthread_cond_wait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex) {
    return pthread_cond_wait_nid_postfix(cond, mutex);
}

int APS5_VABI pthread_condattr_destroy_nid_no_patch(PthreadCondattr* attr) {
    return pthread_condattr_destroy_nid_postfix(attr);
}

int APS5_VABI pthread_condattr_getclock_nid_no_patch(const PthreadCondattr* attr, KernelClockid* clockId) {
    return pthread_condattr_getclock_nid_postfix(attr, clockId);
}

int APS5_VABI pthread_condattr_getpshared_nid_no_patch(const PthreadCondattr* attr, int* pshared) {
    return pthread_condattr_getpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_condattr_init_nid_no_patch(PthreadCondattr* attr) {
    return pthread_condattr_init_nid_postfix(attr);
}

int APS5_VABI pthread_condattr_setclock_nid_no_patch(PthreadCondattr* attr, KernelClockid clockId) {
    return pthread_condattr_setclock_nid_postfix(attr, clockId);
}

int APS5_VABI pthread_condattr_setpshared_nid_no_patch(PthreadCondattr* attr, int pshared) {
    return pthread_condattr_setpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_create_nid_no_patch(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg) {
    return pthread_create_nid_postfix(thread, attr, entry, arg);
}

int APS5_VABI pthread_create_name_np_nid_no_patch(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return pthread_create_name_np_nid_postfix(thread, attr, entry, arg, name);
}

int APS5_VABI pthread_detach_nid_no_patch(Pthread thread) {
    return pthread_detach_nid_postfix(thread);
}

int APS5_VABI pthread_equal_nid_no_patch(Pthread first, Pthread second) {
    return pthread_equal_nid_postfix(first, second);
}

void APS5_VABI pthread_exit_nid_no_patch(void* value) {
    pthread_exit_nid_postfix(value);
}

int APS5_VABI pthread_getaffinity_np_nid_no_patch(Pthread thread, std::size_t cpusetSize, void* cpuset) {
    return pthread_getaffinity_np_nid_postfix(thread, cpusetSize, cpuset);
}

int APS5_VABI pthread_getconcurrency_nid_no_patch(void) {
    return pthread_getconcurrency_nid_postfix();
}

int APS5_VABI pthread_getcpuclockid_nid_no_patch(Pthread thread, int* clockId) {
    return pthread_getcpuclockid_nid_postfix(thread, clockId);
}

int APS5_VABI pthread_getname_np_nid_no_patch(Pthread thread, char* name) {
    return pthread_getname_np_nid_postfix(thread, name);
}

int APS5_VABI pthread_getprio_nid_no_patch(Pthread thread) {
    return pthread_getprio_nid_postfix(thread);
}

int APS5_VABI pthread_getschedparam_nid_no_patch(Pthread thread, int* policy, KernelSchedParam* param) {
    return pthread_getschedparam_nid_postfix(thread, policy, param);
}

void* APS5_VABI pthread_getspecific_nid_no_patch(PthreadKey key) {
    return pthread_getspecific_nid_postfix(key);
}

int APS5_VABI pthread_getthreadid_np_nid_no_patch(void) {
    return pthread_getthreadid_np_nid_postfix();
}

int APS5_VABI pthread_join_nid_no_patch(Pthread thread, void** value) {
    return pthread_join_nid_postfix(thread, value);
}

int APS5_VABI pthread_key_create_nid_no_patch(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return pthread_key_create_nid_postfix(key, destructor);
}

int APS5_VABI pthread_key_delete_nid_no_patch(PthreadKey key) {
    return pthread_key_delete_nid_postfix(key);
}

int APS5_VABI pthread_kill_nid_no_patch(Pthread thread, int guest) {
    return pthread_kill_nid_postfix(thread, guest);
}

int APS5_VABI pthread_mutex_destroy_nid_no_patch(PthreadMutex* mutex) {
    return pthread_mutex_destroy_nid_postfix(mutex);
}

int APS5_VABI pthread_mutex_getprioceiling_nid_no_patch(const PthreadMutex* mutex, int* prioceiling) {
    return pthread_mutex_getprioceiling_nid_postfix(mutex, prioceiling);
}

int APS5_VABI pthread_mutex_getspinloops_np_nid_no_patch(PthreadMutex* mutex, int* count) {
    return pthread_mutex_getspinloops_np_nid_postfix(mutex, count);
}

int APS5_VABI pthread_mutex_getyieldloops_np_nid_no_patch(PthreadMutex* mutex, int* count) {
    return pthread_mutex_getyieldloops_np_nid_postfix(mutex, count);
}

int APS5_VABI pthread_mutex_init_nid_no_patch(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return pthread_mutex_init_nid_postfix(mutex, attr);
}

int APS5_VABI pthread_mutex_isowned_np_nid_no_patch(PthreadMutex* mutex) {
    return pthread_mutex_isowned_np_nid_postfix(mutex);
}

int APS5_VABI pthread_mutex_lock_nid_no_patch(PthreadMutex* mutex) {
    return pthread_mutex_lock_nid_postfix(mutex);
}

int APS5_VABI pthread_mutex_reltimedlock_np_nid_no_patch(PthreadMutex* mutex, KernelUseconds usec) {
    return pthread_mutex_reltimedlock_np_nid_postfix(mutex, usec);
}

int APS5_VABI pthread_mutex_setname_np_nid_no_patch(PthreadMutex* mutex, const char* name) {
    return pthread_mutex_setname_np_nid_postfix(mutex, name);
}

int APS5_VABI pthread_mutex_setprioceiling_nid_no_patch(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return pthread_mutex_setprioceiling_nid_postfix(mutex, ceiling, oldCeiling);
}

int APS5_VABI pthread_mutex_setspinloops_np_nid_no_patch(PthreadMutex* mutex, int count) {
    return pthread_mutex_setspinloops_np_nid_postfix(mutex, count);
}

int APS5_VABI pthread_mutex_setyieldloops_np_nid_no_patch(PthreadMutex* mutex, int count) {
    return pthread_mutex_setyieldloops_np_nid_postfix(mutex, count);
}

int APS5_VABI pthread_mutex_timedlock_nid_no_patch(PthreadMutex* mutex, const KernelTimespec* abstime) {
    return pthread_mutex_timedlock_nid_postfix(mutex, abstime);
}

int APS5_VABI pthread_mutex_trylock_nid_no_patch(PthreadMutex* mutex) {
    return pthread_mutex_trylock_nid_postfix(mutex);
}

int APS5_VABI pthread_mutex_unlock_nid_no_patch(PthreadMutex* mutex) {
    return pthread_mutex_unlock_nid_postfix(mutex);
}

int APS5_VABI pthread_mutexattr_destroy_nid_no_patch(PthreadMutexattr* attr) {
    return pthread_mutexattr_destroy_nid_postfix(attr);
}

int APS5_VABI pthread_mutexattr_getkind_np_nid_no_patch(PthreadMutexattr attr) {
    return pthread_mutexattr_getkind_np_nid_postfix(attr);
}

int APS5_VABI pthread_mutexattr_getprioceiling_nid_no_patch(const PthreadMutexattr* attr, int* prioceiling) {
    return pthread_mutexattr_getprioceiling_nid_postfix(attr, prioceiling);
}

int APS5_VABI pthread_mutexattr_getprotocol_nid_no_patch(const PthreadMutexattr* attr, int* protocol) {
    return pthread_mutexattr_getprotocol_nid_postfix(attr, protocol);
}

int APS5_VABI pthread_mutexattr_getpshared_nid_no_patch(const PthreadMutexattr* attr, int* pshared) {
    return pthread_mutexattr_getpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_mutexattr_gettype_nid_no_patch(const PthreadMutexattr* attr, int* type) {
    return pthread_mutexattr_gettype_nid_postfix(attr, type);
}

int APS5_VABI pthread_mutexattr_init_nid_no_patch(PthreadMutexattr* attr) {
    return pthread_mutexattr_init_nid_postfix(attr);
}

int APS5_VABI pthread_mutexattr_setkind_np_nid_no_patch(PthreadMutexattr* attr, int kind) {
    return pthread_mutexattr_setkind_np_nid_postfix(attr, kind);
}

int APS5_VABI pthread_mutexattr_setprioceiling_nid_no_patch(PthreadMutexattr* attr, int prioceiling) {
    return pthread_mutexattr_setprioceiling_nid_postfix(attr, prioceiling);
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_no_patch(PthreadMutexattr* attr, int protocol) {
    return pthread_mutexattr_setprotocol_nid_postfix(attr, protocol);
}

int APS5_VABI pthread_mutexattr_setpshared_nid_no_patch(PthreadMutexattr* attr, int pshared) {
    return pthread_mutexattr_setpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_mutexattr_settype_nid_no_patch(PthreadMutexattr* attr, int type) {
    return pthread_mutexattr_settype_nid_postfix(attr, type);
}

int APS5_VABI pthread_once_nid_no_patch(GuestOnce* control, void (APS5_VABI *initialize)()) {
    return pthread_once_nid_postfix(control, initialize);
}

int APS5_VABI pthread_rename_np_nid_no_patch(Pthread thread, const char* name) {
    return pthread_rename_np_nid_postfix(thread, name);
}

int APS5_VABI pthread_rwlock_destroy_nid_no_patch(PthreadRwlock* rwlock) {
    return pthread_rwlock_destroy_nid_postfix(rwlock);
}

int APS5_VABI pthread_rwlock_init_nid_no_patch(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    return pthread_rwlock_init_nid_postfix(rwlock, attr);
}

int APS5_VABI pthread_rwlock_rdlock_nid_no_patch(PthreadRwlock* rwlock) {
    return pthread_rwlock_rdlock_nid_postfix(rwlock);
}

int APS5_VABI pthread_rwlock_reltimedrdlock_np_nid_no_patch(PthreadRwlock* rwlock, KernelUseconds usec) {
    return pthread_rwlock_reltimedrdlock_np_nid_postfix(rwlock, usec);
}

int APS5_VABI pthread_rwlock_reltimedwrlock_np_nid_no_patch(PthreadRwlock* rwlock, KernelUseconds usec) {
    return pthread_rwlock_reltimedwrlock_np_nid_postfix(rwlock, usec);
}

int APS5_VABI pthread_rwlock_setname_np_nid_no_patch(PthreadRwlock* rwlock, const char* name) {
    return pthread_rwlock_setname_np_nid_postfix(rwlock, name);
}

int APS5_VABI pthread_rwlock_timedrdlock_nid_no_patch(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    return pthread_rwlock_timedrdlock_nid_postfix(rwlock, abstime);
}

int APS5_VABI pthread_rwlock_timedwrlock_nid_no_patch(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    return pthread_rwlock_timedwrlock_nid_postfix(rwlock, abstime);
}

int APS5_VABI pthread_rwlock_tryrdlock_nid_no_patch(PthreadRwlock* rwlock) {
    return pthread_rwlock_tryrdlock_nid_postfix(rwlock);
}

int APS5_VABI pthread_rwlock_trywrlock_nid_no_patch(PthreadRwlock* rwlock) {
    return pthread_rwlock_trywrlock_nid_postfix(rwlock);
}

int APS5_VABI pthread_rwlock_unlock_nid_no_patch(PthreadRwlock* rwlock) {
    return pthread_rwlock_unlock_nid_postfix(rwlock);
}

int APS5_VABI pthread_rwlock_wrlock_nid_no_patch(PthreadRwlock* rwlock) {
    return pthread_rwlock_wrlock_nid_postfix(rwlock);
}

int APS5_VABI pthread_rwlockattr_destroy_nid_no_patch(PthreadRwlockattr* attr) {
    return pthread_rwlockattr_destroy_nid_postfix(attr);
}

int APS5_VABI pthread_rwlockattr_getpshared_nid_no_patch(const PthreadRwlockattr* attr, int* pshared) {
    return pthread_rwlockattr_getpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_rwlockattr_gettype_np_nid_no_patch(const PthreadRwlockattr* attr, int* type) {
    return pthread_rwlockattr_gettype_np_nid_postfix(attr, type);
}

int APS5_VABI pthread_rwlockattr_init_nid_no_patch(PthreadRwlockattr* attr) {
    return pthread_rwlockattr_init_nid_postfix(attr);
}

int APS5_VABI pthread_rwlockattr_setpshared_nid_no_patch(PthreadRwlockattr* attr, int pshared) {
    return pthread_rwlockattr_setpshared_nid_postfix(attr, pshared);
}

int APS5_VABI pthread_rwlockattr_settype_np_nid_no_patch(PthreadRwlockattr* attr, int type) {
    return pthread_rwlockattr_settype_np_nid_postfix(attr, type);
}

Pthread APS5_VABI pthread_self_nid_no_patch(void) {
    return pthread_self_nid_postfix();
}

void APS5_VABI pthread_set_name_np_nid_no_patch(Pthread thread, const char* name) {
    pthread_set_name_np_nid_postfix(thread, name);
}

int APS5_VABI pthread_setaffinity_np_nid_no_patch(Pthread thread, std::size_t cpusetSize, const void* cpuset) {
    return pthread_setaffinity_np_nid_postfix(thread, cpusetSize, cpuset);
}

int APS5_VABI pthread_setcancelstate_nid_no_patch(int state, int* oldState) {
    return pthread_setcancelstate_nid_postfix(state, oldState);
}

int APS5_VABI pthread_setcanceltype_nid_no_patch(int type, int* oldType) {
    return pthread_setcanceltype_nid_postfix(type, oldType);
}

int APS5_VABI pthread_setconcurrency_nid_no_patch(int newLevel) {
    return pthread_setconcurrency_nid_postfix(newLevel);
}

int APS5_VABI pthread_setprio_nid_no_patch(Pthread thread, int prio) {
    return pthread_setprio_nid_postfix(thread, prio);
}

int APS5_VABI pthread_setschedparam_nid_no_patch(Pthread thread, int policy, const KernelSchedParam* param) {
    return pthread_setschedparam_nid_postfix(thread, policy, param);
}

int APS5_VABI pthread_setspecific_nid_no_patch(PthreadKey key, void* value) {
    return pthread_setspecific_nid_postfix(key, value);
}

int APS5_VABI pthread_sigmask_nid_no_patch(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return pthread_sigmask_nid_postfix(how, set, previousSet);
}

int APS5_VABI pthread_spin_destroy_nid_no_patch(PthreadSpinlockPrivate** lock) {
    return pthread_spin_destroy_nid_postfix(lock);
}

int APS5_VABI pthread_spin_init_nid_no_patch(PthreadSpinlockPrivate** lock, int pshared) {
    return pthread_spin_init_nid_postfix(lock, pshared);
}

int APS5_VABI pthread_spin_lock_nid_no_patch(PthreadSpinlockPrivate** lock) {
    return pthread_spin_lock_nid_postfix(lock);
}

int APS5_VABI pthread_spin_trylock_nid_no_patch(PthreadSpinlockPrivate** lock) {
    return pthread_spin_trylock_nid_postfix(lock);
}

int APS5_VABI pthread_spin_unlock_nid_no_patch(PthreadSpinlockPrivate** lock) {
    return pthread_spin_unlock_nid_postfix(lock);
}

void APS5_VABI pthread_testcancel_nid_no_patch(void) {
    pthread_testcancel_nid_postfix();
}

int APS5_VABI pthread_timedjoin_np_nid_no_patch(Pthread thread, void** value, const KernelTimespec* abstime) {
    return pthread_timedjoin_np_nid_postfix(thread, value, abstime);
}

void APS5_VABI pthread_yield_nid_no_patch(void) {
    pthread_yield_nid_postfix();
}

int64_t APS5_VABI pwritev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return pwritev_nid_postfix(d, iov, iovcnt, offset);
}

int APS5_VABI raise_nid_no_patch(int guest) {
    return raise_nid_postfix(guest);
}

int64_t APS5_VABI read_nid_no_patch(int d, void* buf, uint64_t nbytes) {
    return read_nid_postfix(d, buf, nbytes);
}

int64_t APS5_VABI readv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return readv_nid_postfix(d, iov, iovcnt);
}

std::int64_t APS5_VABI recv_nid_no_patch(int descriptor, void* buffer, std::uint64_t length, int flags) {
    return recv_nid_postfix(descriptor, buffer, length, flags);
}

std::int64_t APS5_VABI recvfrom_nid_no_patch(int descriptor, void* buffer, std::uint64_t length, int flags, void* address, std::uint32_t* addressLength) {
    return recvfrom_nid_postfix(descriptor, buffer, length, flags, address, addressLength);
}

std::int64_t APS5_VABI recvmsg_nid_no_patch(int descriptor, GuestMsghdr* message, int flags) {
    return recvmsg_nid_postfix(descriptor, message, flags);
}

int APS5_VABI rename_nid_no_patch(const char* from, const char* to) {
    return rename_nid_postfix(from, to);
}

int APS5_VABI rfork_thread_nid_no_patch(int flags, void* stack, int (APS5_VABI* function)(void*), void* argument) {
    return rfork_thread_nid_postfix(flags, stack, function, argument);
}

int APS5_VABI rmdir_nid_no_patch(const char* path) {
    return rmdir_nid_postfix(path);
}

int APS5_VABI rtprio_thread_nid_no_patch(int function, int lwpid, GuestRtPrio* priority) {
    return rtprio_thread_nid_postfix(function, lwpid, priority);
}

int APS5_VABI sceKernelAddFileEvent_nid_no_patch(KernelEqueue eq, int fd, int watch, void* udata) {
    return sceKernelAddFileEvent(eq, fd, watch, udata);
}

int APS5_VABI sceKernelAddHRTimerEvent_nid_no_patch(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
    return sceKernelAddHRTimerEvent(eq, id, ts, udata);
}

int APS5_VABI sceKernelAddReadEvent_nid_no_patch(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    return sceKernelAddReadEvent(eq, fd, size, udata);
}

int APS5_VABI sceKernelAddTimerEvent_nid_no_patch(KernelEqueue eq, int id, KernelUseconds usec, void* udata) {
    return sceKernelAddTimerEvent(eq, id, usec, udata);
}

int APS5_VABI sceKernelAddUserEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelAddUserEvent(eq, id);
}

int APS5_VABI sceKernelAddUserEventEdge_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelAddUserEventEdge(eq, id);
}

int APS5_VABI sceKernelAddWriteEvent_nid_no_patch(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    return sceKernelAddWriteEvent(eq, fd, size, udata);
}

int APS5_VABI sceKernelAddAmprEvent_nid_no_patch(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprEvent(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprEvent(eq, id);
}

int APS5_VABI sceKernelAddAmprSystemEvent_nid_no_patch(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprSystemEvent(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprSystemEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprSystemEvent(eq, id);
}

int APS5_VABI sceKernelAioCancelRequest_nid_no_patch(int32_t id, int32_t* state) {
    return sceKernelAioCancelRequest(id, state);
}

int APS5_VABI sceKernelAioCancelRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* state) {
    return sceKernelAioCancelRequests(id, num, state);
}

int APS5_VABI sceKernelAioDeleteRequest_nid_no_patch(int32_t id, int32_t* ret) {
    return sceKernelAioDeleteRequest(id, ret);
}

int APS5_VABI sceKernelAioDeleteRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* ret) {
    return sceKernelAioDeleteRequests(id, num, ret);
}

int APS5_VABI sceKernelAioInitializeImpl_nid_no_patch(void* param, int32_t size) {
    return sceKernelAioInitializeImpl(param, size);
}

void APS5_VABI sceKernelAioInitializeParam_nid_no_patch(void* param) {
    sceKernelAioInitializeParam(param);
}

int APS5_VABI sceKernelAioPollRequest_nid_no_patch(int32_t id, int32_t* state) {
    return sceKernelAioPollRequest(id, state);
}

int APS5_VABI sceKernelAioPollRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* state) {
    return sceKernelAioPollRequests(id, num, state);
}

int APS5_VABI sceKernelAioSetParam_nid_no_patch(KernelAioSchedulingParam* param, int32_t schedulingWindowSize, int32_t delayedCountLimit, uint32_t enableSplit, uint32_t splitSize, uint32_t splitChunkSize) {
    return sceKernelAioSetParam(param, schedulingWindowSize, delayedCountLimit, enableSplit, splitSize, splitChunkSize);
}

int APS5_VABI sceKernelAioSubmitReadCommands_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitReadCommands(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitReadCommandsMultiple_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitReadCommandsMultiple(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommands_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitWriteCommands(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommandsMultiple_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitWriteCommandsMultiple(req, size, prio, id);
}

int APS5_VABI sceKernelAioWaitRequest_nid_no_patch(int32_t id, int32_t* state, uint32_t* usec) {
    return sceKernelAioWaitRequest(id, state, usec);
}

int APS5_VABI sceKernelAioWaitRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* state, uint32_t mode, uint32_t* usec) {
    return sceKernelAioWaitRequests(id, num, state, mode, usec);
}

int APS5_VABI sceKernelAllocateDirectMemory_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateDirectMemory(searchStart, searchEnd, len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAllocateDirectMemoryForApp_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateDirectMemoryForApp(searchStart, searchEnd, len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAllocateMainDirectMemory_nid_no_patch(size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateMainDirectMemory(len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAvailableDirectMemorySize_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t alignment, int64_t* physAddrOut, size_t* sizeOut) {
    return sceKernelAvailableDirectMemorySize(searchStart, searchEnd, alignment, physAddrOut, sizeOut);
}

int APS5_VABI sceKernelAvailableFlexibleMemorySize_nid_no_patch(size_t* size) {
    return sceKernelAvailableFlexibleMemorySize(size);
}

int APS5_VABI sceKernelBatchMap_nid_no_patch(KernelBatchMapEntry* entries, int numEntries, int* numEntriesOut) {
    return sceKernelBatchMap(entries, numEntries, numEntriesOut);
}

int APS5_VABI sceKernelBatchMap2_nid_no_patch(KernelBatchMapEntry* entries, int numEntries, int* numEntriesOut, int flags) {
    return sceKernelBatchMap2(entries, numEntries, numEntriesOut, flags);
}

int APS5_VABI sceKernelCancelEventFlag_nid_no_patch(KernelEventFlag ef, std::uint64_t setPattern, int* numWaitThreads) {
    return sceKernelCancelEventFlag(ef, setPattern, numWaitThreads);
}

int APS5_VABI sceKernelCancelSema_nid_no_patch(KernelSema sem, int count, int* threads) {
    return sceKernelCancelSema(sem, count, threads);
}

int APS5_VABI sceKernelCheckReachability_nid_no_patch(const char* path) {
    return sceKernelCheckReachability(path);
}

int APS5_VABI sceKernelCheckedReleaseDirectMemory_nid_no_patch(int64_t start, size_t len) {
    return sceKernelCheckedReleaseDirectMemory(start, len);
}

int APS5_VABI sceKernelChmod_nid_no_patch(const char* path, std::uint16_t mode) {
    return sceKernelChmod_nid_postfix(path, mode);
}

int APS5_VABI sceKernelClearEventFlag_nid_no_patch(KernelEventFlag ef, uint64_t bitPattern) {
    return sceKernelClearEventFlag(ef, bitPattern);
}

int APS5_VABI sceKernelClockGetres_nid_no_patch(KernelClockid clockId, KernelTimespec* tp) {
    return sceKernelClockGetres(clockId, tp);
}

int APS5_VABI sceKernelClockGettime_nid_no_patch(KernelClockid clockId, KernelTimespec* tp) {
    return sceKernelClockGettime(clockId, tp);
}

int APS5_VABI sceKernelClose_nid_no_patch(int d) {
    return sceKernelClose(d);
}

int APS5_VABI sceKernelCloseEventFlag_nid_no_patch(KernelEventFlag ef) {
    return sceKernelCloseEventFlag(ef);
}

int APS5_VABI sceKernelConfiguredFlexibleMemorySize_nid_no_patch(size_t* size) {
    return sceKernelConfiguredFlexibleMemorySize(size);
}

int APS5_VABI sceKernelConvertLocaltimeToUtc_nid_no_patch(int64_t localTime, int64_t reserved, int64_t* utcTime, KernelTimesec* timezone, int32_t* dstSeconds) {
    return sceKernelConvertLocaltimeToUtc(localTime, reserved, utcTime, timezone, dstSeconds);
}

int APS5_VABI sceKernelConvertUtcToLocaltime_nid_no_patch(int64_t utcTime, int64_t* localTime, KernelTimesec* st, int32_t* dstSec) {
    return sceKernelConvertUtcToLocaltime(utcTime, localTime, st, dstSec);
}

int APS5_VABI sceKernelCreateEqueue_nid_no_patch(KernelEqueue* eq, const char* name) {
    return sceKernelCreateEqueue(eq, name);
}

int APS5_VABI sceKernelCreateEventFlag_nid_no_patch(KernelEventFlag* ef, const char* name, std::uint32_t attr, std::uint64_t initPattern, const void* param) {
    return sceKernelCreateEventFlag(ef, name, attr, initPattern, param);
}

int APS5_VABI sceKernelCreateSema_nid_no_patch(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
    return sceKernelCreateSema(sem, name, attr, init, max, opt);
}

int APS5_VABI sceKernelDebugOutText_nid_no_patch(int channel, const char* text) {
    return sceKernelDebugOutText(channel, text);
}

void APS5_VABI sceKernelDebugRaiseException_nid_no_patch(int c1, int c2) {
    sceKernelDebugRaiseException(c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode_nid_no_patch(int c1, int c2) {
    sceKernelDebugRaiseExceptionOnReleaseMode(c1, c2);
}

int APS5_VABI sceKernelDebugWriteCppExceptionInfo_nid_no_patch(const void* exception, uint64_t unknown, const char* typeName, const char* what) {
    return sceKernelDebugWriteCppExceptionInfo(exception, unknown, typeName, what);
}

int APS5_VABI sceKernelDeleteEqueue_nid_no_patch(KernelEqueue eq) {
    return sceKernelDeleteEqueue(eq);
}

int APS5_VABI sceKernelDeleteEventFlag_nid_no_patch(KernelEventFlag ef) {
    return sceKernelDeleteEventFlag(ef);
}

int APS5_VABI sceKernelDeleteFileEvent_nid_no_patch(KernelEqueue eq, int fd) {
    return sceKernelDeleteFileEvent(eq, fd);
}

int APS5_VABI sceKernelDeleteHRTimerEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteHRTimerEvent(eq, id);
}

int APS5_VABI sceKernelDeleteReadEvent_nid_no_patch(KernelEqueue eq, int fd) {
    return sceKernelDeleteReadEvent(eq, fd);
}

int APS5_VABI sceKernelDeleteSema_nid_no_patch(KernelSema sem) {
    return sceKernelDeleteSema(sem);
}

int APS5_VABI sceKernelDeleteTimerEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteTimerEvent(eq, id);
}

int APS5_VABI sceKernelDeleteUserEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteUserEvent(eq, id);
}

int APS5_VABI sceKernelDeleteWriteEvent_nid_no_patch(KernelEqueue eq, int fd) {
    return sceKernelDeleteWriteEvent(eq, fd);
}

int APS5_VABI sceKernelDirectMemoryQuery_nid_no_patch(int64_t offset, int flags, void* info, size_t infoSize) {
    return sceKernelDirectMemoryQuery(offset, flags, info, infoSize);
}

int APS5_VABI sceKernelDlsym_nid_no_patch(KernelModule handle, const char* symbol, void** addr) {
    return sceKernelDlsym(handle, symbol, addr);
}

int APS5_VABI sceKernelEnableDmemAliasing_nid_no_patch(void) {
    return sceKernelEnableDmemAliasing();
}

std::int32_t APS5_VABI sceKernelError_nid_no_patch(std::int32_t posixError) {
    return sceKernelError(posixError);
}

int APS5_VABI sceKernelFchmod_nid_no_patch(int d, std::uint16_t mode) {
    return sceKernelFchmod(d, mode);
}

int APS5_VABI sceKernelFcntl_nid_no_patch(int d, int command, std::uint64_t v0, std::uint64_t v1) {
    return sceKernelFcntl(d, command, v0, v1);
}

int APS5_VABI sceKernelFstat_nid_no_patch(int d, FileStat* sb) {
    return sceKernelFstat(d, sb);
}

int APS5_VABI sceKernelFsync_nid_no_patch(int fd) {
    return sceKernelFsync(fd);
}

int APS5_VABI sceKernelFdatasync_nid_no_patch(int fd) {
    return sceKernelFdatasync(fd);
}

int APS5_VABI sceKernelFtruncate_nid_no_patch(int d, int64_t length) {
    return sceKernelFtruncate(d, length);
}

int APS5_VABI sceKernelFutimes_nid_no_patch(int d, const KernelTimeval* times) {
    return sceKernelFutimes(d, times);
}

int APS5_VABI sceKernelGetAllowedSdkVersionOnSystem_nid_no_patch(std::uint32_t* version) {
    return sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(version);
}

int APS5_VABI sceKernelGetAppInfo_nid_no_patch(int pid, void* info) {
    return sceKernelGetAppInfo(pid, info);
}

std::int64_t APS5_VABI sceKernelGetCpuFrequency_nid_no_patch(void) {
    return sceKernelGetCpuFrequency();
}

int APS5_VABI sceKernelGetCpuTemperature_nid_no_patch(int* outCelsius) {
    return sceKernelGetCpuTemperature(outCelsius);
}

int APS5_VABI sceKernelGetCpumode_nid_no_patch(void) {
    return sceKernelGetCpumode();
}

int APS5_VABI sceKernelGetCurrentCpu_nid_no_patch(void) {
    return sceKernelGetCurrentCpu();
}

size_t APS5_VABI sceKernelGetDirectMemorySize_nid_no_patch(void) {
    return sceKernelGetDirectMemorySize();
}

int APS5_VABI sceKernelGetDirectMemoryType_nid_no_patch(int64_t offset, int* memoryType, int64_t* start, int64_t* end) {
    return sceKernelGetDirectMemoryType(offset, memoryType, start, end);
}

intptr_t APS5_VABI sceKernelGetEventData_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventData(ev);
}

int APS5_VABI sceKernelGetEventError_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventError(ev);
}

intptr_t APS5_VABI sceKernelGetEventFflags_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventFflags(ev);
}

int APS5_VABI sceKernelGetEventFilter_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventFilter(ev);
}

uintptr_t APS5_VABI sceKernelGetEventId_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventId(ev);
}

void* APS5_VABI sceKernelGetEventUserData_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventUserData(ev);
}

KernelModule APS5_VABI sceKernelGetExecutableModuleHandle_nid_no_patch(void) {
    return sceKernelGetExecutableModuleHandle();
}

const char* APS5_VABI sceKernelGetFsSandboxRandomWord_nid_no_patch(void) {
    return sceKernelGetFsSandboxRandomWord();
}

std::uint64_t APS5_VABI sceKernelGetGPI_nid_no_patch(void) {
    return sceKernelGetGPI();
}

std::int32_t APS5_VABI sceKernelGetLibkernelTextLocation_nid_no_patch(void** address, std::size_t* size) {
    return sceKernelGetLibkernelTextLocation_nid_postfix(address, size);
}

int APS5_VABI sceKernelGetMainSocId_nid_no_patch(void) {
    return sceKernelGetMainSocId();
}

int APS5_VABI sceKernelGetModuleInfo_nid_no_patch(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo(handle, info);
}

int APS5_VABI sceKernelGetModuleInfoForUnwind_nid_no_patch(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    return sceKernelGetModuleInfoForUnwind(addr, flags, info);
}

int APS5_VABI sceKernelGetModuleInfoFromAddr_nid_no_patch(std::uint64_t address, int flags, ModuleInfoEx* info) {
    return sceKernelGetModuleInfoFromAddr(address, flags, info);
}

int APS5_VABI sceKernelGetModuleList_nid_no_patch(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList(handles, count, actual);
}

int APS5_VABI sceKernelGetPageTableStats_nid_no_patch(int* cpuTotal, int* cpuAvailable, int* gpuTotal, int* gpuAvailable) {
    return sceKernelGetPageTableStats(cpuTotal, cpuAvailable, gpuTotal, gpuAvailable);
}

void* APS5_VABI sceKernelGetProcParam_nid_no_patch(void) {
    return sceKernelGetProcParam();
}

int APS5_VABI sceKernelGetProcessAbiType_nid_no_patch(int pid) {
    return sceKernelGetProcessAbiType(pid);
}

std::uint64_t APS5_VABI sceKernelGetProcessTime_nid_no_patch(void) {
    return sceKernelGetProcessTime();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter_nid_no_patch(void) {
    return sceKernelGetProcessTimeCounter();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency_nid_no_patch(void) {
    return sceKernelGetProcessTimeCounterFrequency();
}

int APS5_VABI sceKernelGetProsperoSystemSwVersion_nid_no_patch(SceKernelProsperoSystemSwVersion* version) {
    return sceKernelGetProsperoSystemSwVersion_nid_postfix(version);
}

int APS5_VABI sceKernelGetPrtAperture_nid_no_patch(int index, void** addr, size_t* len) {
    return sceKernelGetPrtAperture(index, addr, len);
}

int APS5_VABI sceKernelGetResidentFmemCount_nid_no_patch(int pid, uint64_t* resident, uint64_t* mapped) {
    return sceKernelGetResidentFmemCount(pid, resident, mapped);
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal_nid_no_patch(void) {
    return sceKernelGetSanitizerMallocReplaceExternal();
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal_nid_no_patch(void) {
    return sceKernelGetSanitizerNewReplaceExternal();
}

int APS5_VABI sceKernelGetSocPowerConsumption_nid_no_patch(void* consumption) {
    return sceKernelGetSocPowerConsumption(consumption);
}

int APS5_VABI sceKernelGetSocSensorTemperature_nid_no_patch(int sensorId, int* outCelsius) {
    return sceKernelGetSocSensorTemperature(sensorId, outCelsius);
}

int APS5_VABI sceKernelGetSystemSwVersion_nid_no_patch(SceKernelSystemSwVersion* version) {
    return sceKernelGetSystemSwVersion_nid_postfix(version);
}

int APS5_VABI sceKernelGetTraceMemoryStats_nid_no_patch(std::uint64_t* first, std::uint64_t* second) {
    return sceKernelGetTraceMemoryStats(first, second);
}

std::uint64_t APS5_VABI sceKernelGetTscFrequency_nid_no_patch(void) {
    return sceKernelGetTscFrequency();
}

int APS5_VABI sceKernelGetdents_nid_no_patch(int fd, char* buf, int nbytes) {
    return sceKernelGetdents(fd, buf, nbytes);
}

int APS5_VABI sceKernelGetdirentries_nid_no_patch(int fd, char* buf, int nbytes, int64_t* basep) {
    return sceKernelGetdirentries(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelGettimeofday_nid_no_patch(KernelTimeval* tp) {
    return sceKernelGettimeofday(tp);
}

int APS5_VABI sceKernelGettimezone_nid_no_patch(KernelTimezone* tz) {
    return sceKernelGettimezone(tz);
}

int APS5_VABI sceKernelInstallExceptionHandler_nid_no_patch(int signum, void* handler) {
    return sceKernelInstallExceptionHandler(signum, handler);
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo_nid_no_patch(void) {
    return sceKernelPrintBacktraceWithModuleInfo();
}

int APS5_VABI sceKernelInternalMemoryGetAvailableSize_nid_no_patch(std::size_t* size) {
    return sceKernelInternalMemoryGetAvailableSize(size);
}

std::int32_t APS5_VABI sceKernelInternalMemoryGetModuleSegmentInfo_nid_no_patch(InternalSegmentInfo* result) {
    return sceKernelInternalMemoryGetModuleSegmentInfo_nid_postfix(result);
}

int APS5_VABI sceKernelIsAddressSanitizerEnabled_nid_no_patch(void) {
    return sceKernelIsAddressSanitizerEnabled();
}

int APS5_VABI sceKernelIsThreadSanitizerEnabled_nid_no_patch(void) {
    return sceKernelIsThreadSanitizerEnabled();
}

int APS5_VABI sceKernelIsStack_nid_no_patch(void* addr, void** start, void** end) {
    return sceKernelIsStack(addr, start, end);
}

KernelModule APS5_VABI sceKernelLoadStartModule_nid_no_patch(const char* moduleFileName, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModule(moduleFileName, args, argp, flags, opt, res);
}

std::int64_t APS5_VABI sceKernelLseek_nid_no_patch(int d, std::int64_t offset, int whence) {
    return sceKernelLseek(d, offset, whence);
}

int APS5_VABI sceKernelMapDirectMemory_nid_no_patch(void** addr, size_t len, int prot, int flags, int64_t directMemoryStart, size_t alignment) {
    return sceKernelMapDirectMemory(addr, len, prot, flags, directMemoryStart, alignment);
}

int APS5_VABI sceKernelMapDirectMemory2_nid_no_patch(void** addr, size_t len, int type, int prot, int flags, int64_t directMemoryStart, size_t alignment) {
    return sceKernelMapDirectMemory2(addr, len, type, prot, flags, directMemoryStart, alignment);
}

int APS5_VABI sceKernelMapFlexibleMemory_nid_no_patch(void** addrInOut, size_t len, int prot, int flags) {
    return sceKernelMapFlexibleMemory(addrInOut, len, prot, flags);
}

int APS5_VABI sceKernelMapNamedDirectMemory_nid_no_patch(void** addr, size_t len, int prot, int flags, int64_t directMemoryStart, size_t alignment, const char* name) {
    return sceKernelMapNamedDirectMemory(addr, len, prot, flags, directMemoryStart, alignment, name);
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory_nid_no_patch(void** addrInOut, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedFlexibleMemory(addrInOut, len, prot, flags, name);
}

int32_t APS5_VABI sceKernelMapNamedSystemFlexibleMemory_nid_no_patch(void** addrInOut, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedSystemFlexibleMemory(addrInOut, len, prot, flags, name);
}

int APS5_VABI sceKernelMemoryPoolBatch_nid_no_patch(const KernelMemoryPoolBatchEntry* entries, int numEntries, int* numEntriesOut, int flags) {
    return sceKernelMemoryPoolBatch(entries, numEntries, numEntriesOut, flags);
}

int APS5_VABI sceKernelMemoryPoolCommit_nid_no_patch(void* addr, size_t len, int type, int prot, int flags) {
    return sceKernelMemoryPoolCommit(addr, len, type, prot, flags);
}

int APS5_VABI sceKernelMemoryPoolDecommit_nid_no_patch(void* addr, size_t len, int flags) {
    return sceKernelMemoryPoolDecommit(addr, len, flags);
}

int APS5_VABI sceKernelMemoryPoolExpand_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physAddrOut) {
    return sceKernelMemoryPoolExpand(searchStart, searchEnd, len, alignment, physAddrOut);
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats_nid_no_patch(KernelMemoryPoolBlockStats* output, size_t outputSize) {
    return sceKernelMemoryPoolGetBlockStats(output, outputSize);
}

int APS5_VABI sceKernelMemoryPoolReserve_nid_no_patch(void* addrIn, size_t len, size_t alignment, int flags, void** addrOut) {
    return sceKernelMemoryPoolReserve(addrIn, len, alignment, flags, addrOut);
}

int APS5_VABI sceKernelMkdir_nid_no_patch(const char* path, uint16_t mode) {
    return sceKernelMkdir(path, mode);
}

int APS5_VABI sceKernelMlock_nid_no_patch(void* address, std::uint64_t length) {
    return sceKernelMlock_nid_postfix(address, length);
}

int APS5_VABI sceKernelMmap_nid_no_patch(void* addr, size_t len, int prot, int flags, int fd, int64_t offset, void** res) {
    return sceKernelMmap(addr, len, prot, flags, fd, offset, res);
}

int APS5_VABI sceKernelMprotect_nid_no_patch(const void* addr, size_t len, int prot) {
    return sceKernelMprotect(addr, len, prot);
}

int APS5_VABI sceKernelMtypeprotect_nid_no_patch(const void* addr, size_t len, int type, int prot) {
    return sceKernelMtypeprotect(addr, len, type, prot);
}

int APS5_VABI sceKernelMunlock_nid_no_patch(void* address, std::uint64_t length) {
    return sceKernelMunlock_nid_postfix(address, length);
}

int APS5_VABI sceKernelMunlockall_nid_no_patch(void) {
    return sceKernelMunlockall_nid_postfix();
}

int APS5_VABI sceKernelMunmap_nid_no_patch(uint64_t vaddr, size_t len) {
    return sceKernelMunmap(vaddr, len);
}

int APS5_VABI sceKernelNanosleep_nid_no_patch(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep(rqtp, rmtp);
}

int APS5_VABI sceKernelOpen_nid_no_patch(const char* path, int flags, std::uint16_t mode) {
    return sceKernelOpen(path, flags, mode);
}

int APS5_VABI sceKernelOpenEventFlag_nid_no_patch(KernelEventFlag* ef, const char* name) {
    return sceKernelOpenEventFlag(ef, name);
}

int APS5_VABI sceKernelPollEventFlag_nid_no_patch(KernelEventFlag ef, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPat) {
    return sceKernelPollEventFlag(ef, bitPattern, waitMode, resultPat);
}

int APS5_VABI sceKernelPollSema_nid_no_patch(KernelSema sem, int need) {
    return sceKernelPollSema(sem, need);
}

int64_t APS5_VABI sceKernelPread_nid_no_patch(int d, void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPread(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPreadv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return sceKernelPreadv(d, iov, iovcnt, offset);
}

int APS5_VABI sceKernelProtectDirectMemoryForPID_nid_no_patch(int64_t offset, size_t len, int prot, int pid) {
    return sceKernelProtectDirectMemoryForPID(offset, len, prot, pid);
}

int64_t APS5_VABI sceKernelPwrite_nid_no_patch(int d, const void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPwrite(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPwritev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return sceKernelPwritev(d, iov, iovcnt, offset);
}

int APS5_VABI sceKernelQueryMemoryProtection_nid_no_patch(void* addr, void** start, void** end, int* prot) {
    return sceKernelQueryMemoryProtection(addr, start, end, prot);
}

int APS5_VABI sceKernelRaiseException_nid_no_patch(Pthread thread, int signum) {
    return sceKernelRaiseException(thread, signum);
}

int APS5_VABI sceKernelRdup_nid_no_patch(int pid, int fd) {
    return sceKernelRdup(pid, fd);
}

std::int64_t APS5_VABI sceKernelRead_nid_no_patch(int d, void* buf, std::size_t nbytes) {
    return sceKernelRead(d, buf, nbytes);
}

uint64_t APS5_VABI sceKernelReadTsc_nid_no_patch(void) {
    return sceKernelReadTsc();
}

int64_t APS5_VABI sceKernelReadv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return sceKernelReadv(d, iov, iovcnt);
}

int APS5_VABI sceKernelReleaseDirectMemory_nid_no_patch(int64_t start, size_t len) {
    return sceKernelReleaseDirectMemory(start, len);
}

int APS5_VABI sceKernelReleaseFlexibleMemory_nid_no_patch(void* addr, size_t len) {
    return sceKernelReleaseFlexibleMemory(addr, len);
}

int APS5_VABI sceKernelRemoveExceptionHandler_nid_no_patch(int signum) {
    return sceKernelRemoveExceptionHandler(signum);
}

int APS5_VABI sceKernelRename_nid_no_patch(const char* from, const char* to) {
    return sceKernelRename(from, to);
}

int APS5_VABI sceKernelReserveVirtualRange_nid_no_patch(void** addr, size_t len, int flags, size_t alignment) {
    return sceKernelReserveVirtualRange(addr, len, flags, alignment);
}

int APS5_VABI sceKernelRmdir_nid_no_patch(const char* path) {
    return sceKernelRmdir(path);
}

int APS5_VABI sceKernelSendNotificationRequest_nid_no_patch(int device, const void* request, std::size_t size, int flags) {
    return sceKernelSendNotificationRequest(device, request, size, flags);
}

int APS5_VABI sceKernelSetEventFlag_nid_no_patch(KernelEventFlag ef, std::uint64_t bitPattern) {
    return sceKernelSetEventFlag(ef, bitPattern);
}

void APS5_VABI sceKernelSetGPO_nid_no_patch(std::uint32_t bits) {
    sceKernelSetGPO(bits);
}

int APS5_VABI sceKernelSetPrtAperture_nid_no_patch(int index, void* addr, size_t len) {
    return sceKernelSetPrtAperture(index, addr, len);
}

int APS5_VABI sceKernelSetVirtualRangeName_nid_no_patch(const void* addr, uint64_t len, const char* name) {
    return sceKernelSetVirtualRangeName(addr, len, name);
}

int APS5_VABI sceKernelSignalSema_nid_no_patch(KernelSema sem, int count) {
    return sceKernelSignalSema(sem, count);
}

unsigned int APS5_VABI sceKernelSleep_nid_no_patch(unsigned int seconds) {
    return sceKernelSleep(seconds);
}

int APS5_VABI sceKernelStat_nid_no_patch(const char* path, FileStat* sb) {
    return sceKernelStat(path, sb);
}

int APS5_VABI sceKernelStopUnloadModule_nid_no_patch(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
    return sceKernelStopUnloadModule(handle, args, argp, flags, opt, res);
}

void APS5_VABI sceKernelSync_nid_no_patch(void) {
    sceKernelSync();
}

int APS5_VABI sceKernelTriggerUserEvent_nid_no_patch(KernelEqueue eq, int id, void* udata) {
    return sceKernelTriggerUserEvent(eq, id, udata);
}

int APS5_VABI sceKernelTruncate_nid_no_patch(const char* path, std::int64_t length) {
    return sceKernelTruncate_nid_postfix(path, length);
}

int APS5_VABI sceKernelUnlink_nid_no_patch(const char* path) {
    return sceKernelUnlink(path);
}

int APS5_VABI sceKernelUsleep_nid_no_patch(KernelUseconds microseconds) {
    return sceKernelUsleep_nid_postfix(microseconds);
}

int APS5_VABI sceKernelUtimes_nid_no_patch(const char* path, const KernelTimeval* times) {
    return sceKernelUtimes_nid_postfix(path, times);
}

int APS5_VABI sceKernelUuidCreate_nid_no_patch(std::uint32_t* uuid) {
    return sceKernelUuidCreate(uuid);
}

int APS5_VABI sceKernelVirtualQuery_nid_no_patch(const void* addr, int flags, VirtualQueryInfo* info, uint64_t infoSize) {
    return sceKernelVirtualQuery(addr, flags, info, infoSize);
}

int APS5_VABI sceKernelWaitEqueue_nid_no_patch(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    return sceKernelWaitEqueue(eq, ev, num, out, timo);
}

int APS5_VABI sceKernelWaitEventFlag_nid_no_patch(KernelEventFlag ef, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPat, KernelUseconds* timeout) {
    return sceKernelWaitEventFlag(ef, bitPattern, waitMode, resultPat, timeout);
}

int APS5_VABI sceKernelWaitSema_nid_no_patch(KernelSema sem, int need, KernelUseconds* time) {
    return sceKernelWaitSema(sem, need, time);
}

std::int64_t APS5_VABI sceKernelWrite_nid_no_patch(int d, const void* buf, std::size_t nbytes) {
    return sceKernelWrite(d, buf, nbytes);
}

int64_t APS5_VABI sceKernelWritev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return sceKernelWritev(d, iov, iovcnt);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return sceKernelWriteModifyMtypeProtectCommand(dst, address, size, type, protection, mask, outSize);
}

int APS5_VABI sceKernelWriteRemapIntoPrtCommand_nid_no_patch(std::uint32_t* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, int protection, int opcode, std::uint64_t* outSize) {
    return sceKernelWriteRemapIntoPrtCommand(dst, address, source, size, protection, opcode, outSize);
}

int APS5_VABI scePthreadAttrDestroy_nid_no_patch(PthreadAttr* attr) {
    return scePthreadAttrDestroy(attr);
}

int APS5_VABI scePthreadAttrGet_nid_no_patch(Pthread thread, PthreadAttr* attr) {
    return scePthreadAttrGet(thread, attr);
}

int APS5_VABI scePthreadAttrGetaffinity_nid_no_patch(const PthreadAttr* attr, KernelCpumask* mask) {
    return scePthreadAttrGetaffinity(attr, mask);
}

int APS5_VABI scePthreadAttrGetdetachstate_nid_no_patch(const PthreadAttr* attr, int* state) {
    return scePthreadAttrGetdetachstate(attr, state);
}

int APS5_VABI scePthreadAttrGetguardsize_nid_no_patch(const PthreadAttr* attr, size_t* guardSize) {
    return scePthreadAttrGetguardsize(attr, guardSize);
}

int APS5_VABI scePthreadAttrGetinheritsched_nid_no_patch(const PthreadAttr* attr, int* inheritSched) {
    return scePthreadAttrGetinheritsched(attr, inheritSched);
}

int APS5_VABI scePthreadAttrGetschedparam_nid_no_patch(const PthreadAttr* attr, KernelSchedParam* param) {
    return scePthreadAttrGetschedparam(attr, param);
}

int APS5_VABI scePthreadAttrGetschedpolicy_nid_no_patch(const PthreadAttr* attr, int* policy) {
    return scePthreadAttrGetschedpolicy(attr, policy);
}

int APS5_VABI scePthreadAttrGetsolosched_nid_no_patch(const PthreadAttr* attr, int* solosched) {
    return scePthreadAttrGetsolosched(attr, solosched);
}

int APS5_VABI scePthreadAttrGetstack_nid_no_patch(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    return scePthreadAttrGetstack(attr, stackaddr, stacksize);
}

int APS5_VABI scePthreadAttrGetstackaddr_nid_no_patch(const PthreadAttr* attr, void** stackAddr) {
    return scePthreadAttrGetstackaddr(attr, stackAddr);
}

int APS5_VABI scePthreadAttrGetstacksize_nid_no_patch(const PthreadAttr* attr, size_t* stackSize) {
    return scePthreadAttrGetstacksize(attr, stackSize);
}

int APS5_VABI scePthreadAttrInit_nid_no_patch(PthreadAttr* attr) {
    return scePthreadAttrInit(attr);
}

int APS5_VABI scePthreadAttrSetaffinity_nid_no_patch(PthreadAttr* attr, KernelCpumask mask) {
    return scePthreadAttrSetaffinity(attr, mask);
}

int APS5_VABI scePthreadAttrSetdetachstate_nid_no_patch(PthreadAttr* attr, int detachstate) {
    return scePthreadAttrSetdetachstate(attr, detachstate);
}

int APS5_VABI scePthreadAttrSetguardsize_nid_no_patch(PthreadAttr* attr, size_t guardSize) {
    return scePthreadAttrSetguardsize(attr, guardSize);
}

int APS5_VABI scePthreadAttrSetinheritsched_nid_no_patch(PthreadAttr* attr, int inheritSched) {
    return scePthreadAttrSetinheritsched(attr, inheritSched);
}

int APS5_VABI scePthreadAttrSetschedparam_nid_no_patch(PthreadAttr* attr, const KernelSchedParam* param) {
    return scePthreadAttrSetschedparam(attr, param);
}

int APS5_VABI scePthreadAttrSetschedpolicy_nid_no_patch(PthreadAttr* attr, int policy) {
    return scePthreadAttrSetschedpolicy(attr, policy);
}

int APS5_VABI scePthreadAttrSetstack_nid_no_patch(PthreadAttr* attr, void* addr, size_t size) {
    return scePthreadAttrSetstack(attr, addr, size);
}

int APS5_VABI scePthreadAttrSetstackaddr_nid_no_patch(PthreadAttr* attr, void* addr) {
    return scePthreadAttrSetstackaddr(attr, addr);
}

int APS5_VABI scePthreadAttrSetstacksize_nid_no_patch(PthreadAttr* attr, std::size_t stacksize) {
    return scePthreadAttrSetstacksize(attr, stacksize);
}

int APS5_VABI scePthreadAttrSetsolosched_nid_no_patch(PthreadAttr* attr, int solosched) {
    return scePthreadAttrSetsolosched(attr, solosched);
}

int APS5_VABI scePthreadCancel_nid_no_patch(Pthread thread) {
    return scePthreadCancel(thread);
}

int APS5_VABI scePthreadCondBroadcast_nid_no_patch(PthreadCond* cond) {
    return scePthreadCondBroadcast(cond);
}

int APS5_VABI scePthreadCondDestroy_nid_no_patch(PthreadCond* cond) {
    return scePthreadCondDestroy(cond);
}

int APS5_VABI scePthreadCondInit_nid_no_patch(PthreadCond* cond, const PthreadCondattr* attr, const char* name) {
    return scePthreadCondInit(cond, attr, name);
}

int APS5_VABI scePthreadCondSignal_nid_no_patch(PthreadCond* cond) {
    return scePthreadCondSignal(cond);
}

int APS5_VABI scePthreadCondSignalto_nid_no_patch(PthreadCond* cond, Pthread thread) {
    return scePthreadCondSignalto(cond, thread);
}

int APS5_VABI scePthreadCondTimedwait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return scePthreadCondTimedwait(cond, mutex, usec);
}

int APS5_VABI scePthreadCondWait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait(cond, mutex);
}

int APS5_VABI scePthreadCondattrDestroy_nid_no_patch(PthreadCondattr* attr) {
    return scePthreadCondattrDestroy(attr);
}

int APS5_VABI scePthreadCondattrGetpshared_nid_no_patch(const PthreadCondattr* attr, int* pshared) {
    return scePthreadCondattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadCondattrInit_nid_no_patch(PthreadCondattr* attr) {
    return scePthreadCondattrInit(attr);
}

int APS5_VABI scePthreadCondattrSetclock_nid_no_patch(PthreadCondattr* attr, KernelClockid clockId) {
    return scePthreadCondattrSetclock(attr, clockId);
}

int APS5_VABI scePthreadCondattrSetpshared_nid_no_patch(PthreadCondattr* attr, int pshared) {
    return scePthreadCondattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadCreate_nid_no_patch(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return scePthreadCreate(thread, attr, entry, arg, name);
}

int APS5_VABI scePthreadDetach_nid_no_patch(Pthread thread) {
    return scePthreadDetach(thread);
}

int APS5_VABI scePthreadEqual_nid_no_patch(Pthread thread1, Pthread thread2) {
    return scePthreadEqual(thread1, thread2);
}

void APS5_VABI scePthreadExit_nid_no_patch(void* retval) {
    scePthreadExit(retval);
}

int APS5_VABI scePthreadGetaffinity_nid_no_patch(Pthread thread, KernelCpumask* mask) {
    return scePthreadGetaffinity(thread, mask);
}

int APS5_VABI scePthreadGetname_nid_no_patch(Pthread thread, char* name) {
    return scePthreadGetname(thread, name);
}

int APS5_VABI scePthreadGetprio_nid_no_patch(Pthread thread, int* prio) {
    return scePthreadGetprio(thread, prio);
}

int APS5_VABI scePthreadGetschedparam_nid_no_patch(Pthread thread, int* policy, KernelSchedParam* param) {
    return scePthreadGetschedparam(thread, policy, param);
}

void* APS5_VABI scePthreadGetspecific_nid_no_patch(PthreadKey key) {
    return scePthreadGetspecific(key);
}

int APS5_VABI scePthreadGetthreadid_nid_no_patch(void) {
    return scePthreadGetthreadid();
}

int APS5_VABI scePthreadJoin_nid_no_patch(Pthread thread, void** retval) {
    return scePthreadJoin(thread, retval);
}

int APS5_VABI scePthreadKeyCreate_nid_no_patch(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return scePthreadKeyCreate(key, destructor);
}

int APS5_VABI scePthreadKeyDelete_nid_no_patch(PthreadKey key) {
    return scePthreadKeyDelete(key);
}

int APS5_VABI scePthreadMutexDestroy_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexDestroy(mutex);
}

int APS5_VABI scePthreadMutexGetprioceiling_nid_no_patch(PthreadMutex* mutex, int* prioceiling) {
    return scePthreadMutexGetprioceiling(mutex, prioceiling);
}

int APS5_VABI scePthreadMutexGetspinloops_nid_no_patch(PthreadMutex* mutex, int* count) {
    return scePthreadMutexGetspinloops(mutex, count);
}

int APS5_VABI scePthreadMutexGetyieldloops_nid_no_patch(PthreadMutex* mutex, int* count) {
    return scePthreadMutexGetyieldloops(mutex, count);
}

int APS5_VABI scePthreadMutexInit_nid_no_patch(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name) {
    return scePthreadMutexInit(mutex, attr, name);
}

int APS5_VABI scePthreadMutexLock_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexLock(mutex);
}

int APS5_VABI scePthreadMutexSetprioceiling_nid_no_patch(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return scePthreadMutexSetprioceiling(mutex, ceiling, oldCeiling);
}

int APS5_VABI scePthreadMutexSetspinloops_nid_no_patch(PthreadMutex* mutex, int count) {
    return scePthreadMutexSetspinloops(mutex, count);
}

int APS5_VABI scePthreadMutexSetyieldloops_nid_no_patch(PthreadMutex* mutex, int count) {
    return scePthreadMutexSetyieldloops(mutex, count);
}

int APS5_VABI scePthreadMutexTimedlock_nid_no_patch(PthreadMutex* mutex, KernelUseconds usec) {
    return scePthreadMutexTimedlock(mutex, usec);
}

int APS5_VABI scePthreadMutexTrylock_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexTrylock(mutex);
}

int APS5_VABI scePthreadMutexUnlock_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexUnlock(mutex);
}

int APS5_VABI scePthreadMutexattrDestroy_nid_no_patch(PthreadMutexattr* attr) {
    return scePthreadMutexattrDestroy(attr);
}

int APS5_VABI scePthreadMutexattrGetkind_nid_no_patch(PthreadMutexattr attr) {
    return scePthreadMutexattrGetkind(attr);
}

int APS5_VABI scePthreadMutexattrGetprioceiling_nid_no_patch(const PthreadMutexattr* attr, int* prioceiling) {
    return scePthreadMutexattrGetprioceiling(attr, prioceiling);
}

int APS5_VABI scePthreadMutexattrGetprotocol_nid_no_patch(const PthreadMutexattr* attr, int* protocol) {
    return scePthreadMutexattrGetprotocol(attr, protocol);
}

int APS5_VABI scePthreadMutexattrGetpshared_nid_no_patch(const PthreadMutexattr* attr, int* pshared) {
    return scePthreadMutexattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadMutexattrGettype_nid_no_patch(const PthreadMutexattr* attr, int* type) {
    return scePthreadMutexattrGettype(attr, type);
}

int APS5_VABI scePthreadMutexattrInit_nid_no_patch(PthreadMutexattr* attr) {
    return scePthreadMutexattrInit(attr);
}

int APS5_VABI scePthreadMutexattrSetkind_nid_no_patch(PthreadMutexattr* attr, int kind) {
    return scePthreadMutexattrSetkind(attr, kind);
}

int APS5_VABI scePthreadMutexattrSetprioceiling_nid_no_patch(PthreadMutexattr* attr, int prioceiling) {
    return scePthreadMutexattrSetprioceiling(attr, prioceiling);
}

int APS5_VABI scePthreadMutexattrSetprotocol_nid_no_patch(PthreadMutexattr* attr, int protocol) {
    return scePthreadMutexattrSetprotocol(attr, protocol);
}

int APS5_VABI scePthreadMutexattrSetpshared_nid_no_patch(PthreadMutexattr* attr, int pshared) {
    return scePthreadMutexattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadMutexattrSettype_nid_no_patch(PthreadMutexattr* attr, int type) {
    return scePthreadMutexattrSettype(attr, type);
}

int APS5_VABI scePthreadOnce_nid_no_patch(int32_t* once, void (APS5_VABI* init)(void)) {
    return scePthreadOnce(once, init);
}

int APS5_VABI scePthreadRename_nid_no_patch(Pthread thread, const char* name) {
    return scePthreadRename(thread, name);
}

int APS5_VABI scePthreadRwlockDestroy_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockDestroy(rwlock);
}

int APS5_VABI scePthreadRwlockInit_nid_no_patch(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
    return scePthreadRwlockInit(rwlock, attr, name);
}

int APS5_VABI scePthreadRwlockRdlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockRdlock(rwlock);
}

int APS5_VABI scePthreadRwlockTimedrdlock_nid_no_patch(PthreadRwlock* rwlock, KernelUseconds usec) {
    return scePthreadRwlockTimedrdlock(rwlock, usec);
}

int APS5_VABI scePthreadRwlockTimedwrlock_nid_no_patch(PthreadRwlock* rwlock, KernelUseconds usec) {
    return scePthreadRwlockTimedwrlock(rwlock, usec);
}

int APS5_VABI scePthreadRwlockTryrdlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockTryrdlock(rwlock);
}

int APS5_VABI scePthreadRwlockTrywrlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockTrywrlock(rwlock);
}

int APS5_VABI scePthreadRwlockUnlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockUnlock(rwlock);
}

int APS5_VABI scePthreadRwlockWrlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockWrlock(rwlock);
}

int APS5_VABI scePthreadRwlockattrDestroy_nid_no_patch(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrDestroy(attr);
}

int APS5_VABI scePthreadRwlockattrGetpshared_nid_no_patch(const PthreadRwlockattr* attr, int* pshared) {
    return scePthreadRwlockattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrInit_nid_no_patch(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrInit(attr);
}

int APS5_VABI scePthreadRwlockattrSetpshared_nid_no_patch(PthreadRwlockattr* attr, int pshared) {
    return scePthreadRwlockattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrGettype_nid_no_patch(const PthreadRwlockattr* attr, int* type) {
    return scePthreadRwlockattrGettype(attr, type);
}

int APS5_VABI scePthreadRwlockattrSettype_nid_no_patch(PthreadRwlockattr* attr, int type) {
    return scePthreadRwlockattrSettype(attr, type);
}

Pthread APS5_VABI scePthreadSelf_nid_no_patch(void) {
    return scePthreadSelf();
}

int APS5_VABI scePthreadSemInit_nid_no_patch(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    return scePthreadSemInit(sem, flag, value, name);
}

int APS5_VABI scePthreadSemDestroy_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemDestroy(sem);
}

int APS5_VABI scePthreadSemTimedwait_nid_no_patch(PthreadSem* sem, KernelUseconds usec) {
    return scePthreadSemTimedwait(sem, usec);
}

int APS5_VABI scePthreadSemGetvalue_nid_no_patch(PthreadSem* sem, int* value) {
    return scePthreadSemGetvalue(sem, value);
}

int APS5_VABI scePthreadSemPost_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemPost(sem);
}

int APS5_VABI scePthreadSemTrywait_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemTrywait(sem);
}

int APS5_VABI scePthreadSemWait_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemWait(sem);
}

int APS5_VABI scePthreadSetaffinity_nid_no_patch(Pthread thread, KernelCpumask mask) {
    return scePthreadSetaffinity(thread, mask);
}

int APS5_VABI scePthreadSetcancelstate_nid_no_patch(int state, int* oldState) {
    return scePthreadSetcancelstate(state, oldState);
}

int APS5_VABI scePthreadSetcanceltype_nid_no_patch(int type, int* oldType) {
    return scePthreadSetcanceltype(type, oldType);
}

int APS5_VABI scePthreadSetprio_nid_no_patch(Pthread thread, int prio) {
    return scePthreadSetprio(thread, prio);
}

int APS5_VABI scePthreadSetschedparam_nid_no_patch(Pthread thread, int policy, const KernelSchedParam* param) {
    return scePthreadSetschedparam(thread, policy, param);
}

int APS5_VABI scePthreadSetspecific_nid_no_patch(PthreadKey key, void* value) {
    return scePthreadSetspecific(key, value);
}

void APS5_VABI scePthreadTestcancel_nid_no_patch(void) {
    scePthreadTestcancel();
}

void APS5_VABI scePthreadYield_nid_no_patch(void) {
    scePthreadYield();
}

int APS5_VABI sched_get_priority_max_nid_no_patch(int policy) {
    return sched_get_priority_max_nid_postfix(policy);
}

int APS5_VABI sched_get_priority_min_nid_no_patch(int policy) {
    return sched_get_priority_min_nid_postfix(policy);
}

int APS5_VABI sched_getparam_nid_no_patch(int pid, KernelSchedParam* param) {
    return sched_getparam_nid_postfix(pid, param);
}

int APS5_VABI sched_getscheduler_nid_no_patch(int pid) {
    return sched_getscheduler_nid_postfix(pid);
}

int APS5_VABI sched_setparam_nid_no_patch(int pid, const KernelSchedParam* param) {
    return sched_setparam_nid_postfix(pid, param);
}

int APS5_VABI sched_yield_nid_no_patch(void) {
    return sched_yield_nid_postfix();
}

int APS5_VABI select_nid_no_patch(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    return select_nid_postfix(nfds, readfds, writefds, exceptfds, timeout);
}

int APS5_VABI sem_close_nid_no_patch(void* sem) {
    return sem_close_nid_postfix(sem);
}

int APS5_VABI sem_destroy_nid_no_patch(void* sem) {
    return sem_destroy_nid_postfix(sem);
}

int APS5_VABI sem_getvalue_nid_no_patch(void* sem, int* value) {
    return sem_getvalue_nid_postfix(sem, value);
}

int APS5_VABI sem_init_nid_no_patch(void* sem, int pshared, unsigned int value) {
    return sem_init_nid_postfix(sem, pshared, value);
}

void* APS5_VABI sem_open_nid_no_patch(const char* name, int flags, std::uint64_t v0, std::uint64_t v1) {
    return sem_open_nid_postfix(name, flags, v0, v1);
}

int APS5_VABI sem_post_nid_no_patch(void* sem) {
    return sem_post_nid_postfix(sem);
}

int APS5_VABI sem_reltimedwait_np_nid_no_patch(void* sem, uint32_t usec) {
    return sem_reltimedwait_np_nid_postfix(sem, usec);
}

int APS5_VABI sem_setname_nid_no_patch(void* sem, const char* name) {
    return sem_setname_nid_postfix(sem, name);
}

int APS5_VABI sem_timedwait_nid_no_patch(void* sem, const KernelTimespec* abstime) {
    return sem_timedwait_nid_postfix(sem, abstime);
}

int APS5_VABI sem_trywait_nid_no_patch(void* sem) {
    return sem_trywait_nid_postfix(sem);
}

int APS5_VABI sem_unlink_nid_no_patch(const char* name) {
    return sem_unlink_nid_postfix(name);
}

int APS5_VABI sem_wait_nid_no_patch(void* sem) {
    return sem_wait_nid_postfix(sem);
}

std::int64_t APS5_VABI send_nid_no_patch(int descriptor, const void* buffer, std::uint64_t length, int flags) {
    return send_nid_postfix(descriptor, buffer, length, flags);
}

int APS5_VABI sendfile_nid_no_patch(int file, int descriptor, std::int64_t offset, std::uint64_t length, const GuestSendfileHeaders* headers, std::int64_t* sbytes, int flags) {
    return sendfile_nid_postfix(file, descriptor, offset, length, headers, sbytes, flags);
}

std::int64_t APS5_VABI sendmsg_nid_no_patch(int descriptor, const GuestMsghdr* message, int flags) {
    return sendmsg_nid_postfix(descriptor, message, flags);
}

std::int64_t APS5_VABI sendto_nid_no_patch(int descriptor, const void* buffer, std::uint64_t length, int flags, const void* address, std::uint32_t addressLength) {
    return sendto_nid_postfix(descriptor, buffer, length, flags, address, addressLength);
}

int APS5_VABI setcontext_nid_no_patch(const UserUcontext* context) {
    return setcontext_nid_postfix(context);
}

int APS5_VABI setegid_nid_no_patch(std::uint32_t egid) {
    return setegid_nid_postfix(egid);
}

int APS5_VABI seteuid_nid_no_patch(std::uint32_t euid) {
    return seteuid_nid_postfix(euid);
}

int APS5_VABI setgroups_nid_no_patch(int count, const std::uint32_t* groups) {
    return setgroups_nid_postfix(count, groups);
}

int APS5_VABI setitimer_nid_no_patch(int which, const GuestItimerval* value, GuestItimerval* previous) {
    return setitimer_nid_postfix(which, value, previous);
}

int APS5_VABI setpriority_nid_no_patch(int which, int who, int niceval) {
    return setpriority_nid_postfix(which, who, niceval);
}

int APS5_VABI setregid_nid_no_patch(std::uint32_t rgid, std::uint32_t egid) {
    return setregid_nid_postfix(rgid, egid);
}

int APS5_VABI setreuid_nid_no_patch(std::uint32_t ruid, std::uint32_t euid) {
    return setreuid_nid_postfix(ruid, euid);
}

int APS5_VABI setrlimit_nid_no_patch(int resource, const GuestResourceLimit* limit) {
    return setrlimit_nid_postfix(resource, limit);
}

int APS5_VABI setsockopt_nid_no_patch(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return setsockopt_nid_postfix(descriptor, level, option, value, length);
}

int APS5_VABI setuid_nid_no_patch(std::uint32_t uid) {
    return setuid_nid_postfix(uid);
}

int APS5_VABI shm_open_nid_no_patch(const char* path, int flags, int mode) {
    return shm_open_nid_postfix(path, flags, mode);
}

int APS5_VABI shm_unlink_nid_no_patch(const char* path) {
    return shm_unlink_nid_postfix(path);
}

int APS5_VABI shutdown_nid_no_patch(int descriptor, int how) {
    return shutdown_nid_postfix(descriptor, how);
}

int APS5_VABI sigaction_nid_no_patch(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return sigaction_nid_postfix(guest, action, previous);
}

int APS5_VABI sigaddset_nid_no_patch(GuestSignalSet* set, int signal) {
    return sigaddset_nid_postfix(set, signal);
}

int APS5_VABI sigaltstack_nid_no_patch(const GuestStack* stack, GuestStack* previous) {
    return sigaltstack_nid_postfix(stack, previous);
}

int APS5_VABI sigdelset_nid_no_patch(GuestSignalSet* set, int signal) {
    return sigdelset_nid_postfix(set, signal);
}

int APS5_VABI sigemptyset_nid_no_patch(GuestSignalSet* set) {
    return sigemptyset_nid_postfix(set);
}

int APS5_VABI sigfillset_nid_no_patch(GuestSignalSet* set) {
    return sigfillset_nid_postfix(set);
}

int APS5_VABI sigismember_nid_no_patch(const GuestSignalSet* set, int signal) {
    return sigismember_nid_postfix(set, signal);
}

void APS5_VABI siglongjmp_nid_no_patch(std::uint64_t* buffer, int value) {
    siglongjmp_nid_postfix(buffer, value);
}

GuestHandler APS5_VABI signal_nid_no_patch(int guest, GuestHandler handler) {
    return signal_nid_postfix(guest, handler);
}

int APS5_VABI sigpending_nid_no_patch(GuestSignalSet* set) {
    return sigpending_nid_postfix(set);
}

int APS5_VABI sigprocmask_nid_no_patch(int how, const void* set, void* previousSet) {
    return sigprocmask_nid_postfix(how, set, previousSet);
}int APS5_VABI sigsuspend_nid_no_patch(const GuestSignalSet* set) {
    return sigsuspend_nid_postfix(set);
}

int APS5_VABI sigtimedwait_nid_no_patch(const GuestSignalSet* set, void* info, const KernelTimespec* timeout) {
    return sigtimedwait_nid_postfix(set, info, timeout);
}

int APS5_VABI sigwait_nid_no_patch(const GuestSignalSet* set, int* received) {
    return sigwait_nid_postfix(set, received);
}

int APS5_VABI sigwaitinfo_nid_no_patch(const GuestSignalSet* set, void* info) {
    return sigwaitinfo_nid_postfix(set, info);
}

unsigned int APS5_VABI sleep_nid_no_patch(unsigned int seconds) {
    return sleep_nid_postfix(seconds);
}

int APS5_VABI socket_nid_no_patch(int family, int type, int protocol) {
    return socket_nid_postfix(family, type, protocol);
}

int APS5_VABI socketpair_nid_no_patch(int family, int type, int protocol, int* descriptors) {
    return socketpair_nid_postfix(family, type, protocol, descriptors);
}

int APS5_VABI stat_nid_no_patch(const char* path, FileStat* sb) {
    return stat_nid_postfix(path, sb);
}void APS5_VABI sync_nid_no_patch(void) {
    sync_nid_postfix();
}

int APS5_VABI sysKernelGetIntdevModeForRcmgr_nid_no_patch(void) {
    return sysKernelGetIntdevModeForRcmgr_nid_postfix();
}

int APS5_VABI sysKernelGetIntdevModeForInternalLibc_nid_no_patch(void) {
    return sysKernelGetIntdevModeForInternalLibc_nid_postfix();
}

int APS5_VABI sysKernelGetLowerLimitUpdVersion_nid_no_patch(std::uint32_t* version) {
    return sysKernelGetLowerLimitUpdVersion_nid_postfix(version);
}

int APS5_VABI sysKernelGetLowerLimitSysexVersion_nid_no_patch(char* version) {
    return sysKernelGetLowerLimitSysexVersion_nid_postfix(version);
}

int APS5_VABI sysKernelGetManufacturingMode_nid_no_patch(void) {
    return sysKernelGetManufacturingMode_nid_postfix();
}

int APS5_VABI sysKernelGetUpdVersion_nid_no_patch(std::uint32_t* version) {
    return sysKernelGetUpdVersion_nid_postfix(version);
}

int APS5_VABI sceKernelGetSystemExVersion_nid_no_patch(char* first, char* second) {
    return sceKernelGetSystemExVersion(first, second);
}

int APS5_VABI sysarch_nid_no_patch(int operation, void* parameters) {
    return sysarch(operation, parameters);
}

std::int64_t APS5_VABI sysconf_nid_no_patch(int name) {
    return sysconf_nid_postfix(name);
}

int APS5_VABI sysctl_nid_no_patch(const int* name, std::uint32_t nameLength, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    return sysctl_nid_postfix(name, nameLength, oldValue, oldLength, newValue, newLength);
}

int APS5_VABI sysctlbyname_nid_no_patch(const char* name, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    return sysctlbyname_nid_postfix(name, oldValue, oldLength, newValue, newLength);
}

int APS5_VABI sysctlnametomib_nid_no_patch(const char* name, int* mib, std::size_t* size) {
    return sysctlnametomib_nid_postfix(name, mib, size);
}

int APS5_VABI system_nid_no_patch(const char* command) {
    return system_nid_postfix(command);
}

int APS5_VABI tcdrain_nid_no_patch(int descriptor) {
    return tcdrain_nid_postfix(descriptor);
}

int APS5_VABI tcflow_nid_no_patch(int descriptor, int action) {
    return tcflow_nid_postfix(descriptor, action);
}

int APS5_VABI tcflush_nid_no_patch(int descriptor, int queue) {
    return tcflush_nid_postfix(descriptor, queue);
}

int APS5_VABI tcgetattr_nid_no_patch(int descriptor, void* attributes) {
    return tcgetattr_nid_postfix(descriptor, attributes);
}

int APS5_VABI tcgetpgrp_nid_no_patch(int descriptor) {
    return tcgetpgrp_nid_postfix(descriptor);
}

int APS5_VABI tcgetsid_nid_no_patch(int descriptor) {
    return tcgetsid_nid_postfix(descriptor);
}

int APS5_VABI tcsendbreak_nid_no_patch(int descriptor, int duration) {
    return tcsendbreak_nid_postfix(descriptor, duration);
}

int APS5_VABI tcsetattr_nid_no_patch(int descriptor, int action, const void* attributes) {
    return tcsetattr_nid_postfix(descriptor, action, attributes);
}

int APS5_VABI tcsetpgrp_nid_no_patch(int descriptor, int group) {
    return tcsetpgrp_nid_postfix(descriptor, group);
}

int APS5_VABI truncate_nid_no_patch(const char* path, std::int64_t length) {
    return truncate_nid_postfix(path, length);
}

int APS5_VABI unlink_nid_no_patch(const char* path) {
    return unlink_nid_postfix(path);
}

int APS5_VABI usleep_nid_no_patch(KernelUseconds microseconds) {
    return usleep_nid_postfix(microseconds);
}

int APS5_VABI utimes_nid_no_patch(const char* path, const KernelTimeval* times) {
    return utimes_nid_postfix(path, times);
}

int APS5_VABI wait_nid_no_patch(int* status) {
    return wait_nid_postfix(status);
}

int APS5_VABI waitpid_nid_no_patch(int pid, int* status, int options) {
    return waitpid_nid_postfix(pid, status, options);
}

int64_t APS5_VABI write_nid_no_patch(int d, const char* str, int64_t size) {
    return write_nid_postfix(d, str, size);
}

int64_t APS5_VABI writev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return writev_nid_postfix(d, iov, iovcnt);
}

int APS5_VABI sceKernelAprResolveFilepathsToIds_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsToIds(paths, count, ids, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizes_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsToIdsAndFileSizes(paths, count, ids, sizes, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIds_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsWithPrefixToIds(prefix, paths, count, ids, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes(prefix, paths, count, ids, sizes, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsForEach_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return sceKernelAprResolveFilepathsToIdsForEach(paths, count, ids, results);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizesForEach_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    return sceKernelAprResolveFilepathsToIdsAndFileSizesForEach(paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsForEach_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsForEach(prefix, paths, count, ids, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach(prefix, paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprGetFileSize_nid_no_patch(uint32_t id, uint64_t* size) {
    return sceKernelAprGetFileSize(id, size);
}

int APS5_VABI sceKernelAprGetFileStat_nid_no_patch(uint32_t id, FileStat* stat) {
    return sceKernelAprGetFileStat(id, stat);
}

int APS5_VABI sceKernelAprSubmitCommandBuffer_nid_no_patch(const Apr::CommandBufferObject* buffer, uint32_t priority) {
    return sceKernelAprSubmitCommandBuffer(buffer, priority);
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetResult_nid_no_patch(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* result, uint32_t* id) {
    return sceKernelAprSubmitCommandBufferAndGetResult(buffer, priority, result, id);
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetId_nid_no_patch(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* id) {
    return sceKernelAprSubmitCommandBufferAndGetId(buffer, priority, id);
}

int APS5_VABI sceKernelAprWaitCommandBuffer_nid_no_patch(uint32_t id) {
    return sceKernelAprWaitCommandBuffer(id);
}

int APS5_VABI cpuset_nid_no_patch(int* setid) {
    return cpuset_nid_postfix(setid);
}

int APS5_VABI cpuset_setid_nid_no_patch(int which, std::int64_t id, int setid) {
    return cpuset_setid_nid_postfix(which, id, setid);
}

int APS5_VABI lutimes_nid_no_patch(const char* path, const KernelTimeval* times) {
    return lutimes_nid_postfix(path, times);
}

int APS5_VABI nmount_nid_no_patch(const KernelIovec* iov, unsigned int niov, int flags) {
    return nmount_nid_postfix(iov, niov, flags);
}

int APS5_VABI sceKernelJitCreateAliasOfSharedMemory_nid_no_patch(int fd, int maxProt, int* fdOut) {
    return sceKernelJitCreateAliasOfSharedMemory(fd, maxProt, fdOut);
}

int APS5_VABI sceKernelJitCreateSharedMemory_nid_no_patch(const char* name, std::size_t len, int maxProt, int* fdOut) {
    return sceKernelJitCreateSharedMemory(name, len, maxProt, fdOut);
}

int APS5_VABI sceKernelJitMapSharedMemory_nid_no_patch(int fd, int prot, void** startOut) {
    return sceKernelJitMapSharedMemory(fd, prot, startOut);
}

int APS5_VABI setpgid_nid_no_patch(int pid, int pgid) {
    return setpgid_nid_postfix(pid, pgid);
}

int APS5_VABI sceKernelGetOpenPsId_nid_no_patch(void* openPsIdOutput) {
    return sceKernelGetOpenPsId(openPsIdOutput);
}

int APS5_VABI sceCoredumpRegisterCoredumpHandler_nid_no_patch(uint64_t handler, size_t stackSize, uint64_t context) {
    return sceCoredumpRegisterCoredumpHandler(handler, stackSize, context);
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler_nid_no_patch(void) {
    return sceCoredumpUnregisterCoredumpHandler();
}

int64_t APS5_VABI sceCoredumpWriteUserData_nid_no_patch(const void* data, size_t size) {
    return sceCoredumpWriteUserData(data, size);
}

int APS5_VABI sceCoredumpAttachUserFile_nid_no_patch(std::uint32_t userValue, const char* path) {
    return sceCoredumpAttachUserFile(userValue, path);
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile_nid_no_patch(std::uint32_t userValue, const void* mem, std::int32_t size, const char* name) {
    return sceCoredumpAttachMemoryRegionAsUserFile(userValue, mem, size, name);
}

int APS5_VABI sceCoredumpAttachMemoryRegion_nid_no_patch(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachMemoryRegion(userValue, mem, size);
}

int APS5_VABI sceCoredumpAttachUserMemoryFile_nid_no_patch(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachUserMemoryFile(userValue, mem, size);
}

void APS5_VABI sceCoredumpDebugTextOut_nid_no_patch(const char* str, int len) {
    sceCoredumpDebugTextOut(str, len);
}

int APS5_VABI sceCoredumpGetStopInfoCpu_nid_no_patch(void* info, size_t size) {
    return sceCoredumpGetStopInfoCpu(info, size);
}

int APS5_VABI sceCoredumpGetStopInfoGpu_Agc_nid_no_patch(void* info, size_t size) {
    return sceCoredumpGetStopInfoGpu_Agc(info, size);
}

int APS5_VABI sceCoredumpGetThreadContextInfo_nid_no_patch(void* threadContextInfo, size_t threadContextInfoSize) {
    return sceCoredumpGetThreadContextInfo(threadContextInfo, threadContextInfoSize);
}

int APS5_VABI sceCoredumpConfigDumpMode_nid_no_patch(int mode) {
    return sceCoredumpConfigDumpMode(mode);
}

int APS5_VABI sceCoredumpSetUserDataType_nid_no_patch(std::int32_t type) {
    return sceCoredumpSetUserDataType(type);
}

int APS5_VABI sceKernelGetModuleInfo2_nid_no_patch(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo2(handle, info);
}

int APS5_VABI sceKernelGetModuleList2_nid_no_patch(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList2(handles, count, actual);
}



int64_t APS5_VABI pwrite_nid_no_patch(int d, const void* buf, size_t nbytes, int64_t offset) {
    return pwrite_nid_disambig1_nid_postfix(d, buf, nbytes, offset);
}
int64_t APS5_VABI fstat_nid_no_patch(int d, FileStat* sb) {
    return fstat_nid_disambig1_nid_postfix(d, sb);
}






#if defined(__ELF__)
__asm__(".protected __Ux86_64_setcontext");
__asm__(".protected __elf_phdr_match_addr_nid_postfix");
__asm__(".protected __inet_ntop_nid_postfix");
__asm__(".protected __inet_pton_nid_postfix");
__asm__(".protected __pthread_cleanup_pop_imp_nid_postfix");
__asm__(".protected __pthread_cleanup_push_imp_nid_postfix");
__asm__(".protected __pthread_cxa_finalize_nid_postfix");
__asm__(".protected __sys_socketex_nid_postfix");
__asm__(".protected __tls_get_addr_nid_postfix");
__asm__(".protected _accept_nid_postfix");
__asm__(".protected _bind_nid_postfix");
__asm__(".protected _close_nid_postfix");
__asm__(".protected _connect_nid_postfix");
__asm__(".protected _exit_nid_postfix");
__asm__(".protected _fcntl_nid_postfix");
__asm__(".protected _fstat_nid_postfix");
__asm__(".protected _fstatfs_nid_postfix");
__asm__(".protected _getdirentries_nid_postfix");
__asm__(".protected _getpeername_nid_postfix");
__asm__(".protected _getsockname_nid_postfix");
__asm__(".protected _getsockopt_nid_postfix");
__asm__(".protected _ioctl_nid_postfix");
__asm__(".protected _is_signal_return_nid_postfix");
__asm__(".protected _listen_nid_postfix");
__asm__(".protected _nanosleep_nid_postfix");
__asm__(".protected _open_nid_postfix");
__asm__(".protected _read_nid_postfix");
__asm__(".protected _readv_nid_postfix");
__asm__(".protected _recvfrom_nid_postfix");
__asm__(".protected _recvmsg_nid_postfix");
__asm__(".protected _sceKernelRtldSetApplicationHeapAPI_nid_postfix");
__asm__(".protected _sceKernelRtldThreadAtexitDecrement_nid_postfix");
__asm__(".protected _sceKernelRtldThreadAtexitIncrement_nid_postfix");
__asm__(".protected _sceKernelSetThreadAtexitCount_nid_postfix");
__asm__(".protected _sceKernelSetThreadAtexitReport_nid_postfix");
__asm__(".protected _sceKernelSetThreadDtors_nid_postfix");
__asm__(".protected _sendmsg_nid_postfix");
__asm__(".protected _sendto_nid_postfix");
__asm__(".protected _setsockopt_nid_postfix");
__asm__(".protected _sigaction_nid_postfix");
__asm__(".protected _sigprocmask_nid_postfix");
__asm__(".protected _sigsuspend_nid_postfix");
__asm__(".protected _umtx_op_nid_postfix");
__asm__(".protected _write_nid_postfix");
__asm__(".protected _writev_nid_postfix");
__asm__(".protected accept_nid_postfix");
__asm__(".protected bind_nid_postfix");
__asm__(".protected cfsetspeed_nid_postfix");
__asm__(".protected chflags_nid_postfix");
__asm__(".protected chmod_nid_postfix");
__asm__(".protected clock_getres_nid_postfix");
__asm__(".protected clock_gettime_nid_postfix");
__asm__(".protected clock_settime_nid_postfix");
__asm__(".protected close_nid_postfix");
__asm__(".protected connect_nid_postfix");
__asm__(".protected cpuset_getaffinity_nid_postfix");
__asm__(".protected cpuset_getid_nid_postfix");
__asm__(".protected cpuset_nid_postfix");
__asm__(".protected cpuset_setaffinity_nid_postfix");
__asm__(".protected cpuset_setid_nid_postfix");
__asm__(".protected creat_nid_postfix");
__asm__(".protected dlclose_nid_postfix");
__asm__(".protected dlerror_nid_postfix");
__asm__(".protected dlopen_nid_postfix");
__asm__(".protected dlsym_nid_postfix");
__asm__(".protected dup2_nid_postfix");
__asm__(".protected dup_nid_postfix");
__asm__(".protected execv_nid_postfix");
__asm__(".protected execve_nid_postfix");
__asm__(".protected execvp_nid_postfix");
__asm__(".protected fchflags_nid_postfix");
__asm__(".protected fchmod_nid_postfix");
__asm__(".protected fcntl_nid_postfix");
__asm__(".protected fdatasync_nid_postfix");
__asm__(".protected flock_nid_postfix");
__asm__(".protected fstat_nid_disambig1_nid_postfix");
__asm__(".protected fsync_nid_postfix");
__asm__(".protected ftruncate_nid_postfix");
__asm__(".protected futimes_nid_postfix");
__asm__(".protected getargc_nid_postfix");
__asm__(".protected getargv_nid_postfix");
__asm__(".protected getdents_nid_postfix");
__asm__(".protected getdirentries_nid_postfix");
__asm__(".protected getdtablesize_nid_postfix");
__asm__(".protected getegid_nid_postfix");
__asm__(".protected geteuid_nid_postfix");
__asm__(".protected getgid_nid_postfix");
__asm__(".protected getgroups_nid_postfix");
__asm__(".protected getitimer_nid_postfix");
__asm__(".protected getlogin_nid_postfix");
__asm__(".protected getlogin_r_nid_postfix");
__asm__(".protected getpagesize_nid_postfix");
__asm__(".protected getpeername_nid_postfix");
__asm__(".protected getpid_nid_postfix");
__asm__(".protected getppid_nid_postfix");
__asm__(".protected getpriority_nid_postfix");
__asm__(".protected getrlimit_nid_postfix");
__asm__(".protected getrusage_nid_postfix");
__asm__(".protected getsid_nid_postfix");
__asm__(".protected getsockname_nid_postfix");
__asm__(".protected getsockopt_nid_postfix");
__asm__(".protected gettimeofday_nid_postfix");
__asm__(".protected getuid_nid_postfix");
__asm__(".protected htonl_nid_postfix");
__asm__(".protected htons_nid_postfix");
__asm__(".protected inet_ntop_nid_postfix");
__asm__(".protected inet_pton_nid_postfix");
__asm__(".protected ioctl_nid_postfix");
__asm__(".protected issetugid_nid_postfix");
__asm__(".protected kevent_nid_postfix");
__asm__(".protected kill_nid_postfix");
__asm__(".protected kqueue_nid_postfix");
__asm__(".protected ksem_destroy_nid_postfix");
__asm__(".protected ksem_getvalue_nid_postfix");
__asm__(".protected ksem_init_nid_postfix");
__asm__(".protected ksem_post_nid_postfix");
__asm__(".protected ksem_timedwait_nid_postfix");
__asm__(".protected ksem_trywait_nid_postfix");
__asm__(".protected ksem_wait_nid_postfix");
__asm__(".protected ktimer_create_nid_postfix");
__asm__(".protected ktimer_delete_nid_postfix");
__asm__(".protected ktimer_getoverrun_nid_postfix");
__asm__(".protected ktimer_gettime_nid_postfix");
__asm__(".protected ktimer_settime_nid_postfix");
__asm__(".protected listen_nid_postfix");
__asm__(".protected lseek_nid_postfix");
__asm__(".protected lstat_nid_postfix");
__asm__(".protected lutimes_nid_postfix");
__asm__(".protected madvise_nid_postfix");
__asm__(".protected mincore_nid_postfix");
__asm__(".protected mkdir_nid_postfix");
__asm__(".protected mlock_nid_postfix");
__asm__(".protected mlockall_nid_postfix");
__asm__(".protected mmap_nid_postfix");
__asm__(".protected mprotect_nid_postfix");
__asm__(".protected msync_nid_postfix");
__asm__(".protected munlock_nid_postfix");
__asm__(".protected munlockall_nid_postfix");
__asm__(".protected munmap_nid_postfix");
__asm__(".protected nanosleep_nid_postfix");
__asm__(".protected nmount_nid_postfix");
__asm__(".protected ntohl_nid_postfix");
__asm__(".protected ntohs_nid_postfix");
__asm__(".protected open_nid_postfix");
__asm__(".protected pause_nid_postfix");
__asm__(".protected physhm_open_nid_postfix");
__asm__(".protected physhm_unlink_nid_postfix");
__asm__(".protected pipe_nid_postfix");
__asm__(".protected poll_nid_postfix");
__asm__(".protected pread_nid_postfix");
__asm__(".protected preadv_nid_postfix");
__asm__(".protected pselect_nid_postfix");
__asm__(".protected pthread_atfork_nid_postfix");
__asm__(".protected pthread_attr_destroy_nid_postfix");
__asm__(".protected pthread_attr_get_np_nid_postfix");
__asm__(".protected pthread_attr_getdetachstate_nid_postfix");
__asm__(".protected pthread_attr_getguardsize_nid_postfix");
__asm__(".protected pthread_attr_getinheritsched_nid_postfix");
__asm__(".protected pthread_attr_getschedparam_nid_postfix");
__asm__(".protected pthread_attr_getschedpolicy_nid_postfix");
__asm__(".protected pthread_attr_getscope_nid_postfix");
__asm__(".protected pthread_attr_getstack_nid_postfix");
__asm__(".protected pthread_attr_getstackaddr_nid_postfix");
__asm__(".protected pthread_attr_getstacksize_nid_postfix");
__asm__(".protected pthread_attr_init_nid_postfix");
__asm__(".protected pthread_attr_setdetachstate_nid_postfix");
__asm__(".protected pthread_attr_setguardsize_nid_postfix");
__asm__(".protected pthread_attr_setinheritsched_nid_postfix");
__asm__(".protected pthread_attr_setschedparam_nid_postfix");
__asm__(".protected pthread_attr_setschedpolicy_nid_postfix");
__asm__(".protected pthread_attr_setscope_nid_postfix");
__asm__(".protected pthread_attr_setsolosched_np_nid_postfix");
__asm__(".protected pthread_attr_setstack_nid_postfix");
__asm__(".protected pthread_attr_setstacksize_nid_postfix");
__asm__(".protected pthread_barrier_destroy_nid_postfix");
__asm__(".protected pthread_barrier_init_nid_postfix");
__asm__(".protected pthread_barrier_setname_np_nid_postfix");
__asm__(".protected pthread_barrier_wait_nid_postfix");
__asm__(".protected pthread_barrierattr_destroy_nid_postfix");
__asm__(".protected pthread_barrierattr_getpshared_nid_postfix");
__asm__(".protected pthread_barrierattr_init_nid_postfix");
__asm__(".protected pthread_barrierattr_setpshared_nid_postfix");
__asm__(".protected pthread_cancel_nid_postfix");
__asm__(".protected pthread_cleanup_pop_nid_postfix");
__asm__(".protected pthread_cleanup_push_nid_postfix");
__asm__(".protected pthread_cond_broadcast_nid_postfix");
__asm__(".protected pthread_cond_destroy_nid_postfix");
__asm__(".protected pthread_cond_init_nid_postfix");
__asm__(".protected pthread_cond_reltimedwait_np_nid_postfix");
__asm__(".protected pthread_cond_setname_np_nid_postfix");
__asm__(".protected pthread_cond_signal_nid_postfix");
__asm__(".protected pthread_cond_signalto_np_nid_postfix");
__asm__(".protected pthread_cond_timedwait_nid_postfix");
__asm__(".protected pthread_cond_wait_nid_postfix");
__asm__(".protected pthread_condattr_destroy_nid_postfix");
__asm__(".protected pthread_condattr_getclock_nid_postfix");
__asm__(".protected pthread_condattr_getpshared_nid_postfix");
__asm__(".protected pthread_condattr_init_nid_postfix");
__asm__(".protected pthread_condattr_setclock_nid_postfix");
__asm__(".protected pthread_condattr_setpshared_nid_postfix");
__asm__(".protected pthread_create_name_np_nid_postfix");
__asm__(".protected pthread_create_nid_postfix");
__asm__(".protected pthread_detach_nid_postfix");
__asm__(".protected pthread_equal_nid_postfix");
__asm__(".protected pthread_exit_nid_postfix");
__asm__(".protected pthread_getaffinity_np_nid_postfix");
__asm__(".protected pthread_getconcurrency_nid_postfix");
__asm__(".protected pthread_getcpuclockid_nid_postfix");
__asm__(".protected pthread_getname_np_nid_postfix");
__asm__(".protected pthread_getprio_nid_postfix");
__asm__(".protected pthread_getschedparam_nid_postfix");
__asm__(".protected pthread_getspecific_nid_postfix");
__asm__(".protected pthread_getthreadid_np_nid_postfix");
__asm__(".protected pthread_join_nid_postfix");
__asm__(".protected pthread_key_create_nid_postfix");
__asm__(".protected pthread_key_delete_nid_postfix");
__asm__(".protected pthread_kill_nid_postfix");
__asm__(".protected pthread_mutex_destroy_nid_postfix");
__asm__(".protected pthread_mutex_getprioceiling_nid_postfix");
__asm__(".protected pthread_mutex_getspinloops_np_nid_postfix");
__asm__(".protected pthread_mutex_getyieldloops_np_nid_postfix");
__asm__(".protected pthread_mutex_init_nid_postfix");
__asm__(".protected pthread_mutex_isowned_np_nid_postfix");
__asm__(".protected pthread_mutex_lock_nid_postfix");
__asm__(".protected pthread_mutex_reltimedlock_np_nid_postfix");
__asm__(".protected pthread_mutex_setname_np_nid_postfix");
__asm__(".protected pthread_mutex_setprioceiling_nid_postfix");
__asm__(".protected pthread_mutex_setspinloops_np_nid_postfix");
__asm__(".protected pthread_mutex_setyieldloops_np_nid_postfix");
__asm__(".protected pthread_mutex_timedlock_nid_postfix");
__asm__(".protected pthread_mutex_trylock_nid_postfix");
__asm__(".protected pthread_mutex_unlock_nid_postfix");
__asm__(".protected pthread_mutexattr_destroy_nid_postfix");
__asm__(".protected pthread_mutexattr_getkind_np_nid_postfix");
__asm__(".protected pthread_mutexattr_getprioceiling_nid_postfix");
__asm__(".protected pthread_mutexattr_getprotocol_nid_postfix");
__asm__(".protected pthread_mutexattr_getpshared_nid_postfix");
__asm__(".protected pthread_mutexattr_gettype_nid_postfix");
__asm__(".protected pthread_mutexattr_init_nid_postfix");
__asm__(".protected pthread_mutexattr_setkind_np_nid_postfix");
__asm__(".protected pthread_mutexattr_setprioceiling_nid_postfix");
__asm__(".protected pthread_mutexattr_setprotocol_nid_postfix");
__asm__(".protected pthread_mutexattr_setpshared_nid_postfix");
__asm__(".protected pthread_mutexattr_settype_nid_postfix");
__asm__(".protected pthread_once_nid_postfix");
__asm__(".protected pthread_rename_np_nid_postfix");
__asm__(".protected pthread_rwlock_destroy_nid_postfix");
__asm__(".protected pthread_rwlock_init_nid_postfix");
__asm__(".protected pthread_rwlock_rdlock_nid_postfix");
__asm__(".protected pthread_rwlock_reltimedrdlock_np_nid_postfix");
__asm__(".protected pthread_rwlock_reltimedwrlock_np_nid_postfix");
__asm__(".protected pthread_rwlock_setname_np_nid_postfix");
__asm__(".protected pthread_rwlock_timedrdlock_nid_postfix");
__asm__(".protected pthread_rwlock_timedwrlock_nid_postfix");
__asm__(".protected pthread_rwlock_tryrdlock_nid_postfix");
__asm__(".protected pthread_rwlock_trywrlock_nid_postfix");
__asm__(".protected pthread_rwlock_unlock_nid_postfix");
__asm__(".protected pthread_rwlock_wrlock_nid_postfix");
__asm__(".protected pthread_rwlockattr_destroy_nid_postfix");
__asm__(".protected pthread_rwlockattr_getpshared_nid_postfix");
__asm__(".protected pthread_rwlockattr_gettype_np_nid_postfix");
__asm__(".protected pthread_rwlockattr_init_nid_postfix");
__asm__(".protected pthread_rwlockattr_setpshared_nid_postfix");
__asm__(".protected pthread_rwlockattr_settype_np_nid_postfix");
__asm__(".protected pthread_self_nid_postfix");
__asm__(".protected pthread_set_name_np_nid_postfix");
__asm__(".protected pthread_setaffinity_np_nid_postfix");
__asm__(".protected pthread_setcancelstate_nid_postfix");
__asm__(".protected pthread_setcanceltype_nid_postfix");
__asm__(".protected pthread_setconcurrency_nid_postfix");
__asm__(".protected pthread_setprio_nid_postfix");
__asm__(".protected pthread_setschedparam_nid_postfix");
__asm__(".protected pthread_setspecific_nid_postfix");
__asm__(".protected pthread_sigmask_nid_postfix");
__asm__(".protected pthread_spin_destroy_nid_postfix");
__asm__(".protected pthread_spin_init_nid_postfix");
__asm__(".protected pthread_spin_lock_nid_postfix");
__asm__(".protected pthread_spin_trylock_nid_postfix");
__asm__(".protected pthread_spin_unlock_nid_postfix");
__asm__(".protected pthread_testcancel_nid_postfix");
__asm__(".protected pthread_timedjoin_np_nid_postfix");
__asm__(".protected pthread_yield_nid_postfix");
__asm__(".protected pwrite_nid_disambig1_nid_postfix");
__asm__(".protected pwritev_nid_postfix");
__asm__(".protected raise_nid_postfix");
__asm__(".protected read_nid_postfix");
__asm__(".protected readv_nid_postfix");
__asm__(".protected recv_nid_postfix");
__asm__(".protected recvfrom_nid_postfix");
__asm__(".protected recvmsg_nid_postfix");
__asm__(".protected rename_nid_postfix");
__asm__(".protected rfork_thread_nid_postfix");
__asm__(".protected rmdir_nid_postfix");
__asm__(".protected rtprio_thread_nid_postfix");
__asm__(".protected sceKernelChmod_nid_postfix");
__asm__(".protected sceKernelGetAllowedSdkVersionOnSystem_nid_postfix");
__asm__(".protected sceKernelGetLibkernelTextLocation_nid_postfix");
__asm__(".protected sceKernelGetProsperoSystemSwVersion_nid_postfix");
__asm__(".protected sceKernelGetSystemSwVersion_nid_postfix");
__asm__(".protected sceKernelInternalMemoryGetModuleSegmentInfo_nid_postfix");
__asm__(".protected sceKernelMlock_nid_postfix");
__asm__(".protected sceKernelMunlock_nid_postfix");
__asm__(".protected sceKernelMunlockall_nid_postfix");
__asm__(".protected sceKernelTruncate_nid_postfix");
__asm__(".protected sceKernelUsleep_nid_postfix");
__asm__(".protected sceKernelUtimes_nid_postfix");
__asm__(".protected sched_get_priority_max_nid_postfix");
__asm__(".protected sched_get_priority_min_nid_postfix");
__asm__(".protected sched_getparam_nid_postfix");
__asm__(".protected sched_getscheduler_nid_postfix");
__asm__(".protected sched_setparam_nid_postfix");
__asm__(".protected sched_yield_nid_postfix");
__asm__(".protected select_nid_postfix");
__asm__(".protected sem_close_nid_postfix");
__asm__(".protected sem_destroy_nid_postfix");
__asm__(".protected sem_getvalue_nid_postfix");
__asm__(".protected sem_init_nid_postfix");
__asm__(".protected sem_open_nid_postfix");
__asm__(".protected sem_post_nid_postfix");
__asm__(".protected sem_reltimedwait_np_nid_postfix");
__asm__(".protected sem_setname_nid_postfix");
__asm__(".protected sem_timedwait_nid_postfix");
__asm__(".protected sem_trywait_nid_postfix");
__asm__(".protected sem_unlink_nid_postfix");
__asm__(".protected sem_wait_nid_postfix");
__asm__(".protected send_nid_postfix");
__asm__(".protected sendfile_nid_postfix");
__asm__(".protected sendmsg_nid_postfix");
__asm__(".protected sendto_nid_postfix");
__asm__(".protected setcontext_nid_postfix");
__asm__(".protected setegid_nid_postfix");
__asm__(".protected seteuid_nid_postfix");
__asm__(".protected setgroups_nid_postfix");
__asm__(".protected setitimer_nid_postfix");
__asm__(".protected setpgid_nid_postfix");
__asm__(".protected setpriority_nid_postfix");
__asm__(".protected setregid_nid_postfix");
__asm__(".protected setreuid_nid_postfix");
__asm__(".protected setrlimit_nid_postfix");
__asm__(".protected setsockopt_nid_postfix");
__asm__(".protected setuid_nid_postfix");
__asm__(".protected shm_open_nid_postfix");
__asm__(".protected shm_unlink_nid_postfix");
__asm__(".protected shutdown_nid_postfix");
__asm__(".protected sigaction_nid_postfix");
__asm__(".protected sigaddset_nid_postfix");
__asm__(".protected sigaltstack_nid_postfix");
__asm__(".protected sigdelset_nid_postfix");
__asm__(".protected sigemptyset_nid_postfix");
__asm__(".protected sigfillset_nid_postfix");
__asm__(".protected sigismember_nid_postfix");
__asm__(".protected siglongjmp_nid_postfix");
__asm__(".protected signal_nid_postfix");
__asm__(".protected sigpending_nid_postfix");
__asm__(".protected sigprocmask_nid_postfix");
__asm__(".protected sigsuspend_nid_postfix");
__asm__(".protected sigtimedwait_nid_postfix");
__asm__(".protected sigwait_nid_postfix");
__asm__(".protected sigwaitinfo_nid_postfix");
__asm__(".protected sleep_nid_postfix");
__asm__(".protected socket_nid_postfix");
__asm__(".protected socketpair_nid_postfix");
__asm__(".protected stat_nid_postfix");
__asm__(".protected sync_nid_postfix");
__asm__(".protected sysKernelGetIntdevModeForInternalLibc_nid_postfix");
__asm__(".protected sysKernelGetIntdevModeForRcmgr_nid_postfix");
__asm__(".protected sysKernelGetLowerLimitSysexVersion_nid_postfix");
__asm__(".protected sysKernelGetLowerLimitUpdVersion_nid_postfix");
__asm__(".protected sysKernelGetManufacturingMode_nid_postfix");
__asm__(".protected sysKernelGetUpdVersion_nid_postfix");
__asm__(".protected sysarch");
__asm__(".protected sysconf_nid_postfix");
__asm__(".protected sysctl_nid_postfix");
__asm__(".protected sysctlbyname_nid_postfix");
__asm__(".protected sysctlnametomib_nid_postfix");
__asm__(".protected system_nid_postfix");
__asm__(".protected tcdrain_nid_postfix");
__asm__(".protected tcflow_nid_postfix");
__asm__(".protected tcflush_nid_postfix");
__asm__(".protected tcgetattr_nid_postfix");
__asm__(".protected tcgetpgrp_nid_postfix");
__asm__(".protected tcgetsid_nid_postfix");
__asm__(".protected tcsendbreak_nid_postfix");
__asm__(".protected tcsetattr_nid_postfix");
__asm__(".protected tcsetpgrp_nid_postfix");
__asm__(".protected truncate_nid_postfix");
__asm__(".protected unlink_nid_postfix");
__asm__(".protected usleep_nid_postfix");
__asm__(".protected utimes_nid_postfix");
__asm__(".protected wait_nid_postfix");
__asm__(".protected waitpid_nid_postfix");
__asm__(".protected write_nid_postfix");
__asm__(".protected writev_nid_postfix");
__asm__(".protected sceCoredumpAttachMemoryRegion");
__asm__(".protected sceCoredumpAttachMemoryRegionAsUserFile");
__asm__(".protected sceCoredumpAttachUserFile");
__asm__(".protected sceCoredumpAttachUserMemoryFile");
__asm__(".protected sceCoredumpConfigDumpMode");
__asm__(".protected sceCoredumpDebugTextOut");
__asm__(".protected sceCoredumpGetStopInfoCpu");
__asm__(".protected sceCoredumpGetStopInfoGpu_Agc");
__asm__(".protected sceCoredumpGetThreadContextInfo");
__asm__(".protected sceCoredumpRegisterCoredumpHandler");
__asm__(".protected sceCoredumpSetUserDataType");
__asm__(".protected sceCoredumpUnregisterCoredumpHandler");
__asm__(".protected sceCoredumpWriteUserData");
__asm__(".protected sceKernelAddAmprEvent");
__asm__(".protected sceKernelAddAmprSystemEvent");
__asm__(".protected sceKernelAddFileEvent");
__asm__(".protected sceKernelAddHRTimerEvent");
__asm__(".protected sceKernelAddReadEvent");
__asm__(".protected sceKernelAddTimerEvent");
__asm__(".protected sceKernelAddUserEvent");
__asm__(".protected sceKernelAddUserEventEdge");
__asm__(".protected sceKernelAddWriteEvent");
__asm__(".protected sceKernelAioCancelRequest");
__asm__(".protected sceKernelAioCancelRequests");
__asm__(".protected sceKernelAioDeleteRequest");
__asm__(".protected sceKernelAioDeleteRequests");
__asm__(".protected sceKernelAioInitializeImpl");
__asm__(".protected sceKernelAioInitializeParam");
__asm__(".protected sceKernelAioPollRequest");
__asm__(".protected sceKernelAioPollRequests");
__asm__(".protected sceKernelAioSetParam");
__asm__(".protected sceKernelAioSubmitReadCommands");
__asm__(".protected sceKernelAioSubmitReadCommandsMultiple");
__asm__(".protected sceKernelAioSubmitWriteCommands");
__asm__(".protected sceKernelAioSubmitWriteCommandsMultiple");
__asm__(".protected sceKernelAioWaitRequest");
__asm__(".protected sceKernelAioWaitRequests");
__asm__(".protected sceKernelAllocateDirectMemory");
__asm__(".protected sceKernelAllocateDirectMemoryForApp");
__asm__(".protected sceKernelAllocateMainDirectMemory");
__asm__(".protected sceKernelAvailableDirectMemorySize");
__asm__(".protected sceKernelAvailableFlexibleMemorySize");
__asm__(".protected sceKernelBatchMap");
__asm__(".protected sceKernelBatchMap2");
__asm__(".protected sceKernelCancelEventFlag");
__asm__(".protected sceKernelCancelSema");
__asm__(".protected sceKernelCheckReachability");
__asm__(".protected sceKernelCheckedReleaseDirectMemory");
__asm__(".protected sceKernelClearEventFlag");
__asm__(".protected sceKernelClockGetres");
__asm__(".protected sceKernelClockGettime");
__asm__(".protected sceKernelClose");
__asm__(".protected sceKernelCloseEventFlag");
__asm__(".protected sceKernelConfiguredFlexibleMemorySize");
__asm__(".protected sceKernelConvertLocaltimeToUtc");
__asm__(".protected sceKernelConvertUtcToLocaltime");
__asm__(".protected sceKernelCreateEqueue");
__asm__(".protected sceKernelCreateEventFlag");
__asm__(".protected sceKernelCreateSema");
__asm__(".protected sceKernelDebugOutText");
__asm__(".protected sceKernelDebugRaiseException");
__asm__(".protected sceKernelDebugRaiseExceptionOnReleaseMode");
__asm__(".protected sceKernelDebugWriteCppExceptionInfo");
__asm__(".protected sceKernelDeleteAmprEvent");
__asm__(".protected sceKernelDeleteAmprSystemEvent");
__asm__(".protected sceKernelDeleteEqueue");
__asm__(".protected sceKernelDeleteEventFlag");
__asm__(".protected sceKernelDeleteFileEvent");
__asm__(".protected sceKernelDeleteHRTimerEvent");
__asm__(".protected sceKernelDeleteReadEvent");
__asm__(".protected sceKernelDeleteSema");
__asm__(".protected sceKernelDeleteTimerEvent");
__asm__(".protected sceKernelDeleteUserEvent");
__asm__(".protected sceKernelDeleteWriteEvent");
__asm__(".protected sceKernelDirectMemoryQuery");
__asm__(".protected sceKernelDlsym");
__asm__(".protected sceKernelEnableDmemAliasing");
__asm__(".protected sceKernelError");
__asm__(".protected sceKernelFchmod");
__asm__(".protected sceKernelFcntl");
__asm__(".protected sceKernelFdatasync");
__asm__(".protected sceKernelFstat");
__asm__(".protected sceKernelFsync");
__asm__(".protected sceKernelFtruncate");
__asm__(".protected sceKernelFutimes");
__asm__(".protected sceKernelGetAppInfo");
__asm__(".protected sceKernelGetCpuFrequency");
__asm__(".protected sceKernelGetCpuTemperature");
__asm__(".protected sceKernelGetCpumode");
__asm__(".protected sceKernelGetCurrentCpu");
__asm__(".protected sceKernelGetDirectMemorySize");
__asm__(".protected sceKernelGetDirectMemoryType");
__asm__(".protected sceKernelGetEventData");
__asm__(".protected sceKernelGetEventError");
__asm__(".protected sceKernelGetEventFflags");
__asm__(".protected sceKernelGetEventFilter");
__asm__(".protected sceKernelGetEventId");
__asm__(".protected sceKernelGetEventUserData");
__asm__(".protected sceKernelGetExecutableModuleHandle");
__asm__(".protected sceKernelGetFsSandboxRandomWord");
__asm__(".protected sceKernelGetGPI");
__asm__(".protected sceKernelGetMainSocId");
__asm__(".protected sceKernelGetModuleInfo");
__asm__(".protected sceKernelGetModuleInfo2");
__asm__(".protected sceKernelGetModuleInfoForUnwind");
__asm__(".protected sceKernelGetModuleInfoFromAddr");
__asm__(".protected sceKernelGetModuleList");
__asm__(".protected sceKernelGetModuleList2");
__asm__(".protected sceKernelGetOpenPsId");
__asm__(".protected sceKernelGetPageTableStats");
__asm__(".protected sceKernelGetProcParam");
__asm__(".protected sceKernelGetProcessAbiType");
__asm__(".protected sceKernelGetProcessTime");
__asm__(".protected sceKernelGetProcessTimeCounter");
__asm__(".protected sceKernelGetProcessTimeCounterFrequency");
__asm__(".protected sceKernelGetPrtAperture");
__asm__(".protected sceKernelGetResidentFmemCount");
__asm__(".protected sceKernelGetSanitizerMallocReplaceExternal");
__asm__(".protected sceKernelGetSanitizerNewReplaceExternal");
__asm__(".protected sceKernelGetSocPowerConsumption");
__asm__(".protected sceKernelGetSocSensorTemperature");
__asm__(".protected sceKernelGetSystemExVersion");
__asm__(".protected sceKernelGetTraceMemoryStats");
__asm__(".protected sceKernelGetTscFrequency");
__asm__(".protected sceKernelGetdents");
__asm__(".protected sceKernelGetdirentries");
__asm__(".protected sceKernelGettimeofday");
__asm__(".protected sceKernelGettimezone");
__asm__(".protected sceKernelInstallExceptionHandler");
__asm__(".protected sceKernelInternalMemoryGetAvailableSize");
__asm__(".protected sceKernelIsAddressSanitizerEnabled");
__asm__(".protected sceKernelIsStack");
__asm__(".protected sceKernelIsThreadSanitizerEnabled");
__asm__(".protected sceKernelJitCreateAliasOfSharedMemory");
__asm__(".protected sceKernelJitCreateSharedMemory");
__asm__(".protected sceKernelJitMapSharedMemory");
__asm__(".protected sceKernelLoadStartModule");
__asm__(".protected sceKernelLseek");
__asm__(".protected sceKernelMapDirectMemory");
__asm__(".protected sceKernelMapDirectMemory2");
__asm__(".protected sceKernelMapFlexibleMemory");
__asm__(".protected sceKernelMapNamedDirectMemory");
__asm__(".protected sceKernelMapNamedFlexibleMemory");
__asm__(".protected sceKernelMapNamedSystemFlexibleMemory");
__asm__(".protected sceKernelMemoryPoolBatch");
__asm__(".protected sceKernelMemoryPoolCommit");
__asm__(".protected sceKernelMemoryPoolDecommit");
__asm__(".protected sceKernelMemoryPoolExpand");
__asm__(".protected sceKernelMemoryPoolGetBlockStats");
__asm__(".protected sceKernelMemoryPoolReserve");
__asm__(".protected sceKernelMkdir");
__asm__(".protected sceKernelMmap");
__asm__(".protected sceKernelMprotect");
__asm__(".protected sceKernelMtypeprotect");
__asm__(".protected sceKernelMunmap");
__asm__(".protected sceKernelNanosleep");
__asm__(".protected sceKernelOpen");
__asm__(".protected sceKernelOpenEventFlag");
__asm__(".protected sceKernelPollEventFlag");
__asm__(".protected sceKernelPollSema");
__asm__(".protected sceKernelPread");
__asm__(".protected sceKernelPreadv");
__asm__(".protected sceKernelPrintBacktraceWithModuleInfo");
__asm__(".protected sceKernelProtectDirectMemoryForPID");
__asm__(".protected sceKernelPwrite");
__asm__(".protected sceKernelPwritev");
__asm__(".protected sceKernelQueryMemoryProtection");
__asm__(".protected sceKernelRaiseException");
__asm__(".protected sceKernelRdup");
__asm__(".protected sceKernelRead");
__asm__(".protected sceKernelReadTsc");
__asm__(".protected sceKernelReadv");
__asm__(".protected sceKernelReleaseDirectMemory");
__asm__(".protected sceKernelReleaseFlexibleMemory");
__asm__(".protected sceKernelRemoveExceptionHandler");
__asm__(".protected sceKernelRename");
__asm__(".protected sceKernelReserveVirtualRange");
__asm__(".protected sceKernelRmdir");
__asm__(".protected sceKernelSendNotificationRequest");
__asm__(".protected sceKernelSetEventFlag");
__asm__(".protected sceKernelSetGPO");
__asm__(".protected sceKernelSetPrtAperture");
__asm__(".protected sceKernelSetVirtualRangeName");
__asm__(".protected sceKernelSignalSema");
__asm__(".protected sceKernelSleep");
__asm__(".protected sceKernelStat");
__asm__(".protected sceKernelStopUnloadModule");
__asm__(".protected sceKernelSync");
__asm__(".protected sceKernelTriggerUserEvent");
__asm__(".protected sceKernelUnlink");
__asm__(".protected sceKernelUuidCreate");
__asm__(".protected sceKernelVirtualQuery");
__asm__(".protected sceKernelWaitEqueue");
__asm__(".protected sceKernelWaitEventFlag");
__asm__(".protected sceKernelWaitSema");
__asm__(".protected sceKernelWrite");
__asm__(".protected sceKernelWriteModifyMtypeProtectCommand");
__asm__(".protected sceKernelWriteRemapIntoPrtCommand");
__asm__(".protected sceKernelWritev");
__asm__(".protected scePthreadAttrDestroy");
__asm__(".protected scePthreadAttrGet");
__asm__(".protected scePthreadAttrGetaffinity");
__asm__(".protected scePthreadAttrGetdetachstate");
__asm__(".protected scePthreadAttrGetguardsize");
__asm__(".protected scePthreadAttrGetinheritsched");
__asm__(".protected scePthreadAttrGetschedparam");
__asm__(".protected scePthreadAttrGetschedpolicy");
__asm__(".protected scePthreadAttrGetsolosched");
__asm__(".protected scePthreadAttrGetstack");
__asm__(".protected scePthreadAttrGetstackaddr");
__asm__(".protected scePthreadAttrGetstacksize");
__asm__(".protected scePthreadAttrInit");
__asm__(".protected scePthreadAttrSetaffinity");
__asm__(".protected scePthreadAttrSetdetachstate");
__asm__(".protected scePthreadAttrSetguardsize");
__asm__(".protected scePthreadAttrSetinheritsched");
__asm__(".protected scePthreadAttrSetschedparam");
__asm__(".protected scePthreadAttrSetschedpolicy");
__asm__(".protected scePthreadAttrSetsolosched");
__asm__(".protected scePthreadAttrSetstack");
__asm__(".protected scePthreadAttrSetstackaddr");
__asm__(".protected scePthreadAttrSetstacksize");
__asm__(".protected scePthreadCancel");
__asm__(".protected scePthreadCondBroadcast");
__asm__(".protected scePthreadCondDestroy");
__asm__(".protected scePthreadCondInit");
__asm__(".protected scePthreadCondSignal");
__asm__(".protected scePthreadCondSignalto");
__asm__(".protected scePthreadCondTimedwait");
__asm__(".protected scePthreadCondWait");
__asm__(".protected scePthreadCondattrDestroy");
__asm__(".protected scePthreadCondattrGetpshared");
__asm__(".protected scePthreadCondattrInit");
__asm__(".protected scePthreadCondattrSetclock");
__asm__(".protected scePthreadCondattrSetpshared");
__asm__(".protected scePthreadCreate");
__asm__(".protected scePthreadDetach");
__asm__(".protected scePthreadEqual");
__asm__(".protected scePthreadExit");
__asm__(".protected scePthreadGetaffinity");
__asm__(".protected scePthreadGetname");
__asm__(".protected scePthreadGetprio");
__asm__(".protected scePthreadGetschedparam");
__asm__(".protected scePthreadGetspecific");
__asm__(".protected scePthreadGetthreadid");
__asm__(".protected scePthreadJoin");
__asm__(".protected scePthreadKeyCreate");
__asm__(".protected scePthreadKeyDelete");
__asm__(".protected scePthreadMutexDestroy");
__asm__(".protected scePthreadMutexGetprioceiling");
__asm__(".protected scePthreadMutexGetspinloops");
__asm__(".protected scePthreadMutexGetyieldloops");
__asm__(".protected scePthreadMutexInit");
__asm__(".protected scePthreadMutexLock");
__asm__(".protected scePthreadMutexSetprioceiling");
__asm__(".protected scePthreadMutexSetspinloops");
__asm__(".protected scePthreadMutexSetyieldloops");
__asm__(".protected scePthreadMutexTimedlock");
__asm__(".protected scePthreadMutexTrylock");
__asm__(".protected scePthreadMutexUnlock");
__asm__(".protected scePthreadMutexattrDestroy");
__asm__(".protected scePthreadMutexattrGetkind");
__asm__(".protected scePthreadMutexattrGetprioceiling");
__asm__(".protected scePthreadMutexattrGetprotocol");
__asm__(".protected scePthreadMutexattrGetpshared");
__asm__(".protected scePthreadMutexattrGettype");
__asm__(".protected scePthreadMutexattrInit");
__asm__(".protected scePthreadMutexattrSetkind");
__asm__(".protected scePthreadMutexattrSetprioceiling");
__asm__(".protected scePthreadMutexattrSetprotocol");
__asm__(".protected scePthreadMutexattrSetpshared");
__asm__(".protected scePthreadMutexattrSettype");
__asm__(".protected scePthreadOnce");
__asm__(".protected scePthreadRename");
__asm__(".protected scePthreadRwlockDestroy");
__asm__(".protected scePthreadRwlockInit");
__asm__(".protected scePthreadRwlockRdlock");
__asm__(".protected scePthreadRwlockTimedrdlock");
__asm__(".protected scePthreadRwlockTimedwrlock");
__asm__(".protected scePthreadRwlockTryrdlock");
__asm__(".protected scePthreadRwlockTrywrlock");
__asm__(".protected scePthreadRwlockUnlock");
__asm__(".protected scePthreadRwlockWrlock");
__asm__(".protected scePthreadRwlockattrDestroy");
__asm__(".protected scePthreadRwlockattrGetpshared");
__asm__(".protected scePthreadRwlockattrGettype");
__asm__(".protected scePthreadRwlockattrInit");
__asm__(".protected scePthreadRwlockattrSetpshared");
__asm__(".protected scePthreadRwlockattrSettype");
__asm__(".protected scePthreadSelf");
__asm__(".protected scePthreadSemDestroy");
__asm__(".protected scePthreadSemGetvalue");
__asm__(".protected scePthreadSemInit");
__asm__(".protected scePthreadSemPost");
__asm__(".protected scePthreadSemTimedwait");
__asm__(".protected scePthreadSemTrywait");
__asm__(".protected scePthreadSemWait");
__asm__(".protected scePthreadSetaffinity");
__asm__(".protected scePthreadSetcancelstate");
__asm__(".protected scePthreadSetcanceltype");
__asm__(".protected scePthreadSetprio");
__asm__(".protected scePthreadSetschedparam");
__asm__(".protected scePthreadSetspecific");
__asm__(".protected scePthreadTestcancel");
__asm__(".protected scePthreadYield");
#endif

}
