#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/LibKernelSysProviders.hpp"

#include "prx/libkernel_sys/include/Forwards.hpp"



extern "C" {



int APS5_VABI __Ux86_64_setcontext_nid_disambig1_nid_postfix(void) {
    return __Ux86_64_setcontext_nid_no_patch();
}

int APS5_VABI __elf_phdr_match_addr_nid_disambig2_nid_postfix(dl_phdr_info* phdrInfo, void* addr) {
    return __elf_phdr_match_addr_nid_no_patch(phdrInfo, addr);
}

int* APS5_VABI __error_nid_disambig2_nid_postfix(void) {
    return __error_nid_no_patch();
}

const char* APS5_VABI __inet_ntop_nid_disambig2_nid_postfix(int family, const void* source, char* destination, std::uint32_t capacity) {
    return __inet_ntop_nid_no_patch(family, source, destination, capacity);
}

int APS5_VABI __inet_pton_nid_disambig2_nid_postfix(int family, const char* text, void* destination) {
    return __inet_pton_nid_no_patch(family, text, destination);
}

void APS5_VABI __pthread_cleanup_pop_imp_nid_disambig2_nid_postfix(int execute) {
    __pthread_cleanup_pop_imp_nid_no_patch(execute);
}

void APS5_VABI __pthread_cleanup_push_imp_nid_disambig2_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    __pthread_cleanup_push_imp_nid_no_patch(routine, argument, info);
}

void APS5_VABI __pthread_cxa_finalize_nid_disambig2_nid_postfix(void* argument) {
    __pthread_cxa_finalize_nid_no_patch(argument);
}

[[noreturn]] void APS5_VABI __stack_chk_fail_nid_disambig2_nid_postfix(void) {
    __stack_chk_fail_nid_no_patch();
}

int APS5_VABI __sys_socketex_nid_disambig2_nid_postfix(const char* name, int family, int type, int protocol) {
    return __sys_socketex_nid_no_patch(name, family, type, protocol);
}

void* APS5_VABI __tls_get_addr_nid_disambig2_nid_postfix(const KernelTlsIndex* index) {
    return __tls_get_addr_nid_no_patch(index);
}

int APS5_VABI _accept_nid_disambig2_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return _accept_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _bind_nid_disambig2_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return _bind_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _close_nid_disambig2_nid_postfix(int descriptor) {
    return _close_nid_no_patch(descriptor);
}

int APS5_VABI _connect_nid_disambig2_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return _connect_nid_no_patch(descriptor, address, length);
}

void APS5_VABI _exit_nid_disambig2_nid_postfix(int status) {
    _exit_nid_no_patch(status);
}

int APS5_VABI _fcntl_nid_disambig2_nid_postfix(int descriptor, int command, std::uint64_t v0, std::uint64_t v1) {
    return _fcntl_nid_no_patch(descriptor, command, v0, v1);
}

int64_t APS5_VABI _fstat_nid_disambig2_nid_postfix(int d, FileStat* sb) {
    return _fstat_nid_no_patch(d, sb);
}

int APS5_VABI _fstatfs_nid_disambig2_nid_postfix(int d, KernelStatfs* buf) {
    return _fstatfs_nid_no_patch(d, buf);
}

int APS5_VABI _getdirentries_nid_disambig2_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return _getdirentries_nid_no_patch(fd, buf, nbytes, basep);
}

int APS5_VABI _getpeername_nid_disambig2_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return _getpeername_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _getsockname_nid_disambig2_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return _getsockname_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _getsockopt_nid_disambig2_nid_postfix(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return _getsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI _ioctl_nid_disambig2_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    return _ioctl_nid_no_patch(descriptor, request, argument);
}

int APS5_VABI _is_signal_return_nid_disambig2_nid_postfix(std::uint64_t programCounter) {
    return _is_signal_return_nid_no_patch(programCounter);
}

int APS5_VABI _listen_nid_disambig2_nid_postfix(int descriptor, int backlog) {
    return _listen_nid_no_patch(descriptor, backlog);
}

int APS5_VABI _nanosleep_nid_disambig2_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return _nanosleep_nid_no_patch(rqtp, rmtp);
}

int APS5_VABI _open_nid_disambig2_nid_postfix(const char* path, int flags, std::uint64_t v0, std::uint64_t v1) {
    return _open_nid_no_patch(path, flags, v0, v1);
}

std::int64_t APS5_VABI _read_nid_disambig2_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    return _read_nid_no_patch(descriptor, buffer, count);
}

int64_t APS5_VABI _readv_nid_disambig2_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return _readv_nid_no_patch(d, iov, iovcnt);
}

std::int64_t APS5_VABI _recvfrom_nid_disambig2_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags, void* address, std::uint32_t* addressLength) {
    return _recvfrom_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

std::int64_t APS5_VABI _recvmsg_nid_disambig2_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    return _recvmsg_nid_no_patch(descriptor, message, flags);
}

void APS5_VABI _sceKernelRtldSetApplicationHeapAPI_nid_disambig2_nid_postfix(void* api[]) {
    _sceKernelRtldSetApplicationHeapAPI_nid_no_patch(api);
}

int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_disambig2_nid_postfix(const void* dsoSymbol) {
    return _sceKernelRtldThreadAtexitDecrement_nid_no_patch(dsoSymbol);
}

int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_disambig2_nid_postfix(const void* dsoSymbol) {
    return _sceKernelRtldThreadAtexitIncrement_nid_no_patch(dsoSymbol);
}

void APS5_VABI _sceKernelSetThreadDtors_nid_disambig2_nid_postfix(thread_dtors_func_t callback) {
    _sceKernelSetThreadDtors_nid_no_patch(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitCount_nid_disambig2_nid_postfix(get_thread_atexit_count_func_t callback) {
    _sceKernelSetThreadAtexitCount_nid_no_patch(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitReport_nid_disambig2_nid_postfix(thread_atexit_report_func_t callback) {
    _sceKernelSetThreadAtexitReport_nid_no_patch(callback);
}

std::int64_t APS5_VABI _sendmsg_nid_disambig2_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    return _sendmsg_nid_no_patch(descriptor, message, flags);
}

std::int64_t APS5_VABI _sendto_nid_disambig2_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags, const void* address, std::uint32_t addressLength) {
    return _sendto_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

int APS5_VABI _setsockopt_nid_disambig2_nid_postfix(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return _setsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI _sigaction_nid_disambig2_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return _sigaction_nid_no_patch(guest, action, previous);
}

int APS5_VABI _sigprocmask_nid_disambig2_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return _sigprocmask_nid_no_patch(how, set, previousSet);
}

int APS5_VABI _sigsuspend_nid_disambig2_nid_postfix(const GuestSignalSet* set) {
    return _sigsuspend_nid_no_patch(set);
}

int APS5_VABI _umtx_op_nid_disambig2_nid_postfix(void* object, int operation, std::uint64_t value, void* sizeArgument, void* timeArgument) {
    return _umtx_op_nid_no_patch(object, operation, value, sizeArgument, timeArgument);
}

std::int64_t APS5_VABI _write_nid_disambig2_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    return _write_nid_no_patch(descriptor, buffer, count);
}

int64_t APS5_VABI _writev_nid_disambig2_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return _writev_nid_no_patch(d, iov, iovcnt);
}

int APS5_VABI accept_nid_disambig2_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return accept_nid_no_patch(descriptor, address, length);
}

int APS5_VABI access_nid_disambig2_nid_postfix(const char* path, int mode) {
    return access_nid_no_patch(path, mode);
}

int APS5_VABI bind_nid_disambig2_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return bind_nid_no_patch(descriptor, address, length);
}

int APS5_VABI cfsetspeed_nid_disambig2_nid_postfix(void* attributes, std::uint32_t speed) {
    return cfsetspeed_nid_no_patch(attributes, speed);
}

int APS5_VABI chdir_nid_disambig2_nid_postfix(const char* path) {
    return chdir_nid_no_patch(path);
}

int APS5_VABI chflags_nid_disambig2_nid_postfix(const char* path, std::uint64_t flags) {
    return chflags_nid_no_patch(path, flags);
}

int APS5_VABI chmod_nid_disambig2_nid_postfix(const char* path, int mode) {
    return chmod_nid_no_patch(path, mode);
}

int APS5_VABI clock_getres_nid_disambig2_nid_postfix(int clockId, KernelTimespec* res) {
    return clock_getres_nid_no_patch(clockId, res);
}

int APS5_VABI clock_gettime_nid_disambig2_nid_postfix(int clockId, KernelTimespec* tp) {
    return clock_gettime_nid_no_patch(clockId, tp);
}

int APS5_VABI clock_settime_nid_disambig2_nid_postfix(int clockId, const KernelTimespec* tp) {
    return clock_settime_nid_no_patch(clockId, tp);
}

int APS5_VABI close_nid_disambig2_nid_postfix(int d) {
    return close_nid_no_patch(d);
}

int APS5_VABI connect_nid_disambig2_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return connect_nid_no_patch(descriptor, address, length);
}

int APS5_VABI cpuset_getaffinity_nid_disambig2_nid_postfix(int level, int which, std::int64_t id, std::size_t size, void* mask) {
    return cpuset_getaffinity_nid_no_patch(level, which, id, size, mask);
}

int APS5_VABI cpuset_getid_nid_disambig2_nid_postfix(int level, int which, std::int64_t id, int* setid) {
    return cpuset_getid_nid_no_patch(level, which, id, setid);
}

int APS5_VABI cpuset_setaffinity_nid_disambig2_nid_postfix(int level, int which, std::int64_t id, std::size_t size, const void* mask) {
    return cpuset_setaffinity_nid_no_patch(level, which, id, size, mask);
}

int APS5_VABI creat_nid_disambig2_nid_postfix(const char* path, int mode) {
    return creat_nid_no_patch(path, mode);
}

int APS5_VABI dlclose_nid_disambig2_nid_postfix(void* handle) {
    return dlclose_nid_no_patch(handle);
}

char* APS5_VABI dlerror_nid_disambig2_nid_postfix(void) {
    return dlerror_nid_no_patch();
}

void* APS5_VABI dlopen_nid_disambig2_nid_postfix(const char* path, int flags) {
    return dlopen_nid_no_patch(path, flags);
}

void* APS5_VABI dlsym_nid_disambig2_nid_postfix(void* handle, const char* name) {
    return dlsym_nid_no_patch(handle, name);
}

int APS5_VABI dup_nid_disambig2_nid_postfix(int d) {
    return dup_nid_no_patch(d);
}

int APS5_VABI dup2_nid_disambig2_nid_postfix(int from, int to) {
    return dup2_nid_no_patch(from, to);
}

int APS5_VABI execv_nid_disambig2_nid_postfix(const char* path, char* const* arguments) {
    return execv_nid_no_patch(path, arguments);
}

int APS5_VABI execve_nid_disambig2_nid_postfix(const char* path, char* const* arguments, char* const* environment) {
    return execve_nid_no_patch(path, arguments, environment);
}

int APS5_VABI execvp_nid_disambig2_nid_postfix(const char* file, char* const* arguments) {
    return execvp_nid_no_patch(file, arguments);
}

int APS5_VABI fchflags_nid_disambig2_nid_postfix(int d, std::uint64_t flags) {
    return fchflags_nid_no_patch(d, flags);
}

int APS5_VABI fchmod_nid_disambig2_nid_postfix(int d, int mode) {
    return fchmod_nid_no_patch(d, mode);
}

int APS5_VABI fcntl_nid_disambig2_nid_postfix(int descriptor, int command, std::uint64_t v0, std::uint64_t v1) {
    return fcntl_nid_no_patch(descriptor, command, v0, v1);
}

int APS5_VABI flock_nid_disambig2_nid_postfix(int d, int operation) {
    return flock_nid_no_patch(d, operation);
}

int APS5_VABI fsync_nid_disambig2_nid_postfix(int fd) {
    return fsync_nid_no_patch(fd);
}

int APS5_VABI fdatasync_nid_disambig2_nid_postfix(int fd) {
    return fdatasync_nid_no_patch(fd);
}

int APS5_VABI ftruncate_nid_disambig2_nid_postfix(int d, int64_t length) {
    return ftruncate_nid_no_patch(d, length);
}

int APS5_VABI futimes_nid_disambig2_nid_postfix(int d, const KernelTimeval* times) {
    return futimes_nid_no_patch(d, times);
}

int APS5_VABI getargc_nid_disambig2_nid_postfix(void) {
    return getargc_nid_no_patch();
}

const char** APS5_VABI getargv_nid_disambig2_nid_postfix(void) {
    return getargv_nid_no_patch();
}

int APS5_VABI getcontext_nid_disambig2_nid_postfix(UserUcontext* context) {
    return getcontext_nid_no_patch(context);
}

int APS5_VABI getdents_nid_disambig2_nid_postfix(int fd, char* buf, int nbytes) {
    return getdents_nid_no_patch(fd, buf, nbytes);
}

int APS5_VABI getdirentries_nid_disambig2_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return getdirentries_nid_no_patch(fd, buf, nbytes, basep);
}

int APS5_VABI getdtablesize_nid_disambig2_nid_postfix(void) {
    return getdtablesize_nid_no_patch();
}

int APS5_VABI getegid_nid_disambig2_nid_postfix(void) {
    return getegid_nid_no_patch();
}

int APS5_VABI geteuid_nid_disambig2_nid_postfix(void) {
    return geteuid_nid_no_patch();
}

int APS5_VABI getgid_nid_disambig2_nid_postfix(void) {
    return getgid_nid_no_patch();
}

int APS5_VABI getgroups_nid_disambig2_nid_postfix(int capacity, std::uint32_t* groups) {
    return getgroups_nid_no_patch(capacity, groups);
}

int APS5_VABI getitimer_nid_disambig2_nid_postfix(int which, GuestItimerval* output) {
    return getitimer_nid_no_patch(which, output);
}

char* APS5_VABI getlogin_nid_disambig2_nid_postfix(void) {
    return getlogin_nid_no_patch();
}

int APS5_VABI getlogin_r_nid_disambig2_nid_postfix(char* output, int size) {
    return getlogin_r_nid_no_patch(output, size);
}

int APS5_VABI getpagesize_nid_disambig2_nid_postfix(void) {
    return getpagesize_nid_no_patch();
}

int APS5_VABI getpeername_nid_disambig2_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return getpeername_nid_no_patch(descriptor, address, length);
}

int APS5_VABI getpid_nid_disambig2_nid_postfix(void) {
    return getpid_nid_no_patch();
}

int APS5_VABI getppid_nid_disambig2_nid_postfix(void) {
    return getppid_nid_no_patch();
}

int APS5_VABI getpriority_nid_disambig2_nid_postfix(int which, int who) {
    return getpriority_nid_no_patch(which, who);
}

int APS5_VABI getrlimit_nid_disambig2_nid_postfix(int resource, GuestResourceLimit* limit) {
    return getrlimit_nid_no_patch(resource, limit);
}

int APS5_VABI getrusage_nid_disambig2_nid_postfix(int who, GuestResourceUsage* usage) {
    return getrusage_nid_no_patch(who, usage);
}

int APS5_VABI getsid_nid_disambig2_nid_postfix(int pid) {
    return getsid_nid_no_patch(pid);
}

int APS5_VABI getsockname_nid_disambig2_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return getsockname_nid_no_patch(descriptor, address, length);
}

int APS5_VABI getsockopt_nid_disambig2_nid_postfix(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return getsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI gettimeofday_nid_disambig2_nid_postfix(KernelTimeval* tv, KernelTimezone* tz) {
    return gettimeofday_nid_no_patch(tv, tz);
}

int APS5_VABI getuid_nid_disambig2_nid_postfix(void) {
    return getuid_nid_no_patch();
}

std::uint32_t APS5_VABI htonl_nid_disambig2_nid_postfix(std::uint32_t value) {
    return htonl_nid_no_patch(value);
}

std::uint16_t APS5_VABI htons_nid_disambig2_nid_postfix(std::uint16_t value) {
    return htons_nid_no_patch(value);
}

const char* APS5_VABI inet_ntop_nid_disambig2_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
    return inet_ntop_nid_no_patch(af, src, dst, size);
}

int APS5_VABI inet_pton_nid_disambig2_nid_postfix(int af, const char* src, void* dst) {
    return inet_pton_nid_no_patch(af, src, dst);
}

int APS5_VABI ioctl_nid_disambig2_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    return ioctl_nid_no_patch(descriptor, request, argument);
}

int APS5_VABI issetugid_nid_disambig2_nid_postfix(void) {
    return issetugid_nid_no_patch();
}

int APS5_VABI kevent_nid_disambig2_nid_postfix(int descriptor, const KernelEvent* changelist, int nchanges, KernelEvent* eventlist, int nevents, const KernelTimespec* timeout) {
    return kevent_nid_no_patch(descriptor, changelist, nchanges, eventlist, nevents, timeout);
}

int APS5_VABI kill_nid_disambig2_nid_postfix(int pid, int guest) {
    return kill_nid_no_patch(pid, guest);
}

int APS5_VABI kqueue_nid_disambig2_nid_postfix(void) {
    return kqueue_nid_no_patch();
}

int APS5_VABI ksem_destroy_nid_disambig2_nid_postfix(std::intptr_t id) {
    return ksem_destroy_nid_no_patch(id);
}

int APS5_VABI ksem_getvalue_nid_disambig2_nid_postfix(std::intptr_t id, int* value) {
    return ksem_getvalue_nid_no_patch(id, value);
}

int APS5_VABI ksem_init_nid_disambig2_nid_postfix(std::intptr_t* idp, unsigned int value) {
    return ksem_init_nid_no_patch(idp, value);
}

int APS5_VABI ksem_post_nid_disambig2_nid_postfix(std::intptr_t id) {
    return ksem_post_nid_no_patch(id);
}

int APS5_VABI ksem_timedwait_nid_disambig2_nid_postfix(std::intptr_t id, const KernelTimespec* abstime) {
    return ksem_timedwait_nid_no_patch(id, abstime);
}

int APS5_VABI ksem_trywait_nid_disambig2_nid_postfix(std::intptr_t id) {
    return ksem_trywait_nid_no_patch(id);
}

int APS5_VABI ksem_wait_nid_disambig2_nid_postfix(std::intptr_t id) {
    return ksem_wait_nid_no_patch(id);
}

int APS5_VABI ktimer_create_nid_disambig2_nid_postfix(int clock, const GuestSigevent* event, int* timerid) {
    return ktimer_create_nid_no_patch(clock, event, timerid);
}

int APS5_VABI ktimer_delete_nid_disambig2_nid_postfix(int id) {
    return ktimer_delete_nid_no_patch(id);
}

int APS5_VABI ktimer_getoverrun_nid_disambig2_nid_postfix(int id) {
    return ktimer_getoverrun_nid_no_patch(id);
}

int APS5_VABI ktimer_gettime_nid_disambig2_nid_postfix(int id, GuestItimerspec* value) {
    return ktimer_gettime_nid_no_patch(id, value);
}

int APS5_VABI ktimer_settime_nid_disambig2_nid_postfix(int id, int flags, const GuestItimerspec* value, GuestItimerspec* oldValue) {
    return ktimer_settime_nid_no_patch(id, flags, value, oldValue);
}

int APS5_VABI listen_nid_disambig2_nid_postfix(int descriptor, int backlog) {
    return listen_nid_no_patch(descriptor, backlog);
}

int64_t APS5_VABI lseek_nid_disambig2_nid_postfix(int d, int64_t offset, int whence) {
    return lseek_nid_no_patch(d, offset, whence);
}

int APS5_VABI lstat_nid_disambig2_nid_postfix(const char* path, FileStat* sb) {
    return lstat_nid_no_patch(path, sb);
}

int APS5_VABI madvise_nid_disambig2_nid_postfix(void* address, std::size_t length, int advice) {
    return madvise_nid_no_patch(address, length, advice);
}

int APS5_VABI mincore_nid_disambig2_nid_postfix(const void* address, std::size_t length, char* vec) {
    return mincore_nid_no_patch(address, length, vec);
}

int APS5_VABI mkdir_nid_disambig2_nid_postfix(const char* path, uint16_t mode) {
    return mkdir_nid_no_patch(path, mode);
}

int APS5_VABI mlock_nid_disambig2_nid_postfix(const void* address, std::size_t length) {
    return mlock_nid_no_patch(address, length);
}

int APS5_VABI mlockall_nid_disambig2_nid_postfix(int flags) {
    return mlockall_nid_no_patch(flags);
}

void* APS5_VABI mmap_nid_disambig2_nid_postfix(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) {
    return mmap_nid_no_patch(address, length, protection, flags, descriptor, offset);
}

int APS5_VABI mprotect_nid_disambig2_nid_postfix(void* address, std::size_t length, int protection) {
    return mprotect_nid_no_patch(address, length, protection);
}

int APS5_VABI msync_nid_disambig2_nid_postfix(void* address, std::size_t length, int flags) {
    return msync_nid_no_patch(address, length, flags);
}

int APS5_VABI munlock_nid_disambig2_nid_postfix(const void* address, std::size_t length) {
    return munlock_nid_no_patch(address, length);
}

int APS5_VABI munlockall_nid_disambig2_nid_postfix(void) {
    return munlockall_nid_no_patch();
}

int APS5_VABI munmap_nid_disambig2_nid_postfix(void* address, std::size_t length) {
    return munmap_nid_no_patch(address, length);
}

int APS5_VABI nanosleep_nid_disambig2_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return nanosleep_nid_no_patch(rqtp, rmtp);
}

std::uint32_t APS5_VABI ntohl_nid_disambig2_nid_postfix(std::uint32_t value) {
    return ntohl_nid_no_patch(value);
}

std::uint16_t APS5_VABI ntohs_nid_disambig2_nid_postfix(std::uint16_t value) {
    return ntohs_nid_no_patch(value);
}

int APS5_VABI open_nid_disambig2_nid_postfix(const char* path, int flags, int mode) {
    return open_nid_no_patch(path, flags, mode);
}

int APS5_VABI pause_nid_disambig2_nid_postfix(void) {
    return pause_nid_no_patch();
}

int APS5_VABI physhm_open_nid_disambig2_nid_postfix(void) {
    return physhm_open_nid_no_patch();
}

int APS5_VABI physhm_unlink_nid_disambig2_nid_postfix(void) {
    return physhm_unlink_nid_no_patch();
}

int APS5_VABI pipe_nid_disambig2_nid_postfix(int* descriptors) {
    return pipe_nid_no_patch(descriptors);
}

int APS5_VABI poll_nid_disambig2_nid_postfix(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    return poll_nid_no_patch(descriptors, count, timeout);
}

int64_t APS5_VABI pread_nid_disambig2_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    return pread_nid_no_patch(d, buf, nbytes, offset);
}

int64_t APS5_VABI preadv_nid_disambig2_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return preadv_nid_no_patch(d, iov, iovcnt, offset);
}

int APS5_VABI pselect_nid_disambig2_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask) {
    return pselect_nid_no_patch(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

int APS5_VABI pthread_atfork_nid_disambig2_nid_postfix(PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child) {
    return pthread_atfork_nid_no_patch(prepare, parent, child);
}

int APS5_VABI pthread_attr_destroy_nid_disambig2_nid_postfix(PthreadAttr* attr) {
    return pthread_attr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_attr_get_np_nid_disambig2_nid_postfix(Pthread thread, PthreadAttr* attr) {
    return pthread_attr_get_np_nid_no_patch(thread, attr);
}

int APS5_VABI pthread_attr_getdetachstate_nid_disambig2_nid_postfix(const PthreadAttr* attr, int* state) {
    return pthread_attr_getdetachstate_nid_no_patch(attr, state);
}

int APS5_VABI pthread_attr_getguardsize_nid_disambig2_nid_postfix(const PthreadAttr* attr, size_t* guardSize) {
    return pthread_attr_getguardsize_nid_no_patch(attr, guardSize);
}

int APS5_VABI pthread_attr_getinheritsched_nid_disambig2_nid_postfix(const PthreadAttr* attr, int* inheritSched) {
    return pthread_attr_getinheritsched_nid_no_patch(attr, inheritSched);
}

int APS5_VABI pthread_attr_getschedparam_nid_disambig2_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
    return pthread_attr_getschedparam_nid_no_patch(attr, param);
}

int APS5_VABI pthread_attr_getschedpolicy_nid_disambig2_nid_postfix(const PthreadAttr* attr, int* policy) {
    return pthread_attr_getschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI pthread_attr_getscope_nid_disambig2_nid_postfix(const PthreadAttr* attr, int* scope) {
    return pthread_attr_getscope_nid_no_patch(attr, scope);
}

int APS5_VABI pthread_attr_getstack_nid_disambig2_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stackAddr, size_t* __restrict stackSize) {
    return pthread_attr_getstack_nid_no_patch(attr, stackAddr, stackSize);
}

int APS5_VABI pthread_attr_getstackaddr_nid_disambig2_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stackAddr) {
    return pthread_attr_getstackaddr_nid_no_patch(attr, stackAddr);
}

int APS5_VABI pthread_attr_getstacksize_nid_disambig2_nid_postfix(const PthreadAttr* attr, size_t* stackSize) {
    return pthread_attr_getstacksize_nid_no_patch(attr, stackSize);
}

int APS5_VABI pthread_attr_init_nid_disambig2_nid_postfix(PthreadAttr* attr) {
    return pthread_attr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_attr_setdetachstate_nid_disambig2_nid_postfix(PthreadAttr* attr, int state) {
    return pthread_attr_setdetachstate_nid_no_patch(attr, state);
}

int APS5_VABI pthread_attr_setguardsize_nid_disambig2_nid_postfix(PthreadAttr* attr, size_t guardSize) {
    return pthread_attr_setguardsize_nid_no_patch(attr, guardSize);
}

int APS5_VABI pthread_attr_setinheritsched_nid_disambig2_nid_postfix(PthreadAttr* attr, int inheritSched) {
    return pthread_attr_setinheritsched_nid_no_patch(attr, inheritSched);
}

int APS5_VABI pthread_attr_setschedparam_nid_disambig2_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    return pthread_attr_setschedparam_nid_no_patch(attr, param);
}

int APS5_VABI pthread_attr_setschedpolicy_nid_disambig2_nid_postfix(PthreadAttr* attr, int policy) {
    return pthread_attr_setschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI pthread_attr_setscope_nid_disambig2_nid_postfix(PthreadAttr* attr, int scope) {
    return pthread_attr_setscope_nid_no_patch(attr, scope);
}

int APS5_VABI pthread_attr_setsolosched_np_nid_disambig2_nid_postfix(PthreadAttr* attr, int solosched) {
    return pthread_attr_setsolosched_np_nid_no_patch(attr, solosched);
}

int APS5_VABI pthread_attr_setstack_nid_disambig2_nid_postfix(PthreadAttr* attr, void* stackAddr, size_t stackSize) {
    return pthread_attr_setstack_nid_no_patch(attr, stackAddr, stackSize);
}

int APS5_VABI pthread_attr_setstacksize_nid_disambig2_nid_postfix(PthreadAttr* attr, size_t stackSize) {
    return pthread_attr_setstacksize_nid_no_patch(attr, stackSize);
}

int APS5_VABI pthread_barrier_destroy_nid_disambig2_nid_postfix(PthreadBarrierPrivate** barrier) {
    return pthread_barrier_destroy_nid_no_patch(barrier);
}

int APS5_VABI pthread_barrier_init_nid_disambig2_nid_postfix(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count) {
    return pthread_barrier_init_nid_no_patch(barrier, attr, count);
}

int APS5_VABI pthread_barrier_setname_np_nid_disambig2_nid_postfix(PthreadBarrierPrivate** barrier, const char* name) {
    return pthread_barrier_setname_np_nid_no_patch(barrier, name);
}

int APS5_VABI pthread_barrier_wait_nid_disambig2_nid_postfix(PthreadBarrierPrivate** barrier) {
    return pthread_barrier_wait_nid_no_patch(barrier);
}

int APS5_VABI pthread_barrierattr_destroy_nid_disambig2_nid_postfix(PthreadBarrierattrPrivate** attr) {
    return pthread_barrierattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_barrierattr_getpshared_nid_disambig2_nid_postfix(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    return pthread_barrierattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_barrierattr_init_nid_disambig2_nid_postfix(PthreadBarrierattrPrivate** attr) {
    return pthread_barrierattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_barrierattr_setpshared_nid_disambig2_nid_postfix(PthreadBarrierattrPrivate** attr, int pshared) {
    return pthread_barrierattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_cancel_nid_disambig2_nid_postfix(Pthread thread) {
    return pthread_cancel_nid_no_patch(thread);
}

void APS5_VABI pthread_cleanup_pop_nid_disambig2_nid_postfix(int execute) {
    pthread_cleanup_pop_nid_no_patch(execute);
}

void APS5_VABI pthread_cleanup_push_nid_disambig2_nid_postfix(void (APS5_VABI* routine)(void*), void* argument) {
    pthread_cleanup_push_nid_no_patch(routine, argument);
}

int APS5_VABI pthread_cond_broadcast_nid_disambig2_nid_postfix(PthreadCond* cond) {
    return pthread_cond_broadcast_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_destroy_nid_disambig2_nid_postfix(PthreadCond* cond) {
    return pthread_cond_destroy_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_init_nid_disambig2_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    return pthread_cond_init_nid_no_patch(cond, attr);
}

int APS5_VABI pthread_cond_reltimedwait_np_nid_disambig2_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return pthread_cond_reltimedwait_np_nid_no_patch(cond, mutex, usec);
}

int APS5_VABI pthread_cond_setname_np_nid_disambig2_nid_postfix(PthreadCond* cond, const char* name) {
    return pthread_cond_setname_np_nid_no_patch(cond, name);
}

int APS5_VABI pthread_cond_signal_nid_disambig2_nid_postfix(PthreadCond* cond) {
    return pthread_cond_signal_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_signalto_np_nid_disambig2_nid_postfix(PthreadCond* cond, Pthread thread) {
    return pthread_cond_signalto_np_nid_no_patch(cond, thread);
}

int APS5_VABI pthread_cond_timedwait_nid_disambig2_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    return pthread_cond_timedwait_nid_no_patch(cond, mutex, abstime);
}

int APS5_VABI pthread_cond_wait_nid_disambig2_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    return pthread_cond_wait_nid_no_patch(cond, mutex);
}

int APS5_VABI pthread_condattr_destroy_nid_disambig2_nid_postfix(PthreadCondattr* attr) {
    return pthread_condattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_condattr_getclock_nid_disambig2_nid_postfix(const PthreadCondattr* attr, KernelClockid* clockId) {
    return pthread_condattr_getclock_nid_no_patch(attr, clockId);
}

int APS5_VABI pthread_condattr_getpshared_nid_disambig2_nid_postfix(const PthreadCondattr* attr, int* pshared) {
    return pthread_condattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_condattr_init_nid_disambig2_nid_postfix(PthreadCondattr* attr) {
    return pthread_condattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_condattr_setclock_nid_disambig2_nid_postfix(PthreadCondattr* attr, KernelClockid clockId) {
    return pthread_condattr_setclock_nid_no_patch(attr, clockId);
}

int APS5_VABI pthread_condattr_setpshared_nid_disambig2_nid_postfix(PthreadCondattr* attr, int pshared) {
    return pthread_condattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_create_nid_disambig2_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg) {
    return pthread_create_nid_no_patch(thread, attr, entry, arg);
}

int APS5_VABI pthread_create_name_np_nid_disambig2_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return pthread_create_name_np_nid_no_patch(thread, attr, entry, arg, name);
}

int APS5_VABI pthread_detach_nid_disambig2_nid_postfix(Pthread thread) {
    return pthread_detach_nid_no_patch(thread);
}

int APS5_VABI pthread_equal_nid_disambig2_nid_postfix(Pthread first, Pthread second) {
    return pthread_equal_nid_no_patch(first, second);
}

void APS5_VABI pthread_exit_nid_disambig2_nid_postfix(void* value) {
    pthread_exit_nid_no_patch(value);
}

int APS5_VABI pthread_getaffinity_np_nid_disambig2_nid_postfix(Pthread thread, std::size_t cpusetSize, void* cpuset) {
    return pthread_getaffinity_np_nid_no_patch(thread, cpusetSize, cpuset);
}

int APS5_VABI pthread_getconcurrency_nid_disambig2_nid_postfix(void) {
    return pthread_getconcurrency_nid_no_patch();
}

int APS5_VABI pthread_getcpuclockid_nid_disambig2_nid_postfix(Pthread thread, int* clockId) {
    return pthread_getcpuclockid_nid_no_patch(thread, clockId);
}

int APS5_VABI pthread_getname_np_nid_disambig2_nid_postfix(Pthread thread, char* name) {
    return pthread_getname_np_nid_no_patch(thread, name);
}

int APS5_VABI pthread_getprio_nid_disambig2_nid_postfix(Pthread thread) {
    return pthread_getprio_nid_no_patch(thread);
}

int APS5_VABI pthread_getschedparam_nid_disambig2_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    return pthread_getschedparam_nid_no_patch(thread, policy, param);
}

void* APS5_VABI pthread_getspecific_nid_disambig2_nid_postfix(PthreadKey key) {
    return pthread_getspecific_nid_no_patch(key);
}

int APS5_VABI pthread_getthreadid_np_nid_disambig2_nid_postfix(void) {
    return pthread_getthreadid_np_nid_no_patch();
}

int APS5_VABI pthread_join_nid_disambig2_nid_postfix(Pthread thread, void** value) {
    return pthread_join_nid_no_patch(thread, value);
}

int APS5_VABI pthread_key_create_nid_disambig2_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return pthread_key_create_nid_no_patch(key, destructor);
}

int APS5_VABI pthread_key_delete_nid_disambig2_nid_postfix(PthreadKey key) {
    return pthread_key_delete_nid_no_patch(key);
}

int APS5_VABI pthread_kill_nid_disambig2_nid_postfix(Pthread thread, int guest) {
    return pthread_kill_nid_no_patch(thread, guest);
}

int APS5_VABI pthread_mutex_destroy_nid_disambig2_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_destroy_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_getprioceiling_nid_disambig2_nid_postfix(const PthreadMutex* mutex, int* prioceiling) {
    return pthread_mutex_getprioceiling_nid_no_patch(mutex, prioceiling);
}

int APS5_VABI pthread_mutex_getspinloops_np_nid_disambig2_nid_postfix(PthreadMutex* mutex, int* count) {
    return pthread_mutex_getspinloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_getyieldloops_np_nid_disambig2_nid_postfix(PthreadMutex* mutex, int* count) {
    return pthread_mutex_getyieldloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_init_nid_disambig2_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return pthread_mutex_init_nid_no_patch(mutex, attr);
}

int APS5_VABI pthread_mutex_isowned_np_nid_disambig2_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_isowned_np_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_lock_nid_disambig2_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_lock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_reltimedlock_np_nid_disambig2_nid_postfix(PthreadMutex* mutex, KernelUseconds usec) {
    return pthread_mutex_reltimedlock_np_nid_no_patch(mutex, usec);
}

int APS5_VABI pthread_mutex_setname_np_nid_disambig2_nid_postfix(PthreadMutex* mutex, const char* name) {
    return pthread_mutex_setname_np_nid_no_patch(mutex, name);
}

int APS5_VABI pthread_mutex_setprioceiling_nid_disambig2_nid_postfix(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return pthread_mutex_setprioceiling_nid_no_patch(mutex, ceiling, oldCeiling);
}

int APS5_VABI pthread_mutex_setspinloops_np_nid_disambig2_nid_postfix(PthreadMutex* mutex, int count) {
    return pthread_mutex_setspinloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_setyieldloops_np_nid_disambig2_nid_postfix(PthreadMutex* mutex, int count) {
    return pthread_mutex_setyieldloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_timedlock_nid_disambig2_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
    return pthread_mutex_timedlock_nid_no_patch(mutex, abstime);
}

int APS5_VABI pthread_mutex_trylock_nid_disambig2_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_trylock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_unlock_nid_disambig2_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_unlock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutexattr_destroy_nid_disambig2_nid_postfix(PthreadMutexattr* attr) {
    return pthread_mutexattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_mutexattr_getkind_np_nid_disambig2_nid_postfix(PthreadMutexattr attr) {
    return pthread_mutexattr_getkind_np_nid_no_patch(attr);
}

int APS5_VABI pthread_mutexattr_getprioceiling_nid_disambig2_nid_postfix(const PthreadMutexattr* attr, int* prioceiling) {
    return pthread_mutexattr_getprioceiling_nid_no_patch(attr, prioceiling);
}

int APS5_VABI pthread_mutexattr_getprotocol_nid_disambig2_nid_postfix(const PthreadMutexattr* attr, int* protocol) {
    return pthread_mutexattr_getprotocol_nid_no_patch(attr, protocol);
}

int APS5_VABI pthread_mutexattr_getpshared_nid_disambig2_nid_postfix(const PthreadMutexattr* attr, int* pshared) {
    return pthread_mutexattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_mutexattr_gettype_nid_disambig2_nid_postfix(const PthreadMutexattr* attr, int* type) {
    return pthread_mutexattr_gettype_nid_no_patch(attr, type);
}

int APS5_VABI pthread_mutexattr_init_nid_disambig2_nid_postfix(PthreadMutexattr* attr) {
    return pthread_mutexattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_mutexattr_setkind_np_nid_disambig2_nid_postfix(PthreadMutexattr* attr, int kind) {
    return pthread_mutexattr_setkind_np_nid_no_patch(attr, kind);
}

int APS5_VABI pthread_mutexattr_setprioceiling_nid_disambig2_nid_postfix(PthreadMutexattr* attr, int prioceiling) {
    return pthread_mutexattr_setprioceiling_nid_no_patch(attr, prioceiling);
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_disambig2_nid_postfix(PthreadMutexattr* attr, int protocol) {
    return pthread_mutexattr_setprotocol_nid_no_patch(attr, protocol);
}

int APS5_VABI pthread_mutexattr_setpshared_nid_disambig2_nid_postfix(PthreadMutexattr* attr, int pshared) {
    return pthread_mutexattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_mutexattr_settype_nid_disambig2_nid_postfix(PthreadMutexattr* attr, int type) {
    return pthread_mutexattr_settype_nid_no_patch(attr, type);
}

int APS5_VABI pthread_once_nid_disambig2_nid_postfix(GuestOnce* control, void (APS5_VABI *initialize)()) {
    return pthread_once_nid_no_patch(control, initialize);
}

int APS5_VABI pthread_rename_np_nid_disambig2_nid_postfix(Pthread thread, const char* name) {
    return pthread_rename_np_nid_no_patch(thread, name);
}

int APS5_VABI pthread_rwlock_destroy_nid_disambig2_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_destroy_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_init_nid_disambig2_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    return pthread_rwlock_init_nid_no_patch(rwlock, attr);
}

int APS5_VABI pthread_rwlock_rdlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_rdlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_reltimedrdlock_np_nid_disambig2_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    return pthread_rwlock_reltimedrdlock_np_nid_no_patch(rwlock, usec);
}

int APS5_VABI pthread_rwlock_reltimedwrlock_np_nid_disambig2_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    return pthread_rwlock_reltimedwrlock_np_nid_no_patch(rwlock, usec);
}

int APS5_VABI pthread_rwlock_setname_np_nid_disambig2_nid_postfix(PthreadRwlock* rwlock, const char* name) {
    return pthread_rwlock_setname_np_nid_no_patch(rwlock, name);
}

int APS5_VABI pthread_rwlock_timedrdlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    return pthread_rwlock_timedrdlock_nid_no_patch(rwlock, abstime);
}

int APS5_VABI pthread_rwlock_timedwrlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    return pthread_rwlock_timedwrlock_nid_no_patch(rwlock, abstime);
}

int APS5_VABI pthread_rwlock_tryrdlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_tryrdlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_trywrlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_trywrlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_unlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_unlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_wrlock_nid_disambig2_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_wrlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlockattr_destroy_nid_disambig2_nid_postfix(PthreadRwlockattr* attr) {
    return pthread_rwlockattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_rwlockattr_getpshared_nid_disambig2_nid_postfix(const PthreadRwlockattr* attr, int* pshared) {
    return pthread_rwlockattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_rwlockattr_gettype_np_nid_disambig2_nid_postfix(const PthreadRwlockattr* attr, int* type) {
    return pthread_rwlockattr_gettype_np_nid_no_patch(attr, type);
}

int APS5_VABI pthread_rwlockattr_init_nid_disambig2_nid_postfix(PthreadRwlockattr* attr) {
    return pthread_rwlockattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_rwlockattr_setpshared_nid_disambig2_nid_postfix(PthreadRwlockattr* attr, int pshared) {
    return pthread_rwlockattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_rwlockattr_settype_np_nid_disambig2_nid_postfix(PthreadRwlockattr* attr, int type) {
    return pthread_rwlockattr_settype_np_nid_no_patch(attr, type);
}

Pthread APS5_VABI pthread_self_nid_disambig2_nid_postfix(void) {
    return pthread_self_nid_no_patch();
}

void APS5_VABI pthread_set_name_np_nid_disambig2_nid_postfix(Pthread thread, const char* name) {
    pthread_set_name_np_nid_no_patch(thread, name);
}

int APS5_VABI pthread_setaffinity_np_nid_disambig2_nid_postfix(Pthread thread, std::size_t cpusetSize, const void* cpuset) {
    return pthread_setaffinity_np_nid_no_patch(thread, cpusetSize, cpuset);
}

int APS5_VABI pthread_setcancelstate_nid_disambig2_nid_postfix(int state, int* oldState) {
    return pthread_setcancelstate_nid_no_patch(state, oldState);
}

int APS5_VABI pthread_setcanceltype_nid_disambig2_nid_postfix(int type, int* oldType) {
    return pthread_setcanceltype_nid_no_patch(type, oldType);
}

int APS5_VABI pthread_setconcurrency_nid_disambig2_nid_postfix(int newLevel) {
    return pthread_setconcurrency_nid_no_patch(newLevel);
}

int APS5_VABI pthread_setprio_nid_disambig2_nid_postfix(Pthread thread, int prio) {
    return pthread_setprio_nid_no_patch(thread, prio);
}

int APS5_VABI pthread_setschedparam_nid_disambig2_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    return pthread_setschedparam_nid_no_patch(thread, policy, param);
}

int APS5_VABI pthread_setspecific_nid_disambig2_nid_postfix(PthreadKey key, void* value) {
    return pthread_setspecific_nid_no_patch(key, value);
}

int APS5_VABI pthread_sigmask_nid_disambig2_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return pthread_sigmask_nid_no_patch(how, set, previousSet);
}

int APS5_VABI pthread_spin_destroy_nid_disambig2_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_destroy_nid_no_patch(lock);
}

int APS5_VABI pthread_spin_init_nid_disambig2_nid_postfix(PthreadSpinlockPrivate** lock, int pshared) {
    return pthread_spin_init_nid_no_patch(lock, pshared);
}

int APS5_VABI pthread_spin_lock_nid_disambig2_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_lock_nid_no_patch(lock);
}

int APS5_VABI pthread_spin_trylock_nid_disambig2_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_trylock_nid_no_patch(lock);
}

int APS5_VABI pthread_spin_unlock_nid_disambig2_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_unlock_nid_no_patch(lock);
}

void APS5_VABI pthread_testcancel_nid_disambig2_nid_postfix(void) {
    pthread_testcancel_nid_no_patch();
}

int APS5_VABI pthread_timedjoin_np_nid_disambig2_nid_postfix(Pthread thread, void** value, const KernelTimespec* abstime) {
    return pthread_timedjoin_np_nid_no_patch(thread, value, abstime);
}

void APS5_VABI pthread_yield_nid_disambig2_nid_postfix(void) {
    pthread_yield_nid_no_patch();
}

int64_t APS5_VABI pwritev_nid_disambig2_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return pwritev_nid_no_patch(d, iov, iovcnt, offset);
}

int APS5_VABI raise_nid_disambig2_nid_postfix(int guest) {
    return raise_nid_no_patch(guest);
}

int64_t APS5_VABI read_nid_disambig2_nid_postfix(int d, void* buf, uint64_t nbytes) {
    return read_nid_no_patch(d, buf, nbytes);
}

int64_t APS5_VABI readv_nid_disambig2_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return readv_nid_no_patch(d, iov, iovcnt);
}

std::int64_t APS5_VABI recv_nid_disambig2_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags) {
    return recv_nid_no_patch(descriptor, buffer, length, flags);
}

std::int64_t APS5_VABI recvfrom_nid_disambig2_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags, void* address, std::uint32_t* addressLength) {
    return recvfrom_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

std::int64_t APS5_VABI recvmsg_nid_disambig2_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    return recvmsg_nid_no_patch(descriptor, message, flags);
}

int APS5_VABI rename_nid_disambig2_nid_postfix(const char* from, const char* to) {
    return rename_nid_no_patch(from, to);
}

int APS5_VABI rfork_thread_nid_disambig2_nid_postfix(int flags, void* stack, int (APS5_VABI* function)(void*), void* argument) {
    return rfork_thread_nid_no_patch(flags, stack, function, argument);
}

int APS5_VABI rmdir_nid_disambig2_nid_postfix(const char* path) {
    return rmdir_nid_no_patch(path);
}

int APS5_VABI rtprio_thread_nid_disambig2_nid_postfix(int function, int lwpid, GuestRtPrio* priority) {
    return rtprio_thread_nid_no_patch(function, lwpid, priority);
}

int APS5_VABI sceKernelAddFileEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd, int watch, void* udata) {
    return sceKernelAddFileEvent_nid_no_patch(eq, fd, watch, udata);
}

int APS5_VABI sceKernelAddHRTimerEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
    return sceKernelAddHRTimerEvent_nid_no_patch(eq, id, ts, udata);
}

int APS5_VABI sceKernelAddReadEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    return sceKernelAddReadEvent_nid_no_patch(eq, fd, size, udata);
}

int APS5_VABI sceKernelAddTimerEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, KernelUseconds usec, void* udata) {
    return sceKernelAddTimerEvent_nid_no_patch(eq, id, usec, udata);
}

int APS5_VABI sceKernelAddUserEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelAddUserEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelAddUserEventEdge_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelAddUserEventEdge_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelAddWriteEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    return sceKernelAddWriteEvent_nid_no_patch(eq, fd, size, udata);
}

int APS5_VABI sceKernelAddAmprEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprEvent_nid_no_patch(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelAddAmprSystemEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprSystemEvent_nid_no_patch(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprSystemEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprSystemEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelAioCancelRequest_nid_disambig1_nid_postfix(int32_t id, int32_t* state) {
    return sceKernelAioCancelRequest_nid_no_patch(id, state);
}

int APS5_VABI sceKernelAioCancelRequests_nid_disambig1_nid_postfix(int32_t* id, int32_t num, int32_t* state) {
    return sceKernelAioCancelRequests_nid_no_patch(id, num, state);
}

int APS5_VABI sceKernelAioDeleteRequest_nid_disambig1_nid_postfix(int32_t id, int32_t* ret) {
    return sceKernelAioDeleteRequest_nid_no_patch(id, ret);
}

int APS5_VABI sceKernelAioDeleteRequests_nid_disambig1_nid_postfix(int32_t* id, int32_t num, int32_t* ret) {
    return sceKernelAioDeleteRequests_nid_no_patch(id, num, ret);
}

int APS5_VABI sceKernelAioInitializeImpl_nid_disambig1_nid_postfix(void* param, int32_t size) {
    return sceKernelAioInitializeImpl_nid_no_patch(param, size);
}

void APS5_VABI sceKernelAioInitializeParam_nid_disambig1_nid_postfix(void* param) {
    sceKernelAioInitializeParam_nid_no_patch(param);
}

int APS5_VABI sceKernelAioPollRequest_nid_disambig1_nid_postfix(int32_t id, int32_t* state) {
    return sceKernelAioPollRequest_nid_no_patch(id, state);
}

int APS5_VABI sceKernelAioPollRequests_nid_disambig1_nid_postfix(int32_t* id, int32_t num, int32_t* state) {
    return sceKernelAioPollRequests_nid_no_patch(id, num, state);
}

int APS5_VABI sceKernelAioSetParam_nid_disambig1_nid_postfix(KernelAioSchedulingParam* param, int32_t schedulingWindowSize, int32_t delayedCountLimit, uint32_t enableSplit, uint32_t splitSize, uint32_t splitChunkSize) {
    return sceKernelAioSetParam_nid_no_patch(param, schedulingWindowSize, delayedCountLimit, enableSplit, splitSize, splitChunkSize);
}

int APS5_VABI sceKernelAioSubmitReadCommands_nid_disambig1_nid_postfix(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitReadCommands_nid_no_patch(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitReadCommandsMultiple_nid_disambig1_nid_postfix(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitReadCommandsMultiple_nid_no_patch(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommands_nid_disambig1_nid_postfix(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitWriteCommands_nid_no_patch(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommandsMultiple_nid_disambig1_nid_postfix(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitWriteCommandsMultiple_nid_no_patch(req, size, prio, id);
}

int APS5_VABI sceKernelAioWaitRequest_nid_disambig1_nid_postfix(int32_t id, int32_t* state, uint32_t* usec) {
    return sceKernelAioWaitRequest_nid_no_patch(id, state, usec);
}

int APS5_VABI sceKernelAioWaitRequests_nid_disambig1_nid_postfix(int32_t* id, int32_t num, int32_t* state, uint32_t mode, uint32_t* usec) {
    return sceKernelAioWaitRequests_nid_no_patch(id, num, state, mode, usec);
}

int APS5_VABI sceKernelAllocateDirectMemory_nid_disambig1_nid_postfix(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateDirectMemory_nid_no_patch(searchStart, searchEnd, len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAllocateDirectMemoryForApp_nid_disambig1_nid_postfix(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateDirectMemoryForApp_nid_no_patch(searchStart, searchEnd, len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAllocateMainDirectMemory_nid_disambig1_nid_postfix(size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateMainDirectMemory_nid_no_patch(len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAvailableDirectMemorySize_nid_disambig1_nid_postfix(int64_t searchStart, int64_t searchEnd, size_t alignment, int64_t* physAddrOut, size_t* sizeOut) {
    return sceKernelAvailableDirectMemorySize_nid_no_patch(searchStart, searchEnd, alignment, physAddrOut, sizeOut);
}

int APS5_VABI sceKernelAvailableFlexibleMemorySize_nid_disambig1_nid_postfix(size_t* size) {
    return sceKernelAvailableFlexibleMemorySize_nid_no_patch(size);
}

int APS5_VABI sceKernelBatchMap_nid_disambig1_nid_postfix(KernelBatchMapEntry* entries, int numEntries, int* numEntriesOut) {
    return sceKernelBatchMap_nid_no_patch(entries, numEntries, numEntriesOut);
}

int APS5_VABI sceKernelBatchMap2_nid_disambig1_nid_postfix(KernelBatchMapEntry* entries, int numEntries, int* numEntriesOut, int flags) {
    return sceKernelBatchMap2_nid_no_patch(entries, numEntries, numEntriesOut, flags);
}

int APS5_VABI sceKernelCancelEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, std::uint64_t setPattern, int* numWaitThreads) {
    return sceKernelCancelEventFlag_nid_no_patch(ef, setPattern, numWaitThreads);
}

int APS5_VABI sceKernelCancelSema_nid_disambig1_nid_postfix(KernelSema sem, int count, int* threads) {
    return sceKernelCancelSema_nid_no_patch(sem, count, threads);
}

int APS5_VABI sceKernelCheckReachability_nid_disambig1_nid_postfix(const char* path) {
    return sceKernelCheckReachability_nid_no_patch(path);
}

int APS5_VABI sceKernelCheckedReleaseDirectMemory_nid_disambig1_nid_postfix(int64_t start, size_t len) {
    return sceKernelCheckedReleaseDirectMemory_nid_no_patch(start, len);
}

int APS5_VABI sceKernelChmod_nid_disambig2_nid_postfix(const char* path, std::uint16_t mode) {
    return sceKernelChmod_nid_no_patch(path, mode);
}

int APS5_VABI sceKernelClearEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bitPattern) {
    return sceKernelClearEventFlag_nid_no_patch(ef, bitPattern);
}

int APS5_VABI sceKernelClockGetres_nid_disambig1_nid_postfix(KernelClockid clockId, KernelTimespec* tp) {
    return sceKernelClockGetres_nid_no_patch(clockId, tp);
}

int APS5_VABI sceKernelClockGettime_nid_disambig1_nid_postfix(KernelClockid clockId, KernelTimespec* tp) {
    return sceKernelClockGettime_nid_no_patch(clockId, tp);
}

int APS5_VABI sceKernelClose_nid_disambig1_nid_postfix(int d) {
    return sceKernelClose_nid_no_patch(d);
}

int APS5_VABI sceKernelCloseEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef) {
    return sceKernelCloseEventFlag_nid_no_patch(ef);
}

int APS5_VABI sceKernelConfiguredFlexibleMemorySize_nid_disambig1_nid_postfix(size_t* size) {
    return sceKernelConfiguredFlexibleMemorySize_nid_no_patch(size);
}

int APS5_VABI sceKernelConvertLocaltimeToUtc_nid_disambig1_nid_postfix(int64_t localTime, int64_t reserved, int64_t* utcTime, KernelTimesec* timezone, int32_t* dstSeconds) {
    return sceKernelConvertLocaltimeToUtc_nid_no_patch(localTime, reserved, utcTime, timezone, dstSeconds);
}

int APS5_VABI sceKernelConvertUtcToLocaltime_nid_disambig1_nid_postfix(int64_t utcTime, int64_t* localTime, KernelTimesec* st, int32_t* dstSec) {
    return sceKernelConvertUtcToLocaltime_nid_no_patch(utcTime, localTime, st, dstSec);
}

int APS5_VABI sceKernelCreateEqueue_nid_disambig1_nid_postfix(KernelEqueue* eq, const char* name) {
    return sceKernelCreateEqueue_nid_no_patch(eq, name);
}

int APS5_VABI sceKernelCreateEventFlag_nid_disambig1_nid_postfix(KernelEventFlag* ef, const char* name, std::uint32_t attr, std::uint64_t initPattern, const void* param) {
    return sceKernelCreateEventFlag_nid_no_patch(ef, name, attr, initPattern, param);
}

int APS5_VABI sceKernelCreateSema_nid_disambig1_nid_postfix(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
    return sceKernelCreateSema_nid_no_patch(sem, name, attr, init, max, opt);
}

int APS5_VABI sceKernelDebugOutText_nid_disambig1_nid_postfix(int channel, const char* text) {
    return sceKernelDebugOutText_nid_no_patch(channel, text);
}

void APS5_VABI sceKernelDebugRaiseException_nid_disambig1_nid_postfix(int c1, int c2) {
    sceKernelDebugRaiseException_nid_no_patch(c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode_nid_disambig1_nid_postfix(int c1, int c2) {
    sceKernelDebugRaiseExceptionOnReleaseMode_nid_no_patch(c1, c2);
}

int APS5_VABI sceKernelDebugWriteCppExceptionInfo_nid_disambig1_nid_postfix(const void* exception, uint64_t unknown, const char* typeName, const char* what) {
    return sceKernelDebugWriteCppExceptionInfo_nid_no_patch(exception, unknown, typeName, what);
}

int APS5_VABI sceKernelDeleteEqueue_nid_disambig1_nid_postfix(KernelEqueue eq) {
    return sceKernelDeleteEqueue_nid_no_patch(eq);
}

int APS5_VABI sceKernelDeleteEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef) {
    return sceKernelDeleteEventFlag_nid_no_patch(ef);
}

int APS5_VABI sceKernelDeleteFileEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd) {
    return sceKernelDeleteFileEvent_nid_no_patch(eq, fd);
}

int APS5_VABI sceKernelDeleteHRTimerEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteHRTimerEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelDeleteReadEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd) {
    return sceKernelDeleteReadEvent_nid_no_patch(eq, fd);
}

int APS5_VABI sceKernelDeleteSema_nid_disambig1_nid_postfix(KernelSema sem) {
    return sceKernelDeleteSema_nid_no_patch(sem);
}

int APS5_VABI sceKernelDeleteTimerEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteTimerEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelDeleteUserEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteUserEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelDeleteWriteEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd) {
    return sceKernelDeleteWriteEvent_nid_no_patch(eq, fd);
}

int APS5_VABI sceKernelDirectMemoryQuery_nid_disambig1_nid_postfix(int64_t offset, int flags, void* info, size_t infoSize) {
    return sceKernelDirectMemoryQuery_nid_no_patch(offset, flags, info, infoSize);
}

int APS5_VABI sceKernelDlsym_nid_disambig1_nid_postfix(KernelModule handle, const char* symbol, void** addr) {
    return sceKernelDlsym_nid_no_patch(handle, symbol, addr);
}

int APS5_VABI sceKernelEnableDmemAliasing_nid_disambig1_nid_postfix(void) {
    return sceKernelEnableDmemAliasing_nid_no_patch();
}

std::int32_t APS5_VABI sceKernelError_nid_disambig1_nid_postfix(std::int32_t posixError) {
    return sceKernelError_nid_no_patch(posixError);
}

int APS5_VABI sceKernelFchmod_nid_disambig1_nid_postfix(int d, std::uint16_t mode) {
    return sceKernelFchmod_nid_no_patch(d, mode);
}

int APS5_VABI sceKernelFcntl_nid_disambig1_nid_postfix(int d, int command, std::uint64_t v0, std::uint64_t v1) {
    return sceKernelFcntl_nid_no_patch(d, command, v0, v1);
}

int APS5_VABI sceKernelFstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    return sceKernelFstat_nid_no_patch(d, sb);
}

int APS5_VABI sceKernelFsync_nid_disambig1_nid_postfix(int fd) {
    return sceKernelFsync_nid_no_patch(fd);
}

int APS5_VABI sceKernelFdatasync_nid_disambig1_nid_postfix(int fd) {
    return sceKernelFdatasync_nid_no_patch(fd);
}

int APS5_VABI sceKernelFtruncate_nid_disambig1_nid_postfix(int d, int64_t length) {
    return sceKernelFtruncate_nid_no_patch(d, length);
}

int APS5_VABI sceKernelFutimes_nid_disambig1_nid_postfix(int d, const KernelTimeval* times) {
    return sceKernelFutimes_nid_no_patch(d, times);
}

int APS5_VABI sceKernelGetAllowedSdkVersionOnSystem_nid_disambig2_nid_postfix(std::uint32_t* version) {
    return sceKernelGetAllowedSdkVersionOnSystem_nid_no_patch(version);
}

int APS5_VABI sceKernelGetAppInfo_nid_disambig1_nid_postfix(int pid, void* info) {
    return sceKernelGetAppInfo_nid_no_patch(pid, info);
}

std::int64_t APS5_VABI sceKernelGetCpuFrequency_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuFrequency_nid_no_patch();
}

int APS5_VABI sceKernelGetCpuTemperature_nid_disambig1_nid_postfix(int* outCelsius) {
    return sceKernelGetCpuTemperature_nid_no_patch(outCelsius);
}

int APS5_VABI sceKernelGetCpumode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpumode_nid_no_patch();
}

int APS5_VABI sceKernelGetCurrentCpu_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCurrentCpu_nid_no_patch();
}

size_t APS5_VABI sceKernelGetDirectMemorySize_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDirectMemorySize_nid_no_patch();
}

int APS5_VABI sceKernelGetDirectMemoryType_nid_disambig1_nid_postfix(int64_t offset, int* memoryType, int64_t* start, int64_t* end) {
    return sceKernelGetDirectMemoryType_nid_no_patch(offset, memoryType, start, end);
}

intptr_t APS5_VABI sceKernelGetEventData_nid_disambig1_nid_postfix(const KernelEvent* ev) {
    return sceKernelGetEventData_nid_no_patch(ev);
}

int APS5_VABI sceKernelGetEventError_nid_disambig1_nid_postfix(const KernelEvent* ev) {
    return sceKernelGetEventError_nid_no_patch(ev);
}

intptr_t APS5_VABI sceKernelGetEventFflags_nid_disambig1_nid_postfix(const KernelEvent* ev) {
    return sceKernelGetEventFflags_nid_no_patch(ev);
}

int APS5_VABI sceKernelGetEventFilter_nid_disambig1_nid_postfix(const KernelEvent* ev) {
    return sceKernelGetEventFilter_nid_no_patch(ev);
}

uintptr_t APS5_VABI sceKernelGetEventId_nid_disambig1_nid_postfix(const KernelEvent* ev) {
    return sceKernelGetEventId_nid_no_patch(ev);
}

void* APS5_VABI sceKernelGetEventUserData_nid_disambig1_nid_postfix(const KernelEvent* ev) {
    return sceKernelGetEventUserData_nid_no_patch(ev);
}

KernelModule APS5_VABI sceKernelGetExecutableModuleHandle_nid_disambig1_nid_postfix(void) {
    return sceKernelGetExecutableModuleHandle_nid_no_patch();
}

const char* APS5_VABI sceKernelGetFsSandboxRandomWord_nid_disambig1_nid_postfix(void) {
    return sceKernelGetFsSandboxRandomWord_nid_no_patch();
}

std::uint64_t APS5_VABI sceKernelGetGPI_nid_disambig1_nid_postfix(void) {
    return sceKernelGetGPI_nid_no_patch();
}

std::int32_t APS5_VABI sceKernelGetLibkernelTextLocation_nid_disambig2_nid_postfix(void** address, std::size_t* size) {
    return sceKernelGetLibkernelTextLocation_nid_no_patch(address, size);
}

int APS5_VABI sceKernelGetMainSocId_nid_disambig1_nid_postfix(void) {
    return sceKernelGetMainSocId_nid_no_patch();
}

int APS5_VABI sceKernelGetModuleInfo_nid_disambig1_nid_postfix(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo_nid_no_patch(handle, info);
}

int APS5_VABI sceKernelGetModuleInfoForUnwind_nid_disambig1_nid_postfix(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    return sceKernelGetModuleInfoForUnwind_nid_no_patch(addr, flags, info);
}

int APS5_VABI sceKernelGetModuleInfoFromAddr_nid_disambig1_nid_postfix(std::uint64_t address, int flags, ModuleInfoEx* info) {
    return sceKernelGetModuleInfoFromAddr_nid_no_patch(address, flags, info);
}

int APS5_VABI sceKernelGetModuleList_nid_disambig1_nid_postfix(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList_nid_no_patch(handles, count, actual);
}

int APS5_VABI sceKernelGetPageTableStats_nid_disambig1_nid_postfix(int* cpuTotal, int* cpuAvailable, int* gpuTotal, int* gpuAvailable) {
    return sceKernelGetPageTableStats_nid_no_patch(cpuTotal, cpuAvailable, gpuTotal, gpuAvailable);
}

void* APS5_VABI sceKernelGetProcParam_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProcParam_nid_no_patch();
}

int APS5_VABI sceKernelGetProcessAbiType_nid_disambig1_nid_postfix(int pid) {
    return sceKernelGetProcessAbiType_nid_no_patch(pid);
}

std::uint64_t APS5_VABI sceKernelGetProcessTime_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProcessTime_nid_no_patch();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProcessTimeCounter_nid_no_patch();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProcessTimeCounterFrequency_nid_no_patch();
}

int APS5_VABI sceKernelGetProsperoSystemSwVersion_nid_disambig2_nid_postfix(SceKernelProsperoSystemSwVersion* version) {
    return sceKernelGetProsperoSystemSwVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetPrtAperture_nid_disambig1_nid_postfix(int index, void** addr, size_t* len) {
    return sceKernelGetPrtAperture_nid_no_patch(index, addr, len);
}

int APS5_VABI sceKernelGetResidentFmemCount_nid_disambig1_nid_postfix(int pid, uint64_t* resident, uint64_t* mapped) {
    return sceKernelGetResidentFmemCount_nid_no_patch(pid, resident, mapped);
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSanitizerMallocReplaceExternal_nid_no_patch();
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSanitizerNewReplaceExternal_nid_no_patch();
}

int APS5_VABI sceKernelGetSocPowerConsumption_nid_disambig1_nid_postfix(void* consumption) {
    return sceKernelGetSocPowerConsumption_nid_no_patch(consumption);
}

int APS5_VABI sceKernelGetSocSensorTemperature_nid_disambig1_nid_postfix(int sensorId, int* outCelsius) {
    return sceKernelGetSocSensorTemperature_nid_no_patch(sensorId, outCelsius);
}

int APS5_VABI sceKernelGetSystemSwVersion_nid_disambig2_nid_postfix(SceKernelSystemSwVersion* version) {
    return sceKernelGetSystemSwVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetTraceMemoryStats_nid_disambig1_nid_postfix(std::uint64_t* first, std::uint64_t* second) {
    return sceKernelGetTraceMemoryStats_nid_no_patch(first, second);
}

std::uint64_t APS5_VABI sceKernelGetTscFrequency_nid_disambig1_nid_postfix(void) {
    return sceKernelGetTscFrequency_nid_no_patch();
}

int APS5_VABI sceKernelGetdents_nid_disambig1_nid_postfix(int fd, char* buf, int nbytes) {
    return sceKernelGetdents_nid_no_patch(fd, buf, nbytes);
}

int APS5_VABI sceKernelGetdirentries_nid_disambig1_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return sceKernelGetdirentries_nid_no_patch(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelGettimeofday_nid_disambig1_nid_postfix(KernelTimeval* tp) {
    return sceKernelGettimeofday_nid_no_patch(tp);
}

int APS5_VABI sceKernelGettimezone_nid_disambig1_nid_postfix(KernelTimezone* tz) {
    return sceKernelGettimezone_nid_no_patch(tz);
}

int APS5_VABI sceKernelInstallExceptionHandler_nid_disambig1_nid_postfix(int signum, void* handler) {
    return sceKernelInstallExceptionHandler_nid_no_patch(signum, handler);
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelPrintBacktraceWithModuleInfo_nid_no_patch();
}

int APS5_VABI sceKernelInternalMemoryGetAvailableSize_nid_disambig1_nid_postfix(std::size_t* size) {
    return sceKernelInternalMemoryGetAvailableSize_nid_no_patch(size);
}

std::int32_t APS5_VABI sceKernelInternalMemoryGetModuleSegmentInfo_nid_disambig2_nid_postfix(InternalSegmentInfo* result) {
    return sceKernelInternalMemoryGetModuleSegmentInfo_nid_no_patch(result);
}

int APS5_VABI sceKernelIsAddressSanitizerEnabled_nid_disambig1_nid_postfix(void) {
    return sceKernelIsAddressSanitizerEnabled_nid_no_patch();
}

int APS5_VABI sceKernelIsThreadSanitizerEnabled_nid_disambig1_nid_postfix(void) {
    return sceKernelIsThreadSanitizerEnabled_nid_no_patch();
}

int APS5_VABI sceKernelIsStack_nid_disambig1_nid_postfix(void* addr, void** start, void** end) {
    return sceKernelIsStack_nid_no_patch(addr, start, end);
}

KernelModule APS5_VABI sceKernelLoadStartModule_nid_disambig1_nid_postfix(const char* moduleFileName, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModule_nid_no_patch(moduleFileName, args, argp, flags, opt, res);
}

std::int64_t APS5_VABI sceKernelLseek_nid_disambig1_nid_postfix(int d, std::int64_t offset, int whence) {
    return sceKernelLseek_nid_no_patch(d, offset, whence);
}

int APS5_VABI sceKernelMapDirectMemory_nid_disambig1_nid_postfix(void** addr, size_t len, int prot, int flags, int64_t directMemoryStart, size_t alignment) {
    return sceKernelMapDirectMemory_nid_no_patch(addr, len, prot, flags, directMemoryStart, alignment);
}

int APS5_VABI sceKernelMapDirectMemory2_nid_disambig1_nid_postfix(void** addr, size_t len, int type, int prot, int flags, int64_t directMemoryStart, size_t alignment) {
    return sceKernelMapDirectMemory2_nid_no_patch(addr, len, type, prot, flags, directMemoryStart, alignment);
}

int APS5_VABI sceKernelMapFlexibleMemory_nid_disambig1_nid_postfix(void** addrInOut, size_t len, int prot, int flags) {
    return sceKernelMapFlexibleMemory_nid_no_patch(addrInOut, len, prot, flags);
}

int APS5_VABI sceKernelMapNamedDirectMemory_nid_disambig1_nid_postfix(void** addr, size_t len, int prot, int flags, int64_t directMemoryStart, size_t alignment, const char* name) {
    return sceKernelMapNamedDirectMemory_nid_no_patch(addr, len, prot, flags, directMemoryStart, alignment, name);
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory_nid_disambig1_nid_postfix(void** addrInOut, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedFlexibleMemory_nid_no_patch(addrInOut, len, prot, flags, name);
}

int32_t APS5_VABI sceKernelMapNamedSystemFlexibleMemory_nid_disambig1_nid_postfix(void** addrInOut, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedSystemFlexibleMemory_nid_no_patch(addrInOut, len, prot, flags, name);
}

int APS5_VABI sceKernelMemoryPoolBatch_nid_disambig1_nid_postfix(const KernelMemoryPoolBatchEntry* entries, int numEntries, int* numEntriesOut, int flags) {
    return sceKernelMemoryPoolBatch_nid_no_patch(entries, numEntries, numEntriesOut, flags);
}

int APS5_VABI sceKernelMemoryPoolCommit_nid_disambig1_nid_postfix(void* addr, size_t len, int type, int prot, int flags) {
    return sceKernelMemoryPoolCommit_nid_no_patch(addr, len, type, prot, flags);
}

int APS5_VABI sceKernelMemoryPoolDecommit_nid_disambig1_nid_postfix(void* addr, size_t len, int flags) {
    return sceKernelMemoryPoolDecommit_nid_no_patch(addr, len, flags);
}

int APS5_VABI sceKernelMemoryPoolExpand_nid_disambig1_nid_postfix(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physAddrOut) {
    return sceKernelMemoryPoolExpand_nid_no_patch(searchStart, searchEnd, len, alignment, physAddrOut);
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats_nid_disambig1_nid_postfix(KernelMemoryPoolBlockStats* output, size_t outputSize) {
    return sceKernelMemoryPoolGetBlockStats_nid_no_patch(output, outputSize);
}

int APS5_VABI sceKernelMemoryPoolReserve_nid_disambig1_nid_postfix(void* addrIn, size_t len, size_t alignment, int flags, void** addrOut) {
    return sceKernelMemoryPoolReserve_nid_no_patch(addrIn, len, alignment, flags, addrOut);
}

int APS5_VABI sceKernelMkdir_nid_disambig1_nid_postfix(const char* path, uint16_t mode) {
    return sceKernelMkdir_nid_no_patch(path, mode);
}

int APS5_VABI sceKernelMlock_nid_disambig2_nid_postfix(void* address, std::uint64_t length) {
    return sceKernelMlock_nid_no_patch(address, length);
}

int APS5_VABI sceKernelMmap_nid_disambig1_nid_postfix(void* addr, size_t len, int prot, int flags, int fd, int64_t offset, void** res) {
    return sceKernelMmap_nid_no_patch(addr, len, prot, flags, fd, offset, res);
}

int APS5_VABI sceKernelMprotect_nid_disambig1_nid_postfix(const void* addr, size_t len, int prot) {
    return sceKernelMprotect_nid_no_patch(addr, len, prot);
}

int APS5_VABI sceKernelMtypeprotect_nid_disambig1_nid_postfix(const void* addr, size_t len, int type, int prot) {
    return sceKernelMtypeprotect_nid_no_patch(addr, len, type, prot);
}

int APS5_VABI sceKernelMunlock_nid_disambig2_nid_postfix(void* address, std::uint64_t length) {
    return sceKernelMunlock_nid_no_patch(address, length);
}

int APS5_VABI sceKernelMunlockall_nid_disambig2_nid_postfix(void) {
    return sceKernelMunlockall_nid_no_patch();
}

int APS5_VABI sceKernelMunmap_nid_disambig1_nid_postfix(uint64_t vaddr, size_t len) {
    return sceKernelMunmap_nid_no_patch(vaddr, len);
}

int APS5_VABI sceKernelNanosleep_nid_disambig1_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep_nid_no_patch(rqtp, rmtp);
}

int APS5_VABI sceKernelOpen_nid_disambig1_nid_postfix(const char* path, int flags, std::uint16_t mode) {
    return sceKernelOpen_nid_no_patch(path, flags, mode);
}

int APS5_VABI sceKernelOpenEventFlag_nid_disambig1_nid_postfix(KernelEventFlag* ef, const char* name) {
    return sceKernelOpenEventFlag_nid_no_patch(ef, name);
}

int APS5_VABI sceKernelPollEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPat) {
    return sceKernelPollEventFlag_nid_no_patch(ef, bitPattern, waitMode, resultPat);
}

int APS5_VABI sceKernelPollSema_nid_disambig1_nid_postfix(KernelSema sem, int need) {
    return sceKernelPollSema_nid_no_patch(sem, need);
}

int64_t APS5_VABI sceKernelPread_nid_disambig1_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPread_nid_no_patch(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPreadv_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return sceKernelPreadv_nid_no_patch(d, iov, iovcnt, offset);
}

int APS5_VABI sceKernelProtectDirectMemoryForPID_nid_disambig1_nid_postfix(int64_t offset, size_t len, int prot, int pid) {
    return sceKernelProtectDirectMemoryForPID_nid_no_patch(offset, len, prot, pid);
}

int64_t APS5_VABI sceKernelPwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPwrite_nid_no_patch(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPwritev_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return sceKernelPwritev_nid_no_patch(d, iov, iovcnt, offset);
}

int APS5_VABI sceKernelQueryMemoryProtection_nid_disambig1_nid_postfix(void* addr, void** start, void** end, int* prot) {
    return sceKernelQueryMemoryProtection_nid_no_patch(addr, start, end, prot);
}

int APS5_VABI sceKernelRaiseException_nid_disambig1_nid_postfix(Pthread thread, int signum) {
    return sceKernelRaiseException_nid_no_patch(thread, signum);
}

int APS5_VABI sceKernelRdup_nid_disambig1_nid_postfix(int pid, int fd) {
    return sceKernelRdup_nid_no_patch(pid, fd);
}

std::int64_t APS5_VABI sceKernelRead_nid_disambig1_nid_postfix(int d, void* buf, std::size_t nbytes) {
    return sceKernelRead_nid_no_patch(d, buf, nbytes);
}

uint64_t APS5_VABI sceKernelReadTsc_nid_disambig1_nid_postfix(void) {
    return sceKernelReadTsc_nid_no_patch();
}

int64_t APS5_VABI sceKernelReadv_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return sceKernelReadv_nid_no_patch(d, iov, iovcnt);
}

int APS5_VABI sceKernelReleaseDirectMemory_nid_disambig1_nid_postfix(int64_t start, size_t len) {
    return sceKernelReleaseDirectMemory_nid_no_patch(start, len);
}

int APS5_VABI sceKernelReleaseFlexibleMemory_nid_disambig1_nid_postfix(void* addr, size_t len) {
    return sceKernelReleaseFlexibleMemory_nid_no_patch(addr, len);
}

int APS5_VABI sceKernelRemoveExceptionHandler_nid_disambig1_nid_postfix(int signum) {
    return sceKernelRemoveExceptionHandler_nid_no_patch(signum);
}

int APS5_VABI sceKernelRename_nid_disambig1_nid_postfix(const char* from, const char* to) {
    return sceKernelRename_nid_no_patch(from, to);
}

int APS5_VABI sceKernelReserveVirtualRange_nid_disambig1_nid_postfix(void** addr, size_t len, int flags, size_t alignment) {
    return sceKernelReserveVirtualRange_nid_no_patch(addr, len, flags, alignment);
}

int APS5_VABI sceKernelRmdir_nid_disambig1_nid_postfix(const char* path) {
    return sceKernelRmdir_nid_no_patch(path);
}

int APS5_VABI sceKernelSendNotificationRequest_nid_disambig1_nid_postfix(int device, const void* request, std::size_t size, int flags) {
    return sceKernelSendNotificationRequest_nid_no_patch(device, request, size, flags);
}

int APS5_VABI sceKernelSetEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, std::uint64_t bitPattern) {
    return sceKernelSetEventFlag_nid_no_patch(ef, bitPattern);
}

void APS5_VABI sceKernelSetGPO_nid_disambig1_nid_postfix(std::uint32_t bits) {
    sceKernelSetGPO_nid_no_patch(bits);
}

int APS5_VABI sceKernelSetPrtAperture_nid_disambig1_nid_postfix(int index, void* addr, size_t len) {
    return sceKernelSetPrtAperture_nid_no_patch(index, addr, len);
}

int APS5_VABI sceKernelSetVirtualRangeName_nid_disambig1_nid_postfix(const void* addr, uint64_t len, const char* name) {
    return sceKernelSetVirtualRangeName_nid_no_patch(addr, len, name);
}

int APS5_VABI sceKernelSignalSema_nid_disambig1_nid_postfix(KernelSema sem, int count) {
    return sceKernelSignalSema_nid_no_patch(sem, count);
}

unsigned int APS5_VABI sceKernelSleep_nid_disambig1_nid_postfix(unsigned int seconds) {
    return sceKernelSleep_nid_no_patch(seconds);
}

int APS5_VABI sceKernelStat_nid_disambig1_nid_postfix(const char* path, FileStat* sb) {
    return sceKernelStat_nid_no_patch(path, sb);
}

int APS5_VABI sceKernelStopUnloadModule_nid_disambig1_nid_postfix(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
    return sceKernelStopUnloadModule_nid_no_patch(handle, args, argp, flags, opt, res);
}

void APS5_VABI sceKernelSync_nid_disambig1_nid_postfix(void) {
    sceKernelSync_nid_no_patch();
}

int APS5_VABI sceKernelTriggerUserEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, void* udata) {
    return sceKernelTriggerUserEvent_nid_no_patch(eq, id, udata);
}

int APS5_VABI sceKernelTruncate_nid_disambig2_nid_postfix(const char* path, std::int64_t length) {
    return sceKernelTruncate_nid_no_patch(path, length);
}

int APS5_VABI sceKernelUnlink_nid_disambig1_nid_postfix(const char* path) {
    return sceKernelUnlink_nid_no_patch(path);
}

int APS5_VABI sceKernelUsleep_nid_disambig2_nid_postfix(KernelUseconds microseconds) {
    return sceKernelUsleep_nid_no_patch(microseconds);
}

int APS5_VABI sceKernelUtimes_nid_disambig2_nid_postfix(const char* path, const KernelTimeval* times) {
    return sceKernelUtimes_nid_no_patch(path, times);
}

int APS5_VABI sceKernelUuidCreate_nid_disambig1_nid_postfix(std::uint32_t* uuid) {
    return sceKernelUuidCreate_nid_no_patch(uuid);
}

int APS5_VABI sceKernelVirtualQuery_nid_disambig1_nid_postfix(const void* addr, int flags, VirtualQueryInfo* info, uint64_t infoSize) {
    return sceKernelVirtualQuery_nid_no_patch(addr, flags, info, infoSize);
}

int APS5_VABI sceKernelWaitEqueue_nid_disambig1_nid_postfix(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    return sceKernelWaitEqueue_nid_no_patch(eq, ev, num, out, timo);
}

int APS5_VABI sceKernelWaitEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPat, KernelUseconds* timeout) {
    return sceKernelWaitEventFlag_nid_no_patch(ef, bitPattern, waitMode, resultPat, timeout);
}

int APS5_VABI sceKernelWaitSema_nid_disambig1_nid_postfix(KernelSema sem, int need, KernelUseconds* time) {
    return sceKernelWaitSema_nid_no_patch(sem, need, time);
}

std::int64_t APS5_VABI sceKernelWrite_nid_disambig1_nid_postfix(int d, const void* buf, std::size_t nbytes) {
    return sceKernelWrite_nid_no_patch(d, buf, nbytes);
}

int64_t APS5_VABI sceKernelWritev_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return sceKernelWritev_nid_no_patch(d, iov, iovcnt);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return sceKernelWriteModifyMtypeProtectCommand_nid_no_patch(dst, address, size, type, protection, mask, outSize);
}

int APS5_VABI sceKernelWriteRemapIntoPrtCommand_nid_disambig1_nid_postfix(std::uint32_t* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, int protection, int opcode, std::uint64_t* outSize) {
    return sceKernelWriteRemapIntoPrtCommand_nid_no_patch(dst, address, source, size, protection, opcode, outSize);
}

int APS5_VABI scePthreadAttrDestroy_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return scePthreadAttrDestroy_nid_no_patch(attr);
}

int APS5_VABI scePthreadAttrGet_nid_disambig1_nid_postfix(Pthread thread, PthreadAttr* attr) {
    return scePthreadAttrGet_nid_no_patch(thread, attr);
}

int APS5_VABI scePthreadAttrGetaffinity_nid_disambig1_nid_postfix(const PthreadAttr* attr, KernelCpumask* mask) {
    return scePthreadAttrGetaffinity_nid_no_patch(attr, mask);
}

int APS5_VABI scePthreadAttrGetdetachstate_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* state) {
    return scePthreadAttrGetdetachstate_nid_no_patch(attr, state);
}

int APS5_VABI scePthreadAttrGetguardsize_nid_disambig1_nid_postfix(const PthreadAttr* attr, size_t* guardSize) {
    return scePthreadAttrGetguardsize_nid_no_patch(attr, guardSize);
}

int APS5_VABI scePthreadAttrGetinheritsched_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* inheritSched) {
    return scePthreadAttrGetinheritsched_nid_no_patch(attr, inheritSched);
}

int APS5_VABI scePthreadAttrGetschedparam_nid_disambig1_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
    return scePthreadAttrGetschedparam_nid_no_patch(attr, param);
}

int APS5_VABI scePthreadAttrGetschedpolicy_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* policy) {
    return scePthreadAttrGetschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI scePthreadAttrGetsolosched_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* solosched) {
    return scePthreadAttrGetsolosched_nid_no_patch(attr, solosched);
}

int APS5_VABI scePthreadAttrGetstack_nid_disambig1_nid_postfix(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    return scePthreadAttrGetstack_nid_no_patch(attr, stackaddr, stacksize);
}

int APS5_VABI scePthreadAttrGetstackaddr_nid_disambig1_nid_postfix(const PthreadAttr* attr, void** stackAddr) {
    return scePthreadAttrGetstackaddr_nid_no_patch(attr, stackAddr);
}

int APS5_VABI scePthreadAttrGetstacksize_nid_disambig1_nid_postfix(const PthreadAttr* attr, size_t* stackSize) {
    return scePthreadAttrGetstacksize_nid_no_patch(attr, stackSize);
}

int APS5_VABI scePthreadAttrInit_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return scePthreadAttrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadAttrSetaffinity_nid_disambig1_nid_postfix(PthreadAttr* attr, KernelCpumask mask) {
    return scePthreadAttrSetaffinity_nid_no_patch(attr, mask);
}

int APS5_VABI scePthreadAttrSetdetachstate_nid_disambig1_nid_postfix(PthreadAttr* attr, int detachstate) {
    return scePthreadAttrSetdetachstate_nid_no_patch(attr, detachstate);
}

int APS5_VABI scePthreadAttrSetguardsize_nid_disambig1_nid_postfix(PthreadAttr* attr, size_t guardSize) {
    return scePthreadAttrSetguardsize_nid_no_patch(attr, guardSize);
}

int APS5_VABI scePthreadAttrSetinheritsched_nid_disambig1_nid_postfix(PthreadAttr* attr, int inheritSched) {
    return scePthreadAttrSetinheritsched_nid_no_patch(attr, inheritSched);
}

int APS5_VABI scePthreadAttrSetschedparam_nid_disambig1_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    return scePthreadAttrSetschedparam_nid_no_patch(attr, param);
}

int APS5_VABI scePthreadAttrSetschedpolicy_nid_disambig1_nid_postfix(PthreadAttr* attr, int policy) {
    return scePthreadAttrSetschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI scePthreadAttrSetstack_nid_disambig1_nid_postfix(PthreadAttr* attr, void* addr, size_t size) {
    return scePthreadAttrSetstack_nid_no_patch(attr, addr, size);
}

int APS5_VABI scePthreadAttrSetstackaddr_nid_disambig1_nid_postfix(PthreadAttr* attr, void* addr) {
    return scePthreadAttrSetstackaddr_nid_no_patch(attr, addr);
}

int APS5_VABI scePthreadAttrSetstacksize_nid_disambig1_nid_postfix(PthreadAttr* attr, std::size_t stacksize) {
    return scePthreadAttrSetstacksize_nid_no_patch(attr, stacksize);
}

int APS5_VABI scePthreadAttrSetsolosched_nid_disambig1_nid_postfix(PthreadAttr* attr, int solosched) {
    return scePthreadAttrSetsolosched_nid_no_patch(attr, solosched);
}

int APS5_VABI scePthreadCancel_nid_disambig1_nid_postfix(Pthread thread) {
    return scePthreadCancel_nid_no_patch(thread);
}

int APS5_VABI scePthreadCondBroadcast_nid_disambig1_nid_postfix(PthreadCond* cond) {
    return scePthreadCondBroadcast_nid_no_patch(cond);
}

int APS5_VABI scePthreadCondDestroy_nid_disambig1_nid_postfix(PthreadCond* cond) {
    return scePthreadCondDestroy_nid_no_patch(cond);
}

int APS5_VABI scePthreadCondInit_nid_disambig1_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr, const char* name) {
    return scePthreadCondInit_nid_no_patch(cond, attr, name);
}

int APS5_VABI scePthreadCondSignal_nid_disambig1_nid_postfix(PthreadCond* cond) {
    return scePthreadCondSignal_nid_no_patch(cond);
}

int APS5_VABI scePthreadCondSignalto_nid_disambig1_nid_postfix(PthreadCond* cond, Pthread thread) {
    return scePthreadCondSignalto_nid_no_patch(cond, thread);
}

int APS5_VABI scePthreadCondTimedwait_nid_disambig1_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return scePthreadCondTimedwait_nid_no_patch(cond, mutex, usec);
}

int APS5_VABI scePthreadCondWait_nid_disambig1_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait_nid_no_patch(cond, mutex);
}

int APS5_VABI scePthreadCondattrDestroy_nid_disambig1_nid_postfix(PthreadCondattr* attr) {
    return scePthreadCondattrDestroy_nid_no_patch(attr);
}

int APS5_VABI scePthreadCondattrGetpshared_nid_disambig1_nid_postfix(const PthreadCondattr* attr, int* pshared) {
    return scePthreadCondattrGetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadCondattrInit_nid_disambig1_nid_postfix(PthreadCondattr* attr) {
    return scePthreadCondattrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadCondattrSetclock_nid_disambig1_nid_postfix(PthreadCondattr* attr, KernelClockid clockId) {
    return scePthreadCondattrSetclock_nid_no_patch(attr, clockId);
}

int APS5_VABI scePthreadCondattrSetpshared_nid_disambig1_nid_postfix(PthreadCondattr* attr, int pshared) {
    return scePthreadCondattrSetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadCreate_nid_disambig1_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return scePthreadCreate_nid_no_patch(thread, attr, entry, arg, name);
}

int APS5_VABI scePthreadDetach_nid_disambig1_nid_postfix(Pthread thread) {
    return scePthreadDetach_nid_no_patch(thread);
}

int APS5_VABI scePthreadEqual_nid_disambig1_nid_postfix(Pthread thread1, Pthread thread2) {
    return scePthreadEqual_nid_no_patch(thread1, thread2);
}

void APS5_VABI scePthreadExit_nid_disambig1_nid_postfix(void* retval) {
    scePthreadExit_nid_no_patch(retval);
}

int APS5_VABI scePthreadGetaffinity_nid_disambig1_nid_postfix(Pthread thread, KernelCpumask* mask) {
    return scePthreadGetaffinity_nid_no_patch(thread, mask);
}

int APS5_VABI scePthreadGetname_nid_disambig1_nid_postfix(Pthread thread, char* name) {
    return scePthreadGetname_nid_no_patch(thread, name);
}

int APS5_VABI scePthreadGetprio_nid_disambig1_nid_postfix(Pthread thread, int* prio) {
    return scePthreadGetprio_nid_no_patch(thread, prio);
}

int APS5_VABI scePthreadGetschedparam_nid_disambig1_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    return scePthreadGetschedparam_nid_no_patch(thread, policy, param);
}

void* APS5_VABI scePthreadGetspecific_nid_disambig1_nid_postfix(PthreadKey key) {
    return scePthreadGetspecific_nid_no_patch(key);
}

int APS5_VABI scePthreadGetthreadid_nid_disambig1_nid_postfix(void) {
    return scePthreadGetthreadid_nid_no_patch();
}

int APS5_VABI scePthreadJoin_nid_disambig1_nid_postfix(Pthread thread, void** retval) {
    return scePthreadJoin_nid_no_patch(thread, retval);
}

int APS5_VABI scePthreadKeyCreate_nid_disambig1_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return scePthreadKeyCreate_nid_no_patch(key, destructor);
}

int APS5_VABI scePthreadKeyDelete_nid_disambig1_nid_postfix(PthreadKey key) {
    return scePthreadKeyDelete_nid_no_patch(key);
}

int APS5_VABI scePthreadMutexDestroy_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return scePthreadMutexDestroy_nid_no_patch(mutex);
}

int APS5_VABI scePthreadMutexGetprioceiling_nid_disambig1_nid_postfix(PthreadMutex* mutex, int* prioceiling) {
    return scePthreadMutexGetprioceiling_nid_no_patch(mutex, prioceiling);
}

int APS5_VABI scePthreadMutexGetspinloops_nid_disambig1_nid_postfix(PthreadMutex* mutex, int* count) {
    return scePthreadMutexGetspinloops_nid_no_patch(mutex, count);
}

int APS5_VABI scePthreadMutexGetyieldloops_nid_disambig1_nid_postfix(PthreadMutex* mutex, int* count) {
    return scePthreadMutexGetyieldloops_nid_no_patch(mutex, count);
}

int APS5_VABI scePthreadMutexInit_nid_disambig1_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name) {
    return scePthreadMutexInit_nid_no_patch(mutex, attr, name);
}

int APS5_VABI scePthreadMutexLock_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return scePthreadMutexLock_nid_no_patch(mutex);
}

int APS5_VABI scePthreadMutexSetprioceiling_nid_disambig1_nid_postfix(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return scePthreadMutexSetprioceiling_nid_no_patch(mutex, ceiling, oldCeiling);
}

int APS5_VABI scePthreadMutexSetspinloops_nid_disambig1_nid_postfix(PthreadMutex* mutex, int count) {
    return scePthreadMutexSetspinloops_nid_no_patch(mutex, count);
}

int APS5_VABI scePthreadMutexSetyieldloops_nid_disambig1_nid_postfix(PthreadMutex* mutex, int count) {
    return scePthreadMutexSetyieldloops_nid_no_patch(mutex, count);
}

int APS5_VABI scePthreadMutexTimedlock_nid_disambig1_nid_postfix(PthreadMutex* mutex, KernelUseconds usec) {
    return scePthreadMutexTimedlock_nid_no_patch(mutex, usec);
}

int APS5_VABI scePthreadMutexTrylock_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return scePthreadMutexTrylock_nid_no_patch(mutex);
}

int APS5_VABI scePthreadMutexUnlock_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return scePthreadMutexUnlock_nid_no_patch(mutex);
}

int APS5_VABI scePthreadMutexattrDestroy_nid_disambig1_nid_postfix(PthreadMutexattr* attr) {
    return scePthreadMutexattrDestroy_nid_no_patch(attr);
}

int APS5_VABI scePthreadMutexattrGetkind_nid_disambig1_nid_postfix(PthreadMutexattr attr) {
    return scePthreadMutexattrGetkind_nid_no_patch(attr);
}

int APS5_VABI scePthreadMutexattrGetprioceiling_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* prioceiling) {
    return scePthreadMutexattrGetprioceiling_nid_no_patch(attr, prioceiling);
}

int APS5_VABI scePthreadMutexattrGetprotocol_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* protocol) {
    return scePthreadMutexattrGetprotocol_nid_no_patch(attr, protocol);
}

int APS5_VABI scePthreadMutexattrGetpshared_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* pshared) {
    return scePthreadMutexattrGetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadMutexattrGettype_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* type) {
    return scePthreadMutexattrGettype_nid_no_patch(attr, type);
}

int APS5_VABI scePthreadMutexattrInit_nid_disambig1_nid_postfix(PthreadMutexattr* attr) {
    return scePthreadMutexattrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadMutexattrSetkind_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int kind) {
    return scePthreadMutexattrSetkind_nid_no_patch(attr, kind);
}

int APS5_VABI scePthreadMutexattrSetprioceiling_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int prioceiling) {
    return scePthreadMutexattrSetprioceiling_nid_no_patch(attr, prioceiling);
}

int APS5_VABI scePthreadMutexattrSetprotocol_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int protocol) {
    return scePthreadMutexattrSetprotocol_nid_no_patch(attr, protocol);
}

int APS5_VABI scePthreadMutexattrSetpshared_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int pshared) {
    return scePthreadMutexattrSetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadMutexattrSettype_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int type) {
    return scePthreadMutexattrSettype_nid_no_patch(attr, type);
}

int APS5_VABI scePthreadOnce_nid_disambig1_nid_postfix(int32_t* once, void (APS5_VABI* init)(void)) {
    return scePthreadOnce_nid_no_patch(once, init);
}

int APS5_VABI scePthreadRename_nid_disambig1_nid_postfix(Pthread thread, const char* name) {
    return scePthreadRename_nid_no_patch(thread, name);
}

int APS5_VABI scePthreadRwlockDestroy_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return scePthreadRwlockDestroy_nid_no_patch(rwlock);
}

int APS5_VABI scePthreadRwlockInit_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
    return scePthreadRwlockInit_nid_no_patch(rwlock, attr, name);
}

int APS5_VABI scePthreadRwlockRdlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return scePthreadRwlockRdlock_nid_no_patch(rwlock);
}

int APS5_VABI scePthreadRwlockTimedrdlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    return scePthreadRwlockTimedrdlock_nid_no_patch(rwlock, usec);
}

int APS5_VABI scePthreadRwlockTimedwrlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    return scePthreadRwlockTimedwrlock_nid_no_patch(rwlock, usec);
}

int APS5_VABI scePthreadRwlockTryrdlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return scePthreadRwlockTryrdlock_nid_no_patch(rwlock);
}

int APS5_VABI scePthreadRwlockTrywrlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return scePthreadRwlockTrywrlock_nid_no_patch(rwlock);
}

int APS5_VABI scePthreadRwlockUnlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return scePthreadRwlockUnlock_nid_no_patch(rwlock);
}

int APS5_VABI scePthreadRwlockWrlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return scePthreadRwlockWrlock_nid_no_patch(rwlock);
}

int APS5_VABI scePthreadRwlockattrDestroy_nid_disambig1_nid_postfix(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrDestroy_nid_no_patch(attr);
}

int APS5_VABI scePthreadRwlockattrGetpshared_nid_disambig1_nid_postfix(const PthreadRwlockattr* attr, int* pshared) {
    return scePthreadRwlockattrGetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrInit_nid_disambig1_nid_postfix(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadRwlockattrSetpshared_nid_disambig1_nid_postfix(PthreadRwlockattr* attr, int pshared) {
    return scePthreadRwlockattrSetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrGettype_nid_disambig1_nid_postfix(const PthreadRwlockattr* attr, int* type) {
    return scePthreadRwlockattrGettype_nid_no_patch(attr, type);
}

int APS5_VABI scePthreadRwlockattrSettype_nid_disambig1_nid_postfix(PthreadRwlockattr* attr, int type) {
    return scePthreadRwlockattrSettype_nid_no_patch(attr, type);
}

Pthread APS5_VABI scePthreadSelf_nid_disambig1_nid_postfix(void) {
    return scePthreadSelf_nid_no_patch();
}

int APS5_VABI scePthreadSemInit_nid_disambig1_nid_postfix(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    return scePthreadSemInit_nid_no_patch(sem, flag, value, name);
}

int APS5_VABI scePthreadSemDestroy_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemDestroy_nid_no_patch(sem);
}

int APS5_VABI scePthreadSemTimedwait_nid_disambig1_nid_postfix(PthreadSem* sem, KernelUseconds usec) {
    return scePthreadSemTimedwait_nid_no_patch(sem, usec);
}

int APS5_VABI scePthreadSemGetvalue_nid_disambig1_nid_postfix(PthreadSem* sem, int* value) {
    return scePthreadSemGetvalue_nid_no_patch(sem, value);
}

int APS5_VABI scePthreadSemPost_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemPost_nid_no_patch(sem);
}

int APS5_VABI scePthreadSemTrywait_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemTrywait_nid_no_patch(sem);
}

int APS5_VABI scePthreadSemWait_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemWait_nid_no_patch(sem);
}

int APS5_VABI scePthreadSetaffinity_nid_disambig1_nid_postfix(Pthread thread, KernelCpumask mask) {
    return scePthreadSetaffinity_nid_no_patch(thread, mask);
}

int APS5_VABI scePthreadSetcancelstate_nid_disambig1_nid_postfix(int state, int* oldState) {
    return scePthreadSetcancelstate_nid_no_patch(state, oldState);
}

int APS5_VABI scePthreadSetcanceltype_nid_disambig1_nid_postfix(int type, int* oldType) {
    return scePthreadSetcanceltype_nid_no_patch(type, oldType);
}

int APS5_VABI scePthreadSetprio_nid_disambig1_nid_postfix(Pthread thread, int prio) {
    return scePthreadSetprio_nid_no_patch(thread, prio);
}

int APS5_VABI scePthreadSetschedparam_nid_disambig1_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    return scePthreadSetschedparam_nid_no_patch(thread, policy, param);
}

int APS5_VABI scePthreadSetspecific_nid_disambig1_nid_postfix(PthreadKey key, void* value) {
    return scePthreadSetspecific_nid_no_patch(key, value);
}

void APS5_VABI scePthreadTestcancel_nid_disambig1_nid_postfix(void) {
    scePthreadTestcancel_nid_no_patch();
}

void APS5_VABI scePthreadYield_nid_disambig1_nid_postfix(void) {
    scePthreadYield_nid_no_patch();
}

int APS5_VABI sched_get_priority_max_nid_disambig2_nid_postfix(int policy) {
    return sched_get_priority_max_nid_no_patch(policy);
}

int APS5_VABI sched_get_priority_min_nid_disambig2_nid_postfix(int policy) {
    return sched_get_priority_min_nid_no_patch(policy);
}

int APS5_VABI sched_getparam_nid_disambig2_nid_postfix(int pid, KernelSchedParam* param) {
    return sched_getparam_nid_no_patch(pid, param);
}

int APS5_VABI sched_getscheduler_nid_disambig2_nid_postfix(int pid) {
    return sched_getscheduler_nid_no_patch(pid);
}

int APS5_VABI sched_setparam_nid_disambig2_nid_postfix(int pid, const KernelSchedParam* param) {
    return sched_setparam_nid_no_patch(pid, param);
}

int APS5_VABI sched_yield_nid_disambig2_nid_postfix(void) {
    return sched_yield_nid_no_patch();
}

int APS5_VABI select_nid_disambig2_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    return select_nid_no_patch(nfds, readfds, writefds, exceptfds, timeout);
}

int APS5_VABI sem_close_nid_disambig2_nid_postfix(void* sem) {
    return sem_close_nid_no_patch(sem);
}

int APS5_VABI sem_destroy_nid_disambig2_nid_postfix(void* sem) {
    return sem_destroy_nid_no_patch(sem);
}

int APS5_VABI sem_getvalue_nid_disambig2_nid_postfix(void* sem, int* value) {
    return sem_getvalue_nid_no_patch(sem, value);
}

int APS5_VABI sem_init_nid_disambig2_nid_postfix(void* sem, int pshared, unsigned int value) {
    return sem_init_nid_no_patch(sem, pshared, value);
}

void* APS5_VABI sem_open_nid_disambig2_nid_postfix(const char* name, int flags, std::uint64_t v0, std::uint64_t v1) {
    return sem_open_nid_no_patch(name, flags, v0, v1);
}

int APS5_VABI sem_post_nid_disambig2_nid_postfix(void* sem) {
    return sem_post_nid_no_patch(sem);
}

int APS5_VABI sem_reltimedwait_np_nid_disambig2_nid_postfix(void* sem, uint32_t usec) {
    return sem_reltimedwait_np_nid_no_patch(sem, usec);
}

int APS5_VABI sem_setname_nid_disambig2_nid_postfix(void* sem, const char* name) {
    return sem_setname_nid_no_patch(sem, name);
}

int APS5_VABI sem_timedwait_nid_disambig2_nid_postfix(void* sem, const KernelTimespec* abstime) {
    return sem_timedwait_nid_no_patch(sem, abstime);
}

int APS5_VABI sem_trywait_nid_disambig2_nid_postfix(void* sem) {
    return sem_trywait_nid_no_patch(sem);
}

int APS5_VABI sem_unlink_nid_disambig2_nid_postfix(const char* name) {
    return sem_unlink_nid_no_patch(name);
}

int APS5_VABI sem_wait_nid_disambig2_nid_postfix(void* sem) {
    return sem_wait_nid_no_patch(sem);
}

std::int64_t APS5_VABI send_nid_disambig2_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags) {
    return send_nid_no_patch(descriptor, buffer, length, flags);
}

int APS5_VABI sendfile_nid_disambig2_nid_postfix(int file, int descriptor, std::int64_t offset, std::uint64_t length, const GuestSendfileHeaders* headers, std::int64_t* sbytes, int flags) {
    return sendfile_nid_no_patch(file, descriptor, offset, length, headers, sbytes, flags);
}

std::int64_t APS5_VABI sendmsg_nid_disambig2_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    return sendmsg_nid_no_patch(descriptor, message, flags);
}

std::int64_t APS5_VABI sendto_nid_disambig2_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags, const void* address, std::uint32_t addressLength) {
    return sendto_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

int APS5_VABI setcontext_nid_disambig2_nid_postfix(const UserUcontext* context) {
    return setcontext_nid_no_patch(context);
}

int APS5_VABI setegid_nid_disambig2_nid_postfix(std::uint32_t egid) {
    return setegid_nid_no_patch(egid);
}

int APS5_VABI seteuid_nid_disambig2_nid_postfix(std::uint32_t euid) {
    return seteuid_nid_no_patch(euid);
}

int APS5_VABI setgroups_nid_disambig2_nid_postfix(int count, const std::uint32_t* groups) {
    return setgroups_nid_no_patch(count, groups);
}

int APS5_VABI setitimer_nid_disambig2_nid_postfix(int which, const GuestItimerval* value, GuestItimerval* previous) {
    return setitimer_nid_no_patch(which, value, previous);
}

int APS5_VABI setpriority_nid_disambig2_nid_postfix(int which, int who, int niceval) {
    return setpriority_nid_no_patch(which, who, niceval);
}

int APS5_VABI setregid_nid_disambig2_nid_postfix(std::uint32_t rgid, std::uint32_t egid) {
    return setregid_nid_no_patch(rgid, egid);
}

int APS5_VABI setreuid_nid_disambig2_nid_postfix(std::uint32_t ruid, std::uint32_t euid) {
    return setreuid_nid_no_patch(ruid, euid);
}

int APS5_VABI setrlimit_nid_disambig2_nid_postfix(int resource, const GuestResourceLimit* limit) {
    return setrlimit_nid_no_patch(resource, limit);
}

int APS5_VABI setsockopt_nid_disambig2_nid_postfix(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return setsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI setuid_nid_disambig2_nid_postfix(std::uint32_t uid) {
    return setuid_nid_no_patch(uid);
}

int APS5_VABI shm_open_nid_disambig2_nid_postfix(const char* path, int flags, int mode) {
    return shm_open_nid_no_patch(path, flags, mode);
}

int APS5_VABI shm_unlink_nid_disambig2_nid_postfix(const char* path) {
    return shm_unlink_nid_no_patch(path);
}

int APS5_VABI shutdown_nid_disambig2_nid_postfix(int descriptor, int how) {
    return shutdown_nid_no_patch(descriptor, how);
}

int APS5_VABI sigaction_nid_disambig2_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return sigaction_nid_no_patch(guest, action, previous);
}

int APS5_VABI sigaddset_nid_disambig2_nid_postfix(GuestSignalSet* set, int signal) {
    return sigaddset_nid_no_patch(set, signal);
}

int APS5_VABI sigaltstack_nid_disambig2_nid_postfix(const GuestStack* stack, GuestStack* previous) {
    return sigaltstack_nid_no_patch(stack, previous);
}

int APS5_VABI sigdelset_nid_disambig2_nid_postfix(GuestSignalSet* set, int signal) {
    return sigdelset_nid_no_patch(set, signal);
}

int APS5_VABI sigemptyset_nid_disambig2_nid_postfix(GuestSignalSet* set) {
    return sigemptyset_nid_no_patch(set);
}

int APS5_VABI sigfillset_nid_disambig2_nid_postfix(GuestSignalSet* set) {
    return sigfillset_nid_no_patch(set);
}

int APS5_VABI sigismember_nid_disambig2_nid_postfix(const GuestSignalSet* set, int signal) {
    return sigismember_nid_no_patch(set, signal);
}

void APS5_VABI siglongjmp_nid_disambig2_nid_postfix(std::uint64_t* buffer, int value) {
    siglongjmp_nid_no_patch(buffer, value);
}

GuestHandler APS5_VABI signal_nid_disambig2_nid_postfix(int guest, GuestHandler handler) {
    return signal_nid_no_patch(guest, handler);
}

int APS5_VABI sigpending_nid_disambig2_nid_postfix(GuestSignalSet* set) {
    return sigpending_nid_no_patch(set);
}

int APS5_VABI sigprocmask_nid_disambig2_nid_postfix(int how, const void* set, void* previousSet) {
    return sigprocmask_nid_no_patch(how, set, previousSet);
}

int APS5_VABI sigsetjmp_nid_disambig2_nid_postfix(std::uint64_t* buffer, int saveMask) {
    return sigsetjmp_nid_no_patch(buffer, saveMask);
}

int APS5_VABI sigsuspend_nid_disambig2_nid_postfix(const GuestSignalSet* set) {
    return sigsuspend_nid_no_patch(set);
}

int APS5_VABI sigtimedwait_nid_disambig2_nid_postfix(const GuestSignalSet* set, void* info, const KernelTimespec* timeout) {
    return sigtimedwait_nid_no_patch(set, info, timeout);
}

int APS5_VABI sigwait_nid_disambig2_nid_postfix(const GuestSignalSet* set, int* received) {
    return sigwait_nid_no_patch(set, received);
}

int APS5_VABI sigwaitinfo_nid_disambig2_nid_postfix(const GuestSignalSet* set, void* info) {
    return sigwaitinfo_nid_no_patch(set, info);
}

unsigned int APS5_VABI sleep_nid_disambig2_nid_postfix(unsigned int seconds) {
    return sleep_nid_no_patch(seconds);
}

int APS5_VABI socket_nid_disambig2_nid_postfix(int family, int type, int protocol) {
    return socket_nid_no_patch(family, type, protocol);
}

int APS5_VABI socketpair_nid_disambig2_nid_postfix(int family, int type, int protocol, int* descriptors) {
    return socketpair_nid_no_patch(family, type, protocol, descriptors);
}

int APS5_VABI stat_nid_disambig2_nid_postfix(const char* path, FileStat* sb) {
    return stat_nid_no_patch(path, sb);
}

int APS5_VABI swapcontext_nid_disambig2_nid_postfix(UserUcontext* oucp, const UserUcontext* ucp) {
    return swapcontext_nid_no_patch(oucp, ucp);
}

void APS5_VABI sync_nid_disambig2_nid_postfix(void) {
    sync_nid_no_patch();
}

int APS5_VABI sysKernelGetIntdevModeForRcmgr_nid_disambig2_nid_postfix(void) {
    return sysKernelGetIntdevModeForRcmgr_nid_no_patch();
}

int APS5_VABI sysKernelGetIntdevModeForInternalLibc_nid_disambig2_nid_postfix(void) {
    return sysKernelGetIntdevModeForInternalLibc_nid_no_patch();
}

int APS5_VABI sysKernelGetLowerLimitUpdVersion_nid_disambig2_nid_postfix(std::uint32_t* version) {
    return sysKernelGetLowerLimitUpdVersion_nid_no_patch(version);
}

int APS5_VABI sysKernelGetLowerLimitSysexVersion_nid_disambig2_nid_postfix(char* version) {
    return sysKernelGetLowerLimitSysexVersion_nid_no_patch(version);
}

int APS5_VABI sysKernelGetManufacturingMode_nid_disambig2_nid_postfix(void) {
    return sysKernelGetManufacturingMode_nid_no_patch();
}

int APS5_VABI sysKernelGetUpdVersion_nid_disambig2_nid_postfix(std::uint32_t* version) {
    return sysKernelGetUpdVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetSystemExVersion_nid_disambig1_nid_postfix(char* first, char* second) {
    return sceKernelGetSystemExVersion_nid_no_patch(first, second);
}

int APS5_VABI sysarch_nid_disambig1_nid_postfix(int operation, void* parameters) {
    return sysarch_nid_no_patch(operation, parameters);
}

std::int64_t APS5_VABI sysconf_nid_disambig2_nid_postfix(int name) {
    return sysconf_nid_no_patch(name);
}

int APS5_VABI sysctl_nid_disambig2_nid_postfix(const int* name, std::uint32_t nameLength, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    return sysctl_nid_no_patch(name, nameLength, oldValue, oldLength, newValue, newLength);
}

int APS5_VABI sysctlbyname_nid_disambig2_nid_postfix(const char* name, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    return sysctlbyname_nid_no_patch(name, oldValue, oldLength, newValue, newLength);
}

int APS5_VABI sysctlnametomib_nid_disambig2_nid_postfix(const char* name, int* mib, std::size_t* size) {
    return sysctlnametomib_nid_no_patch(name, mib, size);
}

int APS5_VABI system_nid_disambig2_nid_postfix(const char* command) {
    return system_nid_no_patch(command);
}

int APS5_VABI tcdrain_nid_disambig2_nid_postfix(int descriptor) {
    return tcdrain_nid_no_patch(descriptor);
}

int APS5_VABI tcflow_nid_disambig2_nid_postfix(int descriptor, int action) {
    return tcflow_nid_no_patch(descriptor, action);
}

int APS5_VABI tcflush_nid_disambig2_nid_postfix(int descriptor, int queue) {
    return tcflush_nid_no_patch(descriptor, queue);
}

int APS5_VABI tcgetattr_nid_disambig2_nid_postfix(int descriptor, void* attributes) {
    return tcgetattr_nid_no_patch(descriptor, attributes);
}

int APS5_VABI tcgetpgrp_nid_disambig2_nid_postfix(int descriptor) {
    return tcgetpgrp_nid_no_patch(descriptor);
}

int APS5_VABI tcgetsid_nid_disambig2_nid_postfix(int descriptor) {
    return tcgetsid_nid_no_patch(descriptor);
}

int APS5_VABI tcsendbreak_nid_disambig2_nid_postfix(int descriptor, int duration) {
    return tcsendbreak_nid_no_patch(descriptor, duration);
}

int APS5_VABI tcsetattr_nid_disambig2_nid_postfix(int descriptor, int action, const void* attributes) {
    return tcsetattr_nid_no_patch(descriptor, action, attributes);
}

int APS5_VABI tcsetpgrp_nid_disambig2_nid_postfix(int descriptor, int group) {
    return tcsetpgrp_nid_no_patch(descriptor, group);
}

int APS5_VABI truncate_nid_disambig2_nid_postfix(const char* path, std::int64_t length) {
    return truncate_nid_no_patch(path, length);
}

int APS5_VABI unlink_nid_disambig2_nid_postfix(const char* path) {
    return unlink_nid_no_patch(path);
}

int APS5_VABI usleep_nid_disambig2_nid_postfix(KernelUseconds microseconds) {
    return usleep_nid_no_patch(microseconds);
}

int APS5_VABI utimes_nid_disambig2_nid_postfix(const char* path, const KernelTimeval* times) {
    return utimes_nid_no_patch(path, times);
}

int APS5_VABI wait_nid_disambig2_nid_postfix(int* status) {
    return wait_nid_no_patch(status);
}

int APS5_VABI waitpid_nid_disambig2_nid_postfix(int pid, int* status, int options) {
    return waitpid_nid_no_patch(pid, status, options);
}

int64_t APS5_VABI write_nid_disambig2_nid_postfix(int d, const char* str, int64_t size) {
    return write_nid_no_patch(d, str, size);
}

int64_t APS5_VABI writev_nid_disambig2_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return writev_nid_no_patch(d, iov, iovcnt);
}

int APS5_VABI sceKernelAprResolveFilepathsToIds_nid_disambig1_nid_postfix(const char** paths, uint32_t count, uint32_t* ids, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsToIds_nid_no_patch(paths, count, ids, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizes_nid_disambig1_nid_postfix(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsToIdsAndFileSizes_nid_no_patch(paths, count, ids, sizes, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIds_nid_disambig1_nid_postfix(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsWithPrefixToIds_nid_no_patch(prefix, paths, count, ids, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes_nid_disambig1_nid_postfix(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes_nid_no_patch(prefix, paths, count, ids, sizes, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsForEach_nid_disambig1_nid_postfix(const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return sceKernelAprResolveFilepathsToIdsForEach_nid_no_patch(paths, count, ids, results);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizesForEach_nid_disambig1_nid_postfix(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    return sceKernelAprResolveFilepathsToIdsAndFileSizesForEach_nid_no_patch(paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsForEach_nid_disambig1_nid_postfix(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsForEach_nid_no_patch(prefix, paths, count, ids, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach_nid_disambig1_nid_postfix(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach_nid_no_patch(prefix, paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprGetFileSize_nid_disambig1_nid_postfix(uint32_t id, uint64_t* size) {
    return sceKernelAprGetFileSize_nid_no_patch(id, size);
}

int APS5_VABI sceKernelAprGetFileStat_nid_disambig1_nid_postfix(uint32_t id, FileStat* stat) {
    return sceKernelAprGetFileStat_nid_no_patch(id, stat);
}

int APS5_VABI sceKernelAprSubmitCommandBuffer_nid_disambig1_nid_postfix(const Apr::CommandBufferObject* buffer, uint32_t priority) {
    return sceKernelAprSubmitCommandBuffer_nid_no_patch(buffer, priority);
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetResult_nid_disambig1_nid_postfix(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* result, uint32_t* id) {
    return sceKernelAprSubmitCommandBufferAndGetResult_nid_no_patch(buffer, priority, result, id);
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetId_nid_disambig1_nid_postfix(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* id) {
    return sceKernelAprSubmitCommandBufferAndGetId_nid_no_patch(buffer, priority, id);
}

int APS5_VABI sceKernelAprWaitCommandBuffer_nid_disambig1_nid_postfix(uint32_t id) {
    return sceKernelAprWaitCommandBuffer_nid_no_patch(id);
}

int APS5_VABI cpuset_nid_disambig2_nid_postfix(int* setid) {
    return cpuset_nid_no_patch(setid);
}

int APS5_VABI cpuset_setid_nid_disambig2_nid_postfix(int which, std::int64_t id, int setid) {
    return cpuset_setid_nid_no_patch(which, id, setid);
}

int APS5_VABI lutimes_nid_disambig2_nid_postfix(const char* path, const KernelTimeval* times) {
    return lutimes_nid_no_patch(path, times);
}

int APS5_VABI nmount_nid_disambig2_nid_postfix(const KernelIovec* iov, unsigned int niov, int flags) {
    return nmount_nid_no_patch(iov, niov, flags);
}

int APS5_VABI sceKernelJitCreateAliasOfSharedMemory_nid_disambig1_nid_postfix(int fd, int maxProt, int* fdOut) {
    return sceKernelJitCreateAliasOfSharedMemory_nid_no_patch(fd, maxProt, fdOut);
}

int APS5_VABI sceKernelJitCreateSharedMemory_nid_disambig1_nid_postfix(const char* name, std::size_t len, int maxProt, int* fdOut) {
    return sceKernelJitCreateSharedMemory_nid_no_patch(name, len, maxProt, fdOut);
}

int APS5_VABI sceKernelJitMapSharedMemory_nid_disambig1_nid_postfix(int fd, int prot, void** startOut) {
    return sceKernelJitMapSharedMemory_nid_no_patch(fd, prot, startOut);
}

int APS5_VABI setpgid_nid_disambig2_nid_postfix(int pid, int pgid) {
    return setpgid_nid_no_patch(pid, pgid);
}

int APS5_VABI sceKernelGetOpenPsId_nid_disambig1_nid_postfix(void* openPsIdOutput) {
    return sceKernelGetOpenPsId_nid_no_patch(openPsIdOutput);
}

int APS5_VABI sceCoredumpRegisterCoredumpHandler_nid_disambig1_nid_postfix(uint64_t handler, size_t stackSize, uint64_t context) {
    return sceCoredumpRegisterCoredumpHandler_nid_no_patch(handler, stackSize, context);
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler_nid_disambig1_nid_postfix(void) {
    return sceCoredumpUnregisterCoredumpHandler_nid_no_patch();
}

int64_t APS5_VABI sceCoredumpWriteUserData_nid_disambig1_nid_postfix(const void* data, size_t size) {
    return sceCoredumpWriteUserData_nid_no_patch(data, size);
}

int APS5_VABI sceCoredumpAttachUserFile_nid_disambig1_nid_postfix(std::uint32_t userValue, const char* path) {
    return sceCoredumpAttachUserFile_nid_no_patch(userValue, path);
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile_nid_disambig1_nid_postfix(std::uint32_t userValue, const void* mem, std::int32_t size, const char* name) {
    return sceCoredumpAttachMemoryRegionAsUserFile_nid_no_patch(userValue, mem, size, name);
}

int APS5_VABI sceCoredumpAttachMemoryRegion_nid_disambig1_nid_postfix(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachMemoryRegion_nid_no_patch(userValue, mem, size);
}

int APS5_VABI sceCoredumpAttachUserMemoryFile_nid_disambig1_nid_postfix(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachUserMemoryFile_nid_no_patch(userValue, mem, size);
}

void APS5_VABI sceCoredumpDebugTextOut_nid_disambig1_nid_postfix(const char* str, int len) {
    sceCoredumpDebugTextOut_nid_no_patch(str, len);
}

int APS5_VABI sceCoredumpGetStopInfoCpu_nid_disambig1_nid_postfix(void* info, size_t size) {
    return sceCoredumpGetStopInfoCpu_nid_no_patch(info, size);
}

int APS5_VABI sceCoredumpGetThreadContextInfo_nid_disambig1_nid_postfix(void* threadContextInfo, size_t threadContextInfoSize) {
    return sceCoredumpGetThreadContextInfo_nid_no_patch(threadContextInfo, threadContextInfoSize);
}

int APS5_VABI sceCoredumpConfigDumpMode_nid_disambig1_nid_postfix(int mode) {
    return sceCoredumpConfigDumpMode_nid_no_patch(mode);
}

int APS5_VABI sceCoredumpSetUserDataType_nid_disambig1_nid_postfix(std::int32_t type) {
    return sceCoredumpSetUserDataType_nid_no_patch(type);
}

int APS5_VABI sceKernelGetModuleInfo2_nid_disambig1_nid_postfix(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo2_nid_no_patch(handle, info);
}

int APS5_VABI sceKernelGetModuleList2_nid_disambig1_nid_postfix(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList2_nid_no_patch(handles, count, actual);
}




int APS5_VABI libkernel_sysForward_Jrs7UUkGOFo_nid_no_patch(void* info, std::size_t size) {
    return sceCoredumpGetStopInfoGpu_Agc_nid_no_patch(info, size);
}

APS5_EXPORT("Jrs7UUkGOFo", libkernel_sysForward_Jrs7UUkGOFo_nid_no_patch);

}
