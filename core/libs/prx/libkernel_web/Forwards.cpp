#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libkernel_web/include/Forwards.hpp"
#include "prx/libkernel/include/LibKernelSysProviders.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

extern "C" {

[[noreturn]] void APS5_VABI __stack_chk_fail_nid_postfix(void);

int APS5_VABI __Ux86_64_setcontext_nid_disambig1_nid_postfix(void) {
    return __Ux86_64_setcontext_nid_no_patch();
}

int APS5_VABI __elf_phdr_match_addr_nid_disambig1_nid_postfix(dl_phdr_info* phdrInfo, void* addr) {
    return __elf_phdr_match_addr_nid_no_patch(phdrInfo, addr);
}

int* APS5_VABI __error_nid_disambig2_nid_postfix(void) {
    return __error_nid_no_patch();
}

const char* APS5_VABI __inet_ntop_nid_disambig1_nid_postfix(int family, const void* source, char* destination, std::uint32_t capacity) {
    return __inet_ntop_nid_no_patch(family, source, destination, capacity);
}

int APS5_VABI __inet_pton_nid_disambig1_nid_postfix(int family, const char* text, void* destination) {
    return __inet_pton_nid_no_patch(family, text, destination);
}

void APS5_VABI __pthread_cleanup_pop_imp_nid_disambig1_nid_postfix(int execute) {
    __pthread_cleanup_pop_imp_nid_no_patch(execute);
}

void APS5_VABI __pthread_cleanup_push_imp_nid_disambig1_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    __pthread_cleanup_push_imp_nid_no_patch(routine, argument, info);
}

void APS5_VABI __pthread_cxa_finalize_nid_disambig1_nid_postfix(void* argument) {
    __pthread_cxa_finalize_nid_no_patch(argument);
}

int APS5_VABI __sys_socketex_nid_disambig1_nid_postfix(const char* name, int family, int type, int protocol) {
    return __sys_socketex_nid_no_patch(name, family, type, protocol);
}

void* APS5_VABI __tls_get_addr_nid_disambig1_nid_postfix(const KernelTlsIndex* index) {
    return __tls_get_addr_nid_no_patch(index);
}

int APS5_VABI _accept_nid_disambig1_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return _accept_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _bind_nid_disambig1_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return _bind_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _close_nid_disambig1_nid_postfix(int descriptor) {
    return _close_nid_no_patch(descriptor);
}

int APS5_VABI _connect_nid_disambig1_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return _connect_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _fcntl_nid_disambig1_nid_postfix(int descriptor, int command, std::uint64_t v0, std::uint64_t v1) {
    return _fcntl_nid_no_patch(descriptor, command, v0, v1);
}

std::int64_t APS5_VABI _fpathconf_nid_disambig1_nid_postfix(int fd, int name) {
    return _fpathconf_nid_no_patch(fd, name);
}

int64_t APS5_VABI _fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    return _fstat_nid_no_patch(d, sb);
}

int APS5_VABI _fstatfs_nid_disambig1_nid_postfix(int d, KernelStatfs* buf) {
    return _fstatfs_nid_no_patch(d, buf);
}

int APS5_VABI _getdirentries_nid_disambig1_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return _getdirentries_nid_no_patch(fd, buf, nbytes, basep);
}

int APS5_VABI _getpeername_nid_disambig1_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return _getpeername_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _getsockname_nid_disambig1_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return _getsockname_nid_no_patch(descriptor, address, length);
}

int APS5_VABI _getsockopt_nid_disambig1_nid_postfix(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return _getsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI _ioctl_nid_disambig1_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    return _ioctl_nid_no_patch(descriptor, request, argument);
}

int APS5_VABI _is_signal_return_nid_disambig1_nid_postfix(std::uint64_t programCounter) {
    return _is_signal_return_nid_no_patch(programCounter);
}

int APS5_VABI _listen_nid_disambig1_nid_postfix(int descriptor, int backlog) {
    return _listen_nid_no_patch(descriptor, backlog);
}

int APS5_VABI _nanosleep_nid_disambig1_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return _nanosleep_nid_no_patch(rqtp, rmtp);
}

int APS5_VABI _open_nid_disambig1_nid_postfix(const char* path, int flags, std::uint64_t v0, std::uint64_t v1) {
    return _open_nid_no_patch(path, flags, v0, v1);
}

int APS5_VABI _openat_nid_disambig1_nid_postfix(int fd, const char* path, int flags, std::uint64_t v0, std::uint64_t v1) {
    return _openat_nid_no_patch(fd, path, flags, v0, v1);
}

std::int64_t APS5_VABI _read_nid_disambig1_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    return _read_nid_no_patch(descriptor, buffer, count);
}

int64_t APS5_VABI _readv_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return _readv_nid_no_patch(d, iov, iovcnt);
}

std::int64_t APS5_VABI _recvfrom_nid_disambig1_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags, void* address, std::uint32_t* addressLength) {
    return _recvfrom_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

std::int64_t APS5_VABI _recvmsg_nid_disambig1_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    return _recvmsg_nid_no_patch(descriptor, message, flags);
}

void APS5_VABI _sceKernelRtldSetApplicationHeapAPI_nid_disambig1_nid_postfix(void* api[]) {
    _sceKernelRtldSetApplicationHeapAPI_nid_no_patch(api);
}

int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_disambig1_nid_postfix(const void* dsoSymbol) {
    return _sceKernelRtldThreadAtexitDecrement_nid_no_patch(dsoSymbol);
}

int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_disambig1_nid_postfix(const void* dsoSymbol) {
    return _sceKernelRtldThreadAtexitIncrement_nid_no_patch(dsoSymbol);
}

void APS5_VABI _sceKernelSetThreadAtexitCount_nid_disambig1_nid_postfix(get_thread_atexit_count_func_t callback) {
    _sceKernelSetThreadAtexitCount_nid_no_patch(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitReport_nid_disambig1_nid_postfix(thread_atexit_report_func_t callback) {
    _sceKernelSetThreadAtexitReport_nid_no_patch(callback);
}

void APS5_VABI _sceKernelSetThreadDtors_nid_disambig1_nid_postfix(thread_dtors_func_t callback) {
    _sceKernelSetThreadDtors_nid_no_patch(callback);
}

std::int64_t APS5_VABI _sendmsg_nid_disambig1_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    return _sendmsg_nid_no_patch(descriptor, message, flags);
}

std::int64_t APS5_VABI _sendto_nid_disambig1_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags, const void* address, std::uint32_t addressLength) {
    return _sendto_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

int APS5_VABI _setsockopt_nid_disambig1_nid_postfix(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return _setsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI _sigaction_nid_disambig1_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return _sigaction_nid_no_patch(guest, action, previous);
}

int APS5_VABI _sigprocmask_nid_disambig1_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return _sigprocmask_nid_no_patch(how, set, previousSet);
}

int APS5_VABI _sigsuspend_nid_disambig1_nid_postfix(const GuestSignalSet* set) {
    return _sigsuspend_nid_no_patch(set);
}

int APS5_VABI _umtx_op_nid_disambig1_nid_postfix(void* object, int operation, std::uint64_t value, void* sizeArgument, void* timeArgument) {
    return _umtx_op_nid_no_patch(object, operation, value, sizeArgument, timeArgument);
}

std::int64_t APS5_VABI _write_nid_disambig1_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    return _write_nid_no_patch(descriptor, buffer, count);
}

int64_t APS5_VABI _writev_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return _writev_nid_no_patch(d, iov, iovcnt);
}

int APS5_VABI accept_nid_disambig1_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return accept_nid_no_patch(descriptor, address, length);
}

int APS5_VABI access_nid_disambig2_nid_postfix(const char* path, int mode) {
    return access_nid_no_patch(path, mode);
}

int APS5_VABI amd64_set_fsbase_nid_disambig1_nid_postfix(void) {
    return amd64_set_fsbase_nid_no_patch();
}

int APS5_VABI bind_nid_disambig1_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return bind_nid_no_patch(descriptor, address, length);
}

int APS5_VABI blockpool_batch_nid_disambig1_nid_postfix(void) {
    return blockpool_batch_nid_no_patch();
}

int APS5_VABI blockpool_map_nid_disambig1_nid_postfix(void) {
    return blockpool_map_nid_no_patch();
}

int APS5_VABI blockpool_open_nid_disambig1_nid_postfix(void) {
    return blockpool_open_nid_no_patch();
}

int APS5_VABI blockpool_unmap_nid_disambig1_nid_postfix(void) {
    return blockpool_unmap_nid_no_patch();
}

int APS5_VABI cfsetspeed_nid_disambig1_nid_postfix(void* attributes, std::uint32_t speed) {
    return cfsetspeed_nid_no_patch(attributes, speed);
}

int APS5_VABI chdir_nid_disambig2_nid_postfix(const char* path) {
    return chdir_nid_no_patch(path);
}

int APS5_VABI chflags_nid_disambig1_nid_postfix(const char* path, std::uint64_t flags) {
    return chflags_nid_no_patch(path, flags);
}

int APS5_VABI chmod_nid_disambig1_nid_postfix(const char* path, int mode) {
    return chmod_nid_no_patch(path, mode);
}

int APS5_VABI clock_getres_nid_disambig1_nid_postfix(int clockId, KernelTimespec* res) {
    return clock_getres_nid_no_patch(clockId, res);
}

int APS5_VABI clock_gettime_nid_disambig1_nid_postfix(int clockId, KernelTimespec* tp) {
    return clock_gettime_nid_no_patch(clockId, tp);
}

int APS5_VABI clock_settime_nid_disambig1_nid_postfix(int clockId, const KernelTimespec* tp) {
    return clock_settime_nid_no_patch(clockId, tp);
}

int APS5_VABI close_nid_disambig1_nid_postfix(int d) {
    return close_nid_no_patch(d);
}

int APS5_VABI connect_nid_disambig1_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return connect_nid_no_patch(descriptor, address, length);
}

int APS5_VABI cpuset_getaffinity_nid_disambig1_nid_postfix(int level, int which, std::int64_t id, std::size_t size, void* mask) {
    return cpuset_getaffinity_nid_no_patch(level, which, id, size, mask);
}

int APS5_VABI cpuset_getid_nid_disambig1_nid_postfix(int level, int which, std::int64_t id, int* setid) {
    return cpuset_getid_nid_no_patch(level, which, id, setid);
}

int APS5_VABI cpuset_setaffinity_nid_disambig1_nid_postfix(int level, int which, std::int64_t id, std::size_t size, const void* mask) {
    return cpuset_setaffinity_nid_no_patch(level, which, id, size, mask);
}

int APS5_VABI creat_nid_disambig1_nid_postfix(const char* path, int mode) {
    return creat_nid_no_patch(path, mode);
}

int APS5_VABI dlclose_nid_disambig1_nid_postfix(void* handle) {
    return dlclose_nid_no_patch(handle);
}

char* APS5_VABI dlerror_nid_disambig1_nid_postfix(void) {
    return dlerror_nid_no_patch();
}

void* APS5_VABI dlopen_nid_disambig1_nid_postfix(const char* path, int flags) {
    return dlopen_nid_no_patch(path, flags);
}

void* APS5_VABI dlsym_nid_disambig1_nid_postfix(void* handle, const char* name) {
    return dlsym_nid_no_patch(handle, name);
}

int APS5_VABI dup_nid_disambig1_nid_postfix(int d) {
    return dup_nid_no_patch(d);
}

int APS5_VABI dup2_nid_disambig1_nid_postfix(int from, int to) {
    return dup2_nid_no_patch(from, to);
}

int APS5_VABI dynlib_get_obj_member_nid_disambig1_nid_postfix(void) {
    return dynlib_get_obj_member_nid_no_patch();
}

int APS5_VABI execv_nid_disambig1_nid_postfix(const char* path, char* const* arguments) {
    return execv_nid_no_patch(path, arguments);
}

int APS5_VABI execve_nid_disambig1_nid_postfix(const char* path, char* const* arguments, char* const* environment) {
    return execve_nid_no_patch(path, arguments, environment);
}

int APS5_VABI execvp_nid_disambig1_nid_postfix(const char* file, char* const* arguments) {
    return execvp_nid_no_patch(file, arguments);
}

int APS5_VABI fchflags_nid_disambig1_nid_postfix(int d, std::uint64_t flags) {
    return fchflags_nid_no_patch(d, flags);
}

int APS5_VABI fchmod_nid_disambig1_nid_postfix(int d, int mode) {
    return fchmod_nid_no_patch(d, mode);
}

int APS5_VABI fcntl_nid_disambig1_nid_postfix(int descriptor, int command, std::uint64_t v0, std::uint64_t v1) {
    return fcntl_nid_no_patch(descriptor, command, v0, v1);
}

int APS5_VABI fdatasync_nid_disambig1_nid_postfix(int fd) {
    return fdatasync_nid_no_patch(fd);
}

int APS5_VABI flock_nid_disambig1_nid_postfix(int d, int operation) {
    return flock_nid_no_patch(d, operation);
}

int APS5_VABI fstat_nid_disambig2_nid_postfix(int d, FileStat* sb) {
    return fstat_nid_no_patch(d, sb);
}

int APS5_VABI fsync_nid_disambig1_nid_postfix(int fd) {
    return fsync_nid_no_patch(fd);
}

int APS5_VABI ftruncate_nid_disambig1_nid_postfix(int d, int64_t length) {
    return ftruncate_nid_no_patch(d, length);
}

int APS5_VABI futimes_nid_disambig1_nid_postfix(int d, const KernelTimeval* times) {
    return futimes_nid_no_patch(d, times);
}

int APS5_VABI get_authinfo_nid_disambig1_nid_postfix(int pid, KernelAuthInfo* info) {
    return get_authinfo_nid_no_patch(pid, info);
}

int APS5_VABI get_module_info_list_nid_disambig1_nid_postfix(void) {
    return get_module_info_list_nid_no_patch();
}

int APS5_VABI get_page_table_stats_nid_disambig1_nid_postfix(void) {
    return get_page_table_stats_nid_no_patch();
}

int APS5_VABI get_sdk_compiled_version_nid_disambig1_nid_postfix(void) {
    return get_sdk_compiled_version_nid_no_patch();
}

int APS5_VABI get_self_auth_info_nid_disambig1_nid_postfix(KernelAuthInfo* info) {
    return get_self_auth_info_nid_no_patch(info);
}

int APS5_VABI get_vm_map_timestamp_nid_disambig1_nid_postfix(void) {
    return get_vm_map_timestamp_nid_no_patch();
}

int APS5_VABI getargc_nid_disambig1_nid_postfix(void) {
    return getargc_nid_no_patch();
}

const char** APS5_VABI getargv_nid_disambig1_nid_postfix(void) {
    return getargv_nid_no_patch();
}

int APS5_VABI getdents_nid_disambig1_nid_postfix(int fd, char* buf, int nbytes) {
    return getdents_nid_no_patch(fd, buf, nbytes);
}

int APS5_VABI getdirentries_nid_disambig1_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return getdirentries_nid_no_patch(fd, buf, nbytes, basep);
}

int APS5_VABI getdtablesize_nid_disambig1_nid_postfix(void) {
    return getdtablesize_nid_no_patch();
}

int APS5_VABI getegid_nid_disambig1_nid_postfix(void) {
    return getegid_nid_no_patch();
}

int APS5_VABI geteuid_nid_disambig1_nid_postfix(void) {
    return geteuid_nid_no_patch();
}

int APS5_VABI getgid_nid_disambig1_nid_postfix(void) {
    return getgid_nid_no_patch();
}

int APS5_VABI getgroups_nid_disambig1_nid_postfix(int capacity, std::uint32_t* groups) {
    return getgroups_nid_no_patch(capacity, groups);
}

int APS5_VABI getitimer_nid_disambig1_nid_postfix(int which, GuestItimerval* output) {
    return getitimer_nid_no_patch(which, output);
}

char* APS5_VABI getlogin_nid_disambig1_nid_postfix(void) {
    return getlogin_nid_no_patch();
}

int APS5_VABI getlogin_r_nid_disambig1_nid_postfix(char* output, int size) {
    return getlogin_r_nid_no_patch(output, size);
}

int APS5_VABI getpagesize_nid_disambig1_nid_postfix(void) {
    return getpagesize_nid_no_patch();
}

int APS5_VABI getpeername_nid_disambig1_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return getpeername_nid_no_patch(descriptor, address, length);
}

int APS5_VABI getpid_nid_disambig1_nid_postfix(void) {
    return getpid_nid_no_patch();
}

int APS5_VABI getppid_nid_disambig1_nid_postfix(void) {
    return getppid_nid_no_patch();
}

int APS5_VABI getpriority_nid_disambig1_nid_postfix(int which, int who) {
    return getpriority_nid_no_patch(which, who);
}

int APS5_VABI getrlimit_nid_disambig1_nid_postfix(int resource, GuestResourceLimit* limit) {
    return getrlimit_nid_no_patch(resource, limit);
}

int APS5_VABI getrusage_nid_disambig1_nid_postfix(int who, GuestResourceUsage* usage) {
    return getrusage_nid_no_patch(who, usage);
}

int APS5_VABI getsid_nid_disambig1_nid_postfix(int pid) {
    return getsid_nid_no_patch(pid);
}

int APS5_VABI getsockname_nid_disambig1_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return getsockname_nid_no_patch(descriptor, address, length);
}

int APS5_VABI getsockopt_nid_disambig1_nid_postfix(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return getsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI gettimeofday_nid_disambig1_nid_postfix(KernelTimeval* output, KernelTimezone* zoneOutput) {
    return gettimeofday_nid_no_patch(output, zoneOutput);
}

int APS5_VABI getuid_nid_disambig1_nid_postfix(void) {
    return getuid_nid_no_patch();
}

std::uint32_t APS5_VABI htonl_nid_disambig1_nid_postfix(std::uint32_t value) {
    return htonl_nid_no_patch(value);
}

std::uint16_t APS5_VABI htons_nid_disambig1_nid_postfix(std::uint16_t value) {
    return htons_nid_no_patch(value);
}

const char* APS5_VABI inet_ntop_nid_disambig1_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
    return inet_ntop_nid_no_patch(af, src, dst, size);
}

int APS5_VABI inet_pton_nid_disambig1_nid_postfix(int af, const char* src, void* dst) {
    return inet_pton_nid_no_patch(af, src, dst);
}

int APS5_VABI ioctl_nid_disambig1_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    return ioctl_nid_no_patch(descriptor, request, argument);
}

int APS5_VABI ipmimgr_call_nid_disambig1_nid_postfix(void) {
    return ipmimgr_call_nid_no_patch();
}

int APS5_VABI is_in_sandbox_nid_disambig1_nid_postfix(void) {
    return is_in_sandbox_nid_no_patch();
}

int APS5_VABI issetugid_nid_disambig1_nid_postfix(void) {
    return issetugid_nid_no_patch();
}

int APS5_VABI kevent_nid_disambig1_nid_postfix(int descriptor, const KernelEvent* changelist, int nchanges, KernelEvent* eventlist, int nevents, const KernelTimespec* timeout) {
    return kevent_nid_no_patch(descriptor, changelist, nchanges, eventlist, nevents, timeout);
}

int APS5_VABI kill_nid_disambig1_nid_postfix(int pid, int guest) {
    return kill_nid_no_patch(pid, guest);
}

int APS5_VABI kqueue_nid_disambig1_nid_postfix(void) {
    return kqueue_nid_no_patch();
}

int APS5_VABI ksem_close_nid_disambig1_nid_postfix(void) {
    return ksem_close_nid_no_patch();
}

int APS5_VABI ksem_destroy_nid_disambig1_nid_postfix(std::intptr_t id) {
    return ksem_destroy_nid_no_patch(id);
}

int APS5_VABI ksem_getvalue_nid_disambig1_nid_postfix(std::intptr_t id, int* value) {
    return ksem_getvalue_nid_no_patch(id, value);
}

int APS5_VABI ksem_init_nid_disambig1_nid_postfix(std::intptr_t* idp, unsigned int value) {
    return ksem_init_nid_no_patch(idp, value);
}

int APS5_VABI ksem_open_nid_disambig1_nid_postfix(void) {
    return ksem_open_nid_no_patch();
}

int APS5_VABI ksem_post_nid_disambig1_nid_postfix(std::intptr_t id) {
    return ksem_post_nid_no_patch(id);
}

int APS5_VABI ksem_timedwait_nid_disambig1_nid_postfix(std::intptr_t id, const KernelTimespec* abstime) {
    return ksem_timedwait_nid_no_patch(id, abstime);
}

int APS5_VABI ksem_trywait_nid_disambig1_nid_postfix(std::intptr_t id) {
    return ksem_trywait_nid_no_patch(id);
}

int APS5_VABI ksem_unlink_nid_disambig1_nid_postfix(void) {
    return ksem_unlink_nid_no_patch();
}

int APS5_VABI ksem_wait_nid_disambig1_nid_postfix(std::intptr_t id) {
    return ksem_wait_nid_no_patch(id);
}

int APS5_VABI ktimer_create_nid_disambig1_nid_postfix(int clock, const GuestSigevent* event, int* timerid) {
    return ktimer_create_nid_no_patch(clock, event, timerid);
}

int APS5_VABI ktimer_delete_nid_disambig1_nid_postfix(int id) {
    return ktimer_delete_nid_no_patch(id);
}

int APS5_VABI ktimer_getoverrun_nid_disambig1_nid_postfix(int id) {
    return ktimer_getoverrun_nid_no_patch(id);
}

int APS5_VABI ktimer_gettime_nid_disambig1_nid_postfix(int id, GuestItimerspec* value) {
    return ktimer_gettime_nid_no_patch(id, value);
}

int APS5_VABI ktimer_settime_nid_disambig1_nid_postfix(int id, int flags, const GuestItimerspec* value, GuestItimerspec* oldValue) {
    return ktimer_settime_nid_no_patch(id, flags, value, oldValue);
}

int APS5_VABI listen_nid_disambig1_nid_postfix(int descriptor, int backlog) {
    return listen_nid_no_patch(descriptor, backlog);
}

int64_t APS5_VABI lseek_nid_disambig1_nid_postfix(int d, int64_t offset, int whence) {
    return lseek_nid_no_patch(d, offset, whence);
}

int APS5_VABI lstat_nid_disambig1_nid_postfix(const char* path, FileStat* sb) {
    return lstat_nid_no_patch(path, sb);
}

int APS5_VABI madvise_nid_disambig1_nid_postfix(void* address, std::size_t length, int advice) {
    return madvise_nid_no_patch(address, length, advice);
}

int APS5_VABI mdbg_service_nid_disambig1_nid_postfix(void) {
    return mdbg_service_nid_no_patch();
}

int APS5_VABI mincore_nid_disambig1_nid_postfix(const void* address, std::size_t length, char* vec) {
    return mincore_nid_no_patch(address, length, vec);
}

int APS5_VABI mkdir_nid_disambig1_nid_postfix(const char* path, uint16_t mode) {
    return mkdir_nid_no_patch(path, mode);
}

int APS5_VABI mlock_nid_disambig1_nid_postfix(const void* address, std::size_t length) {
    return mlock_nid_no_patch(address, length);
}

int APS5_VABI mlockall_nid_disambig1_nid_postfix(int flags) {
    return mlockall_nid_no_patch(flags);
}

int APS5_VABI msync_nid_disambig1_nid_postfix(void* address, std::size_t length, int flags) {
    return msync_nid_no_patch(address, length, flags);
}

int APS5_VABI munlock_nid_disambig1_nid_postfix(const void* address, std::size_t length) {
    return munlock_nid_no_patch(address, length);
}

int APS5_VABI munlockall_nid_disambig1_nid_postfix(void) {
    return munlockall_nid_no_patch();
}

int APS5_VABI nanosleep_nid_disambig1_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return nanosleep_nid_no_patch(rqtp, rmtp);
}

std::uint32_t APS5_VABI ntohl_nid_disambig1_nid_postfix(std::uint32_t value) {
    return ntohl_nid_no_patch(value);
}

std::uint16_t APS5_VABI ntohs_nid_disambig1_nid_postfix(std::uint16_t value) {
    return ntohs_nid_no_patch(value);
}

int APS5_VABI open_nid_disambig1_nid_postfix(const char* path, int flags, int mode) {
    return open_nid_no_patch(path, flags, mode);
}

int APS5_VABI openintr_nid_disambig1_nid_postfix(void) {
    return openintr_nid_no_patch();
}

int APS5_VABI pause_nid_disambig1_nid_postfix(void) {
    return pause_nid_no_patch();
}

int APS5_VABI physhm_open_nid_disambig1_nid_postfix(void) {
    return physhm_open_nid_no_patch();
}

int APS5_VABI physhm_unlink_nid_disambig1_nid_postfix(void) {
    return physhm_unlink_nid_no_patch();
}

int APS5_VABI pipe_nid_disambig1_nid_postfix(int* descriptors) {
    return pipe_nid_no_patch(descriptors);
}

int APS5_VABI poll_nid_disambig1_nid_postfix(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    return poll_nid_no_patch(descriptors, count, timeout);
}

int64_t APS5_VABI pread_nid_disambig1_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    return pread_nid_no_patch(d, buf, nbytes, offset);
}

int64_t APS5_VABI preadv_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return preadv_nid_no_patch(d, iov, iovcnt, offset);
}

int APS5_VABI profil_nid_disambig1_nid_postfix(void* samples, std::size_t size, std::size_t offset, unsigned int scale) {
    return profil_nid_no_patch(samples, size, offset, scale);
}

int APS5_VABI pselect_nid_disambig1_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask) {
    return pselect_nid_no_patch(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

int APS5_VABI pthread_attr_destroy_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return pthread_attr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_attr_get_np_nid_disambig1_nid_postfix(Pthread thread, PthreadAttr* attr) {
    return pthread_attr_get_np_nid_no_patch(thread, attr);
}

int APS5_VABI pthread_attr_getaffinity_np_nid_disambig1_nid_postfix(const PthreadAttr* attr, std::size_t cpusetSize, void* cpuset) {
    return pthread_attr_getaffinity_np_nid_no_patch(attr, cpusetSize, cpuset);
}

int APS5_VABI pthread_attr_getdetachstate_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* state) {
    return pthread_attr_getdetachstate_nid_no_patch(attr, state);
}

int APS5_VABI pthread_attr_getguardsize_nid_disambig1_nid_postfix(const PthreadAttr* attr, size_t* guard_size) {
    return pthread_attr_getguardsize_nid_no_patch(attr, guard_size);
}

int APS5_VABI pthread_attr_getinheritsched_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* inherit_sched) {
    return pthread_attr_getinheritsched_nid_no_patch(attr, inherit_sched);
}

int APS5_VABI pthread_attr_getschedparam_nid_disambig1_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
    return pthread_attr_getschedparam_nid_no_patch(attr, param);
}

int APS5_VABI pthread_attr_getschedpolicy_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* policy) {
    return pthread_attr_getschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI pthread_attr_getscope_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* scope) {
    return pthread_attr_getscope_nid_no_patch(attr, scope);
}

int APS5_VABI pthread_attr_getsolosched_np_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* solosched) {
    return pthread_attr_getsolosched_np_nid_no_patch(attr, solosched);
}

int APS5_VABI pthread_attr_getstack_nid_disambig1_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr, size_t* __restrict stack_size) {
    return pthread_attr_getstack_nid_no_patch(attr, stack_addr, stack_size);
}

int APS5_VABI pthread_attr_getstackaddr_nid_disambig1_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr) {
    return pthread_attr_getstackaddr_nid_no_patch(attr, stack_addr);
}

int APS5_VABI pthread_attr_getstacksize_nid_disambig1_nid_postfix(const PthreadAttr* attr, size_t* stack_size) {
    return pthread_attr_getstacksize_nid_no_patch(attr, stack_size);
}

int APS5_VABI pthread_attr_init_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return pthread_attr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_attr_setaffinity_np_nid_disambig1_nid_postfix(PthreadAttr* attr, std::size_t cpusetSize, const void* cpuset) {
    return pthread_attr_setaffinity_np_nid_no_patch(attr, cpusetSize, cpuset);
}

int APS5_VABI pthread_attr_setcreatesuspend_np_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return pthread_attr_setcreatesuspend_np_nid_no_patch(attr);
}

int APS5_VABI pthread_attr_setdetachstate_nid_disambig1_nid_postfix(PthreadAttr* attr, int state) {
    return pthread_attr_setdetachstate_nid_no_patch(attr, state);
}

int APS5_VABI pthread_attr_setguardsize_nid_disambig1_nid_postfix(PthreadAttr* attr, size_t guard_size) {
    return pthread_attr_setguardsize_nid_no_patch(attr, guard_size);
}

int APS5_VABI pthread_attr_setinheritsched_nid_disambig1_nid_postfix(PthreadAttr* attr, int inherit_sched) {
    return pthread_attr_setinheritsched_nid_no_patch(attr, inherit_sched);
}

int APS5_VABI pthread_attr_setschedparam_nid_disambig1_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    return pthread_attr_setschedparam_nid_no_patch(attr, param);
}

int APS5_VABI pthread_attr_setschedpolicy_nid_disambig1_nid_postfix(PthreadAttr* attr, int policy) {
    return pthread_attr_setschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI pthread_attr_setscope_nid_disambig1_nid_postfix(PthreadAttr* attr, int scope) {
    return pthread_attr_setscope_nid_no_patch(attr, scope);
}

int APS5_VABI pthread_attr_setsolosched_np_nid_disambig1_nid_postfix(PthreadAttr* attr, int solosched) {
    return pthread_attr_setsolosched_np_nid_no_patch(attr, solosched);
}

int APS5_VABI pthread_attr_setstack_nid_disambig1_nid_postfix(PthreadAttr* attr, void* stack_addr, size_t stack_size) {
    return pthread_attr_setstack_nid_no_patch(attr, stack_addr, stack_size);
}

int APS5_VABI pthread_attr_setstackaddr_nid_disambig1_nid_postfix(PthreadAttr* attr, void* stack_addr) {
    return pthread_attr_setstackaddr_nid_no_patch(attr, stack_addr);
}

int APS5_VABI pthread_attr_setstacksize_nid_disambig1_nid_postfix(PthreadAttr* attr, size_t stack_size) {
    return pthread_attr_setstacksize_nid_no_patch(attr, stack_size);
}

int APS5_VABI pthread_barrier_destroy_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier) {
    return pthread_barrier_destroy_nid_no_patch(barrier);
}

int APS5_VABI pthread_barrier_init_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count) {
    return pthread_barrier_init_nid_no_patch(barrier, attr, count);
}

int APS5_VABI pthread_barrier_setname_np_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier, const char* name) {
    return pthread_barrier_setname_np_nid_no_patch(barrier, name);
}

int APS5_VABI pthread_barrier_wait_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier) {
    return pthread_barrier_wait_nid_no_patch(barrier);
}

int APS5_VABI pthread_barrierattr_destroy_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate** attr) {
    return pthread_barrierattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_barrierattr_getpshared_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    return pthread_barrierattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_barrierattr_init_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate** attr) {
    return pthread_barrierattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_barrierattr_setpshared_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate** attr, int pshared) {
    return pthread_barrierattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_cancel_nid_disambig1_nid_postfix(Pthread thread) {
    return pthread_cancel_nid_no_patch(thread);
}

void APS5_VABI pthread_cleanup_pop_nid_disambig1_nid_postfix(int execute) {
    pthread_cleanup_pop_nid_no_patch(execute);
}

void APS5_VABI pthread_cleanup_push_nid_disambig1_nid_postfix(void (APS5_VABI* routine)(void*), void* argument) {
    pthread_cleanup_push_nid_no_patch(routine, argument);
}

int APS5_VABI pthread_cond_broadcast_nid_disambig1_nid_postfix(PthreadCond* cond) {
    return pthread_cond_broadcast_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_destroy_nid_disambig1_nid_postfix(PthreadCond* cond) {
    return pthread_cond_destroy_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_init_nid_disambig1_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    return pthread_cond_init_nid_no_patch(cond, attr);
}

int APS5_VABI pthread_cond_reltimedwait_np_nid_disambig1_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return pthread_cond_reltimedwait_np_nid_no_patch(cond, mutex, usec);
}

int APS5_VABI pthread_cond_setname_np_nid_disambig1_nid_postfix(PthreadCond* cond, const char* name) {
    return pthread_cond_setname_np_nid_no_patch(cond, name);
}

int APS5_VABI pthread_cond_signal_nid_disambig1_nid_postfix(PthreadCond* cond) {
    return pthread_cond_signal_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_signalto_np_nid_disambig1_nid_postfix(PthreadCond* cond, Pthread thread) {
    return pthread_cond_signalto_np_nid_no_patch(cond, thread);
}

int APS5_VABI pthread_cond_timedwait_nid_disambig1_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    return pthread_cond_timedwait_nid_no_patch(cond, mutex, abstime);
}

int APS5_VABI pthread_cond_wait_nid_disambig1_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    return pthread_cond_wait_nid_no_patch(cond, mutex);
}

int APS5_VABI pthread_condattr_destroy_nid_disambig1_nid_postfix(PthreadCondattr* attr) {
    return pthread_condattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_condattr_getclock_nid_disambig1_nid_postfix(const PthreadCondattr* attr, KernelClockid* clockId) {
    return pthread_condattr_getclock_nid_no_patch(attr, clockId);
}

int APS5_VABI pthread_condattr_getpshared_nid_disambig1_nid_postfix(const PthreadCondattr* attr, int* pshared) {
    return pthread_condattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_condattr_init_nid_disambig1_nid_postfix(PthreadCondattr* attr) {
    return pthread_condattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_condattr_setclock_nid_disambig1_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
    return pthread_condattr_setclock_nid_no_patch(attr, clock_id);
}

int APS5_VABI pthread_condattr_setpshared_nid_disambig1_nid_postfix(PthreadCondattr* attr, int pshared) {
    return pthread_condattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_create_nid_disambig1_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg) {
    return pthread_create_nid_no_patch(thread, attr, entry, arg);
}

int APS5_VABI pthread_create_name_np_nid_disambig1_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return pthread_create_name_np_nid_no_patch(thread, attr, entry, arg, name);
}

int APS5_VABI pthread_detach_nid_disambig1_nid_postfix(Pthread thread) {
    return pthread_detach_nid_no_patch(thread);
}

int APS5_VABI pthread_equal_nid_disambig1_nid_postfix(Pthread first, Pthread second) {
    return pthread_equal_nid_no_patch(first, second);
}

void APS5_VABI pthread_exit_nid_disambig1_nid_postfix(void* value) {
    pthread_exit_nid_no_patch(value);
}

int APS5_VABI pthread_get_specificarray_np_nid_disambig1_nid_postfix(void) {
    return pthread_get_specificarray_np_nid_no_patch();
}

int APS5_VABI pthread_get_user_context_np_nid_disambig1_nid_postfix(void) {
    return pthread_get_user_context_np_nid_no_patch();
}

int APS5_VABI pthread_getaffinity_np_nid_disambig1_nid_postfix(Pthread thread, std::size_t cpusetSize, void* cpuset) {
    return pthread_getaffinity_np_nid_no_patch(thread, cpusetSize, cpuset);
}

int APS5_VABI pthread_getconcurrency_nid_disambig1_nid_postfix(void) {
    return pthread_getconcurrency_nid_no_patch();
}

int APS5_VABI pthread_getcpuclockid_nid_disambig1_nid_postfix(Pthread thread, int* clockId) {
    return pthread_getcpuclockid_nid_no_patch(thread, clockId);
}

int APS5_VABI pthread_getname_np_nid_disambig1_nid_postfix(Pthread thread, char* name) {
    return pthread_getname_np_nid_no_patch(thread, name);
}

int APS5_VABI pthread_getprio_nid_disambig1_nid_postfix(Pthread thread) {
    return pthread_getprio_nid_no_patch(thread);
}

int APS5_VABI pthread_getschedparam_nid_disambig1_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    return pthread_getschedparam_nid_no_patch(thread, policy, param);
}

void* APS5_VABI pthread_getspecific_nid_disambig1_nid_postfix(PthreadKey key) {
    return pthread_getspecific_nid_no_patch(key);
}

int APS5_VABI pthread_getstack_np_nid_disambig1_nid_postfix(void) {
    return pthread_getstack_np_nid_no_patch();
}

int APS5_VABI pthread_getthreadid_np_nid_disambig1_nid_postfix(void) {
    return pthread_getthreadid_np_nid_no_patch();
}

int APS5_VABI pthread_join_nid_disambig1_nid_postfix(Pthread thread, void** value) {
    return pthread_join_nid_no_patch(thread, value);
}

int APS5_VABI pthread_key_create_nid_disambig1_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return pthread_key_create_nid_no_patch(key, destructor);
}

int APS5_VABI pthread_key_delete_nid_disambig1_nid_postfix(PthreadKey key) {
    return pthread_key_delete_nid_no_patch(key);
}

int APS5_VABI pthread_kill_nid_disambig1_nid_postfix(Pthread thread, int guest) {
    return pthread_kill_nid_no_patch(thread, guest);
}

int APS5_VABI pthread_main_np_nid_disambig1_nid_postfix(void) {
    return pthread_main_np_nid_no_patch();
}

int APS5_VABI pthread_multi_np_nid_disambig1_nid_postfix(void) {
    return pthread_multi_np_nid_no_patch();
}

int APS5_VABI pthread_mutex_destroy_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_destroy_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_getprioceiling_nid_disambig1_nid_postfix(const PthreadMutex* mutex, int* prioceiling) {
    return pthread_mutex_getprioceiling_nid_no_patch(mutex, prioceiling);
}

int APS5_VABI pthread_mutex_getspinloops_np_nid_disambig1_nid_postfix(PthreadMutex* mutex, int* count) {
    return pthread_mutex_getspinloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_getyieldloops_np_nid_disambig1_nid_postfix(PthreadMutex* mutex, int* count) {
    return pthread_mutex_getyieldloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_init_nid_disambig1_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return pthread_mutex_init_nid_no_patch(mutex, attr);
}

int APS5_VABI pthread_mutex_init_for_mono_nid_disambig1_nid_postfix(void) {
    return pthread_mutex_init_for_mono_nid_no_patch();
}

int APS5_VABI pthread_mutex_isowned_np_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_isowned_np_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_lock_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_lock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_reltimedlock_np_nid_disambig1_nid_postfix(PthreadMutex* mutex, KernelUseconds usec) {
    return pthread_mutex_reltimedlock_np_nid_no_patch(mutex, usec);
}

int APS5_VABI pthread_mutex_setname_np_nid_disambig1_nid_postfix(PthreadMutex* mutex, const char* name) {
    return pthread_mutex_setname_np_nid_no_patch(mutex, name);
}

int APS5_VABI pthread_mutex_setprioceiling_nid_disambig1_nid_postfix(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return pthread_mutex_setprioceiling_nid_no_patch(mutex, ceiling, oldCeiling);
}

int APS5_VABI pthread_mutex_setspinloops_np_nid_disambig1_nid_postfix(PthreadMutex* mutex, int count) {
    return pthread_mutex_setspinloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_setyieldloops_np_nid_disambig1_nid_postfix(PthreadMutex* mutex, int count) {
    return pthread_mutex_setyieldloops_np_nid_no_patch(mutex, count);
}

int APS5_VABI pthread_mutex_timedlock_nid_disambig1_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
    return pthread_mutex_timedlock_nid_no_patch(mutex, abstime);
}

int APS5_VABI pthread_mutex_trylock_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_trylock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_unlock_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_unlock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutexattr_destroy_nid_disambig1_nid_postfix(PthreadMutexattr* attr) {
    return pthread_mutexattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_mutexattr_getgen_np_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* generation) {
    return pthread_mutexattr_getgen_np_nid_no_patch(attr, generation);
}

int APS5_VABI pthread_mutexattr_getkind_np_nid_disambig1_nid_postfix(PthreadMutexattr attr) {
    return pthread_mutexattr_getkind_np_nid_no_patch(attr);
}

int APS5_VABI pthread_mutexattr_getprioceiling_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* prioceiling) {
    return pthread_mutexattr_getprioceiling_nid_no_patch(attr, prioceiling);
}

int APS5_VABI pthread_mutexattr_getprotocol_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* protocol) {
    return pthread_mutexattr_getprotocol_nid_no_patch(attr, protocol);
}

int APS5_VABI pthread_mutexattr_getpshared_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* pshared) {
    return pthread_mutexattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_mutexattr_gettype_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* type) {
    return pthread_mutexattr_gettype_nid_no_patch(attr, type);
}

int APS5_VABI pthread_mutexattr_init_nid_disambig1_nid_postfix(PthreadMutexattr* attr) {
    return pthread_mutexattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_mutexattr_setgen_np_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int generation) {
    return pthread_mutexattr_setgen_np_nid_no_patch(attr, generation);
}

int APS5_VABI pthread_mutexattr_setkind_np_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int kind) {
    return pthread_mutexattr_setkind_np_nid_no_patch(attr, kind);
}

int APS5_VABI pthread_mutexattr_setprioceiling_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int prioceiling) {
    return pthread_mutexattr_setprioceiling_nid_no_patch(attr, prioceiling);
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int protocol) {
    return pthread_mutexattr_setprotocol_nid_no_patch(attr, protocol);
}

int APS5_VABI pthread_mutexattr_setpshared_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int pshared) {
    return pthread_mutexattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_mutexattr_settype_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int type) {
    return pthread_mutexattr_settype_nid_no_patch(attr, type);
}

int APS5_VABI pthread_once_nid_disambig1_nid_postfix(GuestOnce* control, void (APS5_VABI *initialize)()) {
    return pthread_once_nid_no_patch(control, initialize);
}

int APS5_VABI pthread_rename_np_nid_disambig1_nid_postfix(Pthread thread, const char* name) {
    return pthread_rename_np_nid_no_patch(thread, name);
}

void APS5_VABI pthread_resume_all_np_nid_disambig1_nid_postfix(void) {
    pthread_resume_all_np_nid_no_patch();
}

int APS5_VABI pthread_resume_np_nid_disambig1_nid_postfix(Pthread thread) {
    return pthread_resume_np_nid_no_patch(thread);
}

int APS5_VABI pthread_resume_user_context_np_nid_disambig1_nid_postfix(void) {
    return pthread_resume_user_context_np_nid_no_patch();
}

int APS5_VABI pthread_rwlock_destroy_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_destroy_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_init_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    return pthread_rwlock_init_nid_no_patch(rwlock, attr);
}

int APS5_VABI pthread_rwlock_rdlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_rdlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_reltimedrdlock_np_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    return pthread_rwlock_reltimedrdlock_np_nid_no_patch(rwlock, usec);
}

int APS5_VABI pthread_rwlock_reltimedwrlock_np_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    return pthread_rwlock_reltimedwrlock_np_nid_no_patch(rwlock, usec);
}

int APS5_VABI pthread_rwlock_setname_np_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, const char* name) {
    return pthread_rwlock_setname_np_nid_no_patch(rwlock, name);
}

int APS5_VABI pthread_rwlock_timedrdlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    return pthread_rwlock_timedrdlock_nid_no_patch(rwlock, abstime);
}

int APS5_VABI pthread_rwlock_timedwrlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    return pthread_rwlock_timedwrlock_nid_no_patch(rwlock, abstime);
}

int APS5_VABI pthread_rwlock_tryrdlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_tryrdlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_trywrlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_trywrlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_unlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_unlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_wrlock_nid_disambig1_nid_postfix(PthreadRwlock* rwlock) {
    return pthread_rwlock_wrlock_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlockattr_destroy_nid_disambig1_nid_postfix(PthreadRwlockattr* attr) {
    return pthread_rwlockattr_destroy_nid_no_patch(attr);
}

int APS5_VABI pthread_rwlockattr_getpshared_nid_disambig1_nid_postfix(const PthreadRwlockattr* attr, int* pshared) {
    return pthread_rwlockattr_getpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_rwlockattr_gettype_np_nid_disambig1_nid_postfix(const PthreadRwlockattr* attr, int* type) {
    return pthread_rwlockattr_gettype_np_nid_no_patch(attr, type);
}

int APS5_VABI pthread_rwlockattr_init_nid_disambig1_nid_postfix(PthreadRwlockattr* attr) {
    return pthread_rwlockattr_init_nid_no_patch(attr);
}

int APS5_VABI pthread_rwlockattr_setpshared_nid_disambig1_nid_postfix(PthreadRwlockattr* attr, int pshared) {
    return pthread_rwlockattr_setpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI pthread_rwlockattr_settype_np_nid_disambig1_nid_postfix(PthreadRwlockattr* attr, int type) {
    return pthread_rwlockattr_settype_np_nid_no_patch(attr, type);
}

Pthread APS5_VABI pthread_self_nid_disambig1_nid_postfix(void) {
    return pthread_self_nid_no_patch();
}

int APS5_VABI pthread_set_defaultstacksize_np_nid_disambig1_nid_postfix(std::size_t size) {
    return pthread_set_defaultstacksize_np_nid_no_patch(size);
}

void APS5_VABI pthread_set_name_np_nid_disambig1_nid_postfix(Pthread thread, const char* name) {
    pthread_set_name_np_nid_no_patch(thread, name);
}

int APS5_VABI pthread_set_user_context_np_nid_disambig1_nid_postfix(void) {
    return pthread_set_user_context_np_nid_no_patch();
}

int APS5_VABI pthread_setaffinity_np_nid_disambig1_nid_postfix(Pthread thread, std::size_t cpusetSize, const void* cpuset) {
    return pthread_setaffinity_np_nid_no_patch(thread, cpusetSize, cpuset);
}

int APS5_VABI pthread_setcancelstate_nid_disambig1_nid_postfix(int state, int* old_state) {
    return pthread_setcancelstate_nid_no_patch(state, old_state);
}

int APS5_VABI pthread_setcanceltype_nid_disambig1_nid_postfix(int type, int* old_type) {
    return pthread_setcanceltype_nid_no_patch(type, old_type);
}

int APS5_VABI pthread_setconcurrency_nid_disambig1_nid_postfix(int newLevel) {
    return pthread_setconcurrency_nid_no_patch(newLevel);
}

int APS5_VABI pthread_setprio_nid_disambig1_nid_postfix(Pthread thread, int prio) {
    return pthread_setprio_nid_no_patch(thread, prio);
}

int APS5_VABI pthread_setschedparam_nid_disambig1_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    return pthread_setschedparam_nid_no_patch(thread, policy, param);
}

int APS5_VABI pthread_setspecific_nid_disambig1_nid_postfix(PthreadKey key, void* value) {
    return pthread_setspecific_nid_no_patch(key, value);
}

int APS5_VABI pthread_sigmask_nid_disambig1_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    return pthread_sigmask_nid_no_patch(how, set, previousSet);
}

int APS5_VABI pthread_single_np_nid_disambig1_nid_postfix(void) {
    return pthread_single_np_nid_no_patch();
}

int APS5_VABI pthread_spin_destroy_nid_disambig1_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_destroy_nid_no_patch(lock);
}

int APS5_VABI pthread_spin_init_nid_disambig1_nid_postfix(PthreadSpinlockPrivate** lock, int pshared) {
    return pthread_spin_init_nid_no_patch(lock, pshared);
}

int APS5_VABI pthread_spin_lock_nid_disambig1_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_lock_nid_no_patch(lock);
}

int APS5_VABI pthread_spin_trylock_nid_disambig1_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_trylock_nid_no_patch(lock);
}

int APS5_VABI pthread_spin_unlock_nid_disambig1_nid_postfix(PthreadSpinlockPrivate** lock) {
    return pthread_spin_unlock_nid_no_patch(lock);
}

int APS5_VABI pthread_suspend_all_np_nid_disambig1_nid_postfix(void) {
    return pthread_suspend_all_np_nid_no_patch();
}

int APS5_VABI pthread_suspend_np_nid_disambig1_nid_postfix(void) {
    return pthread_suspend_np_nid_no_patch();
}

int APS5_VABI pthread_suspend_user_context_np_nid_disambig1_nid_postfix(void) {
    return pthread_suspend_user_context_np_nid_no_patch();
}

int APS5_VABI pthread_switch_add_np_nid_disambig1_nid_postfix(void) {
    return pthread_switch_add_np_nid_no_patch();
}

int APS5_VABI pthread_switch_delete_np_nid_disambig1_nid_postfix(void) {
    return pthread_switch_delete_np_nid_no_patch();
}

void APS5_VABI pthread_testcancel_nid_disambig1_nid_postfix(void) {
    pthread_testcancel_nid_no_patch();
}

int APS5_VABI pthread_timedjoin_np_nid_disambig1_nid_postfix(Pthread thread, void** value, const KernelTimespec* abstime) {
    return pthread_timedjoin_np_nid_no_patch(thread, value, abstime);
}

void APS5_VABI pthread_yield_nid_disambig1_nid_postfix(void) {
    pthread_yield_nid_no_patch();
}

std::int64_t APS5_VABI pwrite_nid_disambig2_nid_postfix(int d, const void* buf, std::size_t nbytes, std::int64_t offset) {
    return pwrite_nid_no_patch(d, buf, nbytes, offset);
}

int64_t APS5_VABI pwritev_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return pwritev_nid_no_patch(d, iov, iovcnt, offset);
}

int APS5_VABI raise_nid_disambig1_nid_postfix(int guest) {
    return raise_nid_no_patch(guest);
}

int64_t APS5_VABI read_nid_disambig1_nid_postfix(int d, void* buf, uint64_t nbytes) {
    return read_nid_no_patch(d, buf, nbytes);
}

int64_t APS5_VABI readv_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return readv_nid_no_patch(d, iov, iovcnt);
}

std::int64_t APS5_VABI recv_nid_disambig1_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags) {
    return recv_nid_no_patch(descriptor, buffer, length, flags);
}

std::int64_t APS5_VABI recvfrom_nid_disambig1_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags, void* address, std::uint32_t* addressLength) {
    return recvfrom_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

std::int64_t APS5_VABI recvmsg_nid_disambig1_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    return recvmsg_nid_no_patch(descriptor, message, flags);
}

int APS5_VABI rfork_thread_nid_disambig1_nid_postfix(int flags, void* stack, int (APS5_VABI* function)(void*), void* argument) {
    return rfork_thread_nid_no_patch(flags, stack, function, argument);
}

int APS5_VABI rmdir_nid_disambig1_nid_postfix(const char* path) {
    return rmdir_nid_no_patch(path);
}

int APS5_VABI rtld_printf_nid_disambig1_nid_postfix(void) {
    return rtld_printf_nid_no_patch();
}

int APS5_VABI rtprio_thread_nid_disambig1_nid_postfix(int function, int lwpid, GuestRtPrio* priority) {
    return rtprio_thread_nid_no_patch(function, lwpid, priority);
}

int APS5_VABI sceCoredumpAttachMemoryRegion_nid_disambig1_nid_postfix(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachMemoryRegion_nid_no_patch(userValue, mem, size);
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile_nid_disambig1_nid_postfix(std::uint32_t userValue, const void* mem, std::int32_t size, const char* name) {
    return sceCoredumpAttachMemoryRegionAsUserFile_nid_no_patch(userValue, mem, size, name);
}

int APS5_VABI sceCoredumpAttachUserFile_nid_disambig1_nid_postfix(std::uint32_t userValue, const char* path) {
    return sceCoredumpAttachUserFile_nid_no_patch(userValue, path);
}

int APS5_VABI sceCoredumpAttachUserMemoryFile_nid_disambig1_nid_postfix(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachUserMemoryFile_nid_no_patch(userValue, mem, size);
}

int APS5_VABI sceCoredumpConfigDumpMode_nid_disambig1_nid_postfix(int mode) {
    return sceCoredumpConfigDumpMode_nid_no_patch(mode);
}

int APS5_VABI sceCoredumpDebugForceCoredumpOnAppClose_nid_disambig1_nid_postfix(void) {
    return sceCoredumpDebugForceCoredumpOnAppClose_nid_no_patch();
}

void APS5_VABI sceCoredumpDebugTextOut_nid_disambig1_nid_postfix(const char* str, int len) {
    sceCoredumpDebugTextOut_nid_no_patch(str, len);
}

int APS5_VABI sceCoredumpDebugTriggerCoredump_nid_disambig1_nid_postfix(void) {
    return sceCoredumpDebugTriggerCoredump_nid_no_patch();
}

int APS5_VABI sceCoredumpGetStopInfoCpu_nid_disambig1_nid_postfix(void* info, size_t size) {
    return sceCoredumpGetStopInfoCpu_nid_no_patch(info, size);
}

int APS5_VABI sceCoredumpGetStopInfoGpu_nid_disambig1_nid_postfix(void) {
    return sceCoredumpGetStopInfoGpu_nid_no_patch();
}

int APS5_VABI sceCoredumpGetThreadContextInfo_nid_disambig1_nid_postfix(void* threadContextInfo, size_t threadContextInfoSize) {
    return sceCoredumpGetThreadContextInfo_nid_no_patch(threadContextInfo, threadContextInfoSize);
}

int APS5_VABI sceCoredumpInternalGetThreadContextInfo_nid_disambig1_nid_postfix(void) {
    return sceCoredumpInternalGetThreadContextInfo_nid_no_patch();
}

int APS5_VABI sceCoredumpRegisterCoredumpHandler_nid_disambig1_nid_postfix(uint64_t handler, size_t stack_size, uint64_t context) {
    return sceCoredumpRegisterCoredumpHandler_nid_no_patch(handler, stack_size, context);
}

int APS5_VABI sceCoredumpSetUserDataType_nid_disambig1_nid_postfix(std::int32_t type) {
    return sceCoredumpSetUserDataType_nid_no_patch(type);
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler_nid_disambig1_nid_postfix(void) {
    return sceCoredumpUnregisterCoredumpHandler_nid_no_patch();
}

int64_t APS5_VABI sceCoredumpWriteUserData_nid_disambig1_nid_postfix(const void* data, size_t size) {
    return sceCoredumpWriteUserData_nid_no_patch(data, size);
}

int APS5_VABI sceKernelAddAmprEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprEvent_nid_no_patch(eq, id, udata);
}

int APS5_VABI sceKernelAddAmprSystemEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprSystemEvent_nid_no_patch(eq, id, udata);
}

int APS5_VABI sceKernelAddCpumodeEvent_nid_disambig1_nid_postfix(void) {
    return sceKernelAddCpumodeEvent_nid_no_patch();
}

int APS5_VABI sceKernelAddExternalTriggerEvent_nid_disambig1_nid_postfix(void) {
    return sceKernelAddExternalTriggerEvent_nid_no_patch();
}

int APS5_VABI sceKernelAddFileEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd, int watch, void* udata) {
    return sceKernelAddFileEvent_nid_no_patch(eq, fd, watch, udata);
}

int APS5_VABI sceKernelAddGpuExceptionEvent_nid_disambig1_nid_postfix(void) {
    return sceKernelAddGpuExceptionEvent_nid_no_patch();
}

int APS5_VABI sceKernelAddHRTimerEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
    return sceKernelAddHRTimerEvent_nid_no_patch(eq, id, ts, udata);
}

int APS5_VABI sceKernelAddProcessToCanvasMap_nid_disambig1_nid_postfix(void) {
    return sceKernelAddProcessToCanvasMap_nid_no_patch();
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

int APS5_VABI sceKernelAllocateDirectMemory_nid_disambig1_nid_postfix(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
    return sceKernelAllocateDirectMemory_nid_no_patch(search_start, search_end, len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAllocateDirectMemory2_nid_disambig1_nid_postfix(void) {
    return sceKernelAllocateDirectMemory2_nid_no_patch();
}

int APS5_VABI sceKernelAllocateDirectMemoryForApp_nid_disambig1_nid_postfix(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
    return sceKernelAllocateDirectMemoryForApp_nid_no_patch(search_start, search_end, len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAllocateDirectMemoryForMiniApp_nid_disambig1_nid_postfix(void) {
    return sceKernelAllocateDirectMemoryForMiniApp_nid_no_patch();
}

int APS5_VABI sceKernelAllocateMainDirectMemory_nid_disambig1_nid_postfix(size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
    return sceKernelAllocateMainDirectMemory_nid_no_patch(len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAllocateToolMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelAllocateToolMemory_nid_no_patch();
}

int APS5_VABI sceKernelAllocateTraceDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelAllocateTraceDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelAvailableDirectMemorySize_nid_disambig1_nid_postfix(int64_t search_start, int64_t search_end, size_t alignment, int64_t* phys_addr_out, size_t* size_out) {
    return sceKernelAvailableDirectMemorySize_nid_no_patch(search_start, search_end, alignment, phys_addr_out, size_out);
}

int APS5_VABI sceKernelAvailableFlexibleMemorySize_nid_disambig1_nid_postfix(size_t* size) {
    return sceKernelAvailableFlexibleMemorySize_nid_no_patch(size);
}

int APS5_VABI sceKernelAvailableToolMemorySize_nid_disambig1_nid_postfix(void) {
    return sceKernelAvailableToolMemorySize_nid_no_patch();
}

int APS5_VABI sceKernelBacktraceSelf_nid_disambig1_nid_postfix(void) {
    return sceKernelBacktraceSelf_nid_no_patch();
}

int APS5_VABI sceKernelBatchMap_nid_disambig1_nid_postfix(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out) {
    return sceKernelBatchMap_nid_no_patch(entries, num_entries, num_entries_out);
}

int APS5_VABI sceKernelBatchMap2_nid_disambig1_nid_postfix(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out, int flags) {
    return sceKernelBatchMap2_nid_no_patch(entries, num_entries, num_entries_out, flags);
}

int APS5_VABI sceKernelCallIndirectBuffer_nid_disambig1_nid_postfix(std::uint32_t priority, void* base, std::uint64_t bytes) {
    return sceKernelCallIndirectBuffer_nid_no_patch(priority, base, bytes);
}

int APS5_VABI sceKernelCallIndirectBuffer2_nid_disambig1_nid_postfix(std::uint32_t priority, void* base, std::uint64_t bytes, KernelMapperSubmitResult* result, std::uint32_t* id) {
    return sceKernelCallIndirectBuffer2_nid_no_patch(priority, base, bytes, result, id);
}

int APS5_VABI sceKernelCallIndirectBuffer3_nid_disambig1_nid_postfix(std::uint32_t priority, void* base, std::uint64_t bytes, std::uint32_t* id) {
    return sceKernelCallIndirectBuffer3_nid_no_patch(priority, base, bytes, id);
}

int APS5_VABI sceKernelCancelEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
    return sceKernelCancelEventFlag_nid_no_patch(ef, set_pattern, num_wait_threads);
}

int APS5_VABI sceKernelCancelSema_nid_disambig1_nid_postfix(KernelSema sem, int count, int* threads) {
    return sceKernelCancelSema_nid_no_patch(sem, count, threads);
}

int APS5_VABI sceKernelCheckProsperoSdkVersion_nid_disambig1_nid_postfix(void) {
    return sceKernelCheckProsperoSdkVersion_nid_no_patch();
}

int APS5_VABI sceKernelCheckReachability_nid_disambig1_nid_postfix(const char* path) {
    return sceKernelCheckReachability_nid_no_patch(path);
}

int APS5_VABI sceKernelCheckedReleaseDirectMemory_nid_disambig1_nid_postfix(int64_t start, size_t len) {
    return sceKernelCheckedReleaseDirectMemory_nid_no_patch(start, len);
}

int APS5_VABI sceKernelChmod_nid_disambig1_nid_postfix(const char* path, std::uint16_t mode) {
    return sceKernelChmod_nid_no_patch(path, mode);
}

int APS5_VABI sceKernelClearBootReqNotifyCount_nid_disambig1_nid_postfix(void) {
    return sceKernelClearBootReqNotifyCount_nid_no_patch();
}

int APS5_VABI sceKernelClearEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bit_pattern) {
    return sceKernelClearEventFlag_nid_no_patch(ef, bit_pattern);
}

int APS5_VABI sceKernelClearGameDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelClearGameDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelClockGetres_nid_disambig1_nid_postfix(KernelClockid clock_id, KernelTimespec* tp) {
    return sceKernelClockGetres_nid_no_patch(clock_id, tp);
}

int APS5_VABI sceKernelClockGettime_nid_disambig1_nid_postfix(KernelClockid clock_id, KernelTimespec* tp) {
    return sceKernelClockGettime_nid_no_patch(clock_id, tp);
}

int APS5_VABI sceKernelClose_nid_disambig1_nid_postfix(int d) {
    return sceKernelClose_nid_no_patch(d);
}

int APS5_VABI sceKernelCloseEport_nid_disambig1_nid_postfix(void) {
    return sceKernelCloseEport_nid_no_patch();
}

int APS5_VABI sceKernelCloseEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef) {
    return sceKernelCloseEventFlag_nid_no_patch(ef);
}

int APS5_VABI sceKernelCloseSema_nid_disambig1_nid_postfix(void) {
    return sceKernelCloseSema_nid_no_patch();
}

int APS5_VABI sceKernelConfiguredFlexibleMemorySize_nid_disambig1_nid_postfix(size_t* size) {
    return sceKernelConfiguredFlexibleMemorySize_nid_no_patch(size);
}

int APS5_VABI sceKernelConvertLocaltimeToUtc_nid_disambig1_nid_postfix(int64_t local_time, int64_t reserved, int64_t* utc_time, KernelTimesec* timezone, int32_t* dst_seconds) {
    return sceKernelConvertLocaltimeToUtc_nid_no_patch(local_time, reserved, utc_time, timezone, dst_seconds);
}

int APS5_VABI sceKernelConvertUtcToLocaltime_nid_disambig1_nid_postfix(int64_t utc_time, int64_t* local_time, KernelTimesec* st, int32_t* dst_sec) {
    return sceKernelConvertUtcToLocaltime_nid_no_patch(utc_time, local_time, st, dst_sec);
}

int APS5_VABI sceKernelCreateEport_nid_disambig1_nid_postfix(void) {
    return sceKernelCreateEport_nid_no_patch();
}

int APS5_VABI sceKernelCreateEqueue_nid_disambig1_nid_postfix(KernelEqueue* eq, const char* name) {
    return sceKernelCreateEqueue_nid_no_patch(eq, name);
}

int APS5_VABI sceKernelCreateEventFlag_nid_disambig1_nid_postfix(KernelEventFlag* ef, const char* name, uint32_t attr, uint64_t init_pattern, const void* param) {
    return sceKernelCreateEventFlag_nid_no_patch(ef, name, attr, init_pattern, param);
}

int APS5_VABI sceKernelCreateSema_nid_disambig1_nid_postfix(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
    return sceKernelCreateSema_nid_no_patch(sem, name, attr, init, max, opt);
}

int APS5_VABI sceKernelDebugAcquireAndUpdateDebugRegister_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugAcquireAndUpdateDebugRegister_nid_no_patch();
}

int APS5_VABI sceKernelDebugGetAppStatus_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugGetAppStatus_nid_no_patch();
}

int APS5_VABI sceKernelDebugGetPauseCount_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugGetPauseCount_nid_no_patch();
}

int APS5_VABI sceKernelDebugGpuPaDebugIsInProgress_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugGpuPaDebugIsInProgress_nid_no_patch();
}

int APS5_VABI sceKernelDebugOutText_nid_disambig1_nid_postfix(int channel, const char* text) {
    return sceKernelDebugOutText_nid_no_patch(channel, text);
}

int APS5_VABI sceKernelDebugPauseWithDebugContext_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugPauseWithDebugContext_nid_no_patch();
}

void APS5_VABI sceKernelDebugRaiseException_nid_disambig1_nid_postfix(int c1, int c2) {
    sceKernelDebugRaiseException_nid_no_patch(c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode_nid_disambig1_nid_postfix(int c1, int c2) {
    sceKernelDebugRaiseExceptionOnReleaseMode_nid_no_patch(c1, c2);
}

int APS5_VABI sceKernelDebugRaiseExceptionWithContext_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugRaiseExceptionWithContext_nid_no_patch();
}

int APS5_VABI sceKernelDebugRaiseExceptionWithInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugRaiseExceptionWithInfo_nid_no_patch();
}

int APS5_VABI sceKernelDebugReleaseDebugContext_nid_disambig1_nid_postfix(void) {
    return sceKernelDebugReleaseDebugContext_nid_no_patch();
}

int APS5_VABI sceKernelDebugWriteCppExceptionInfo_nid_disambig1_nid_postfix(const void* exception, uint64_t unknown, const char* typeName, const char* what) {
    return sceKernelDebugWriteCppExceptionInfo_nid_no_patch(exception, unknown, typeName, what);
}

int APS5_VABI sceKernelDeleteAmprEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelDeleteAmprSystemEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprSystemEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelDeleteCpumodeEvent_nid_disambig1_nid_postfix(void) {
    return sceKernelDeleteCpumodeEvent_nid_no_patch();
}

int APS5_VABI sceKernelDeleteEport_nid_disambig1_nid_postfix(void) {
    return sceKernelDeleteEport_nid_no_patch();
}

int APS5_VABI sceKernelDeleteEqueue_nid_disambig1_nid_postfix(KernelEqueue eq) {
    return sceKernelDeleteEqueue_nid_no_patch(eq);
}

int APS5_VABI sceKernelDeleteEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef) {
    return sceKernelDeleteEventFlag_nid_no_patch(ef);
}

int APS5_VABI sceKernelDeleteExternalTriggerEvent_nid_disambig1_nid_postfix(void) {
    return sceKernelDeleteExternalTriggerEvent_nid_no_patch();
}

int APS5_VABI sceKernelDeleteFileEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int fd) {
    return sceKernelDeleteFileEvent_nid_no_patch(eq, fd);
}

int APS5_VABI sceKernelDeleteGpuExceptionEvent_nid_disambig1_nid_postfix(void) {
    return sceKernelDeleteGpuExceptionEvent_nid_no_patch();
}

int APS5_VABI sceKernelDeleteHRTimerEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id) {
    return sceKernelDeleteHRTimerEvent_nid_no_patch(eq, id);
}

int APS5_VABI sceKernelDeleteProcessFromCanvasMap_nid_disambig1_nid_postfix(void) {
    return sceKernelDeleteProcessFromCanvasMap_nid_no_patch();
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

int APS5_VABI sceKernelDirectMemoryQuery_nid_disambig1_nid_postfix(int64_t offset, int flags, void* info, size_t info_size) {
    return sceKernelDirectMemoryQuery_nid_no_patch(offset, flags, info, info_size);
}

int APS5_VABI sceKernelDirectMemoryQueryForId_nid_disambig1_nid_postfix(void) {
    return sceKernelDirectMemoryQueryForId_nid_no_patch();
}

int APS5_VABI sceKernelDlsym_nid_disambig1_nid_postfix(KernelModule handle, const char* symbol, void** addr) {
    return sceKernelDlsym_nid_no_patch(handle, symbol, addr);
}

int APS5_VABI sceKernelEnableDmemAliasing_nid_disambig1_nid_postfix(void) {
    return sceKernelEnableDmemAliasing_nid_no_patch();
}

int APS5_VABI sceKernelEnableDmemAliasing2_nid_disambig1_nid_postfix(void) {
    return sceKernelEnableDmemAliasing2_nid_no_patch();
}

int APS5_VABI sceKernelEnablePthreadObjectCheck_nid_disambig1_nid_postfix(void) {
    return sceKernelEnablePthreadObjectCheck_nid_no_patch();
}

std::int32_t APS5_VABI sceKernelError_nid_disambig1_nid_postfix(std::int32_t posixError) {
    return sceKernelError_nid_no_patch(posixError);
}

int APS5_VABI sceKernelEventLogInit_nid_disambig1_nid_postfix(void) {
    return sceKernelEventLogInit_nid_no_patch();
}

int APS5_VABI sceKernelEventLogPread_nid_disambig1_nid_postfix(void) {
    return sceKernelEventLogPread_nid_no_patch();
}

int APS5_VABI sceKernelEventLogRead_nid_disambig1_nid_postfix(void) {
    return sceKernelEventLogRead_nid_no_patch();
}

int APS5_VABI sceKernelEventLogWrite_nid_disambig1_nid_postfix(void) {
    return sceKernelEventLogWrite_nid_no_patch();
}

int APS5_VABI sceKernelExtendMaximumModuleNumber_nid_disambig1_nid_postfix(void) {
    return sceKernelExtendMaximumModuleNumber_nid_no_patch();
}

int APS5_VABI sceKernelFchmod_nid_disambig1_nid_postfix(int d, std::uint16_t mode) {
    return sceKernelFchmod_nid_no_patch(d, mode);
}

int APS5_VABI sceKernelFcntl_nid_disambig1_nid_postfix(int d, int command, std::uint64_t v0, std::uint64_t v1) {
    return sceKernelFcntl_nid_no_patch(d, command, v0, v1);
}

int APS5_VABI sceKernelFdatasync_nid_disambig1_nid_postfix(int fd) {
    return sceKernelFdatasync_nid_no_patch(fd);
}

int APS5_VABI sceKernelFlock_nid_disambig1_nid_postfix(void) {
    return sceKernelFlock_nid_no_patch();
}

int APS5_VABI sceKernelFstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    return sceKernelFstat_nid_no_patch(d, sb);
}

int APS5_VABI sceKernelFsync_nid_disambig1_nid_postfix(int fd) {
    return sceKernelFsync_nid_no_patch(fd);
}

int APS5_VABI sceKernelFtruncate_nid_disambig1_nid_postfix(int d, int64_t length) {
    return sceKernelFtruncate_nid_no_patch(d, length);
}

int APS5_VABI sceKernelFutimes_nid_disambig1_nid_postfix(int d, const KernelTimeval* times) {
    return sceKernelFutimes_nid_no_patch(d, times);
}

int APS5_VABI sceKernelGetAllowedSdkVersionOnSystem_nid_disambig1_nid_postfix(std::uint32_t* version) {
    return sceKernelGetAllowedSdkVersionOnSystem_nid_no_patch(version);
}

int APS5_VABI sceKernelGetAmprCounter_nid_disambig1_nid_postfix(void) {
    return sceKernelGetAmprCounter_nid_no_patch();
}

int APS5_VABI sceKernelGetAppCategoryType_nid_disambig1_nid_postfix(int pid, std::uint32_t* category) {
    return sceKernelGetAppCategoryType_nid_no_patch(pid, category);
}

int APS5_VABI sceKernelGetAppInfo_nid_disambig1_nid_postfix(int pid, void* info) {
    return sceKernelGetAppInfo_nid_no_patch(pid, info);
}

int APS5_VABI sceKernelGetAppLibcData_nid_disambig1_nid_postfix(void) {
    return sceKernelGetAppLibcData_nid_no_patch();
}

int APS5_VABI sceKernelGetAslrStatus_nid_disambig1_nid_postfix(void) {
    return sceKernelGetAslrStatus_nid_no_patch();
}

int APS5_VABI sceKernelGetAutoUpdateVersion_nid_disambig1_nid_postfix(void) {
    return sceKernelGetAutoUpdateVersion_nid_no_patch();
}

int APS5_VABI sceKernelGetBackupRestoreMode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBackupRestoreMode_nid_no_patch();
}

int APS5_VABI sceKernelGetBackupRestoreModeOfNextBoot_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBackupRestoreModeOfNextBoot_nid_no_patch();
}

int APS5_VABI sceKernelGetBasicProductShape_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBasicProductShape_nid_no_patch();
}

int APS5_VABI sceKernelGetBetaUpdateTestForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBetaUpdateTestForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetBioUsageAll_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBioUsageAll_nid_no_patch();
}

int APS5_VABI sceKernelGetBootReqNotifyCount_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBootReqNotifyCount_nid_no_patch();
}

int APS5_VABI sceKernelGetBuildNumber_nid_disambig1_nid_postfix(void) {
    return sceKernelGetBuildNumber_nid_no_patch();
}

int APS5_VABI sceKernelGetCallRecord_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCallRecord_nid_no_patch();
}

int APS5_VABI sceKernelGetCanvasHandle_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCanvasHandle_nid_no_patch();
}

int APS5_VABI sceKernelGetCanvasHandleByPid_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCanvasHandleByPid_nid_no_patch();
}

int APS5_VABI sceKernelGetCompiledSdkVersion_nid_disambig1_nid_postfix(std::int32_t* version) {
    return sceKernelGetCompiledSdkVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetCompiledSdkVersionByPath_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCompiledSdkVersionByPath_nid_no_patch();
}

int APS5_VABI sceKernelGetCompiledSdkVersionByPid_nid_disambig1_nid_postfix(int pid, std::int32_t* version) {
    return sceKernelGetCompiledSdkVersionByPid_nid_no_patch(pid, version);
}

int APS5_VABI sceKernelGetCpuCoreClock_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuCoreClock_nid_no_patch();
}

std::int64_t APS5_VABI sceKernelGetCpuFrequency_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuFrequency_nid_no_patch();
}

int APS5_VABI sceKernelGetCpuTemperature_nid_disambig1_nid_postfix(int* outCelsius) {
    return sceKernelGetCpuTemperature_nid_no_patch(outCelsius);
}

int APS5_VABI sceKernelGetCpuUsage_nid_disambig1_nid_postfix(void* stats, int32_t* size) {
    return sceKernelGetCpuUsage_nid_no_patch(stats, size);
}

int APS5_VABI sceKernelGetCpuUsageAll_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuUsageAll_nid_no_patch();
}

int APS5_VABI sceKernelGetCpuUsageProc_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuUsageProc_nid_no_patch();
}

int APS5_VABI sceKernelGetCpuUsageProc2_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuUsageProc2_nid_no_patch();
}

int APS5_VABI sceKernelGetCpuUsageThread_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpuUsageThread_nid_no_patch();
}

int APS5_VABI sceKernelGetCpumode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCpumode_nid_no_patch();
}

int APS5_VABI sceKernelGetCpumodeGame_nid_disambig1_nid_postfix(int* mode) {
    return sceKernelGetCpumodeGame_nid_no_patch(mode);
}

int APS5_VABI sceKernelGetCurrentCpu_nid_disambig1_nid_postfix(void) {
    return sceKernelGetCurrentCpu_nid_no_patch();
}

int APS5_VABI sceKernelGetDataTransferMode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDataTransferMode_nid_no_patch();
}

int APS5_VABI sceKernelGetDebugMenuMiniModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDebugMenuMiniModeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetDebugMenuModeForPsmForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDebugMenuModeForPsmForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetDebugMenuModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDebugMenuModeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetDefaultToolMemorySize_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDefaultToolMemorySize_nid_no_patch();
}

size_t APS5_VABI sceKernelGetDirectMemorySize_nid_disambig1_nid_postfix(void) {
    return sceKernelGetDirectMemorySize_nid_no_patch();
}

int APS5_VABI sceKernelGetDirectMemoryType_nid_disambig1_nid_postfix(int64_t offset, int* memory_type, int64_t* start, int64_t* end) {
    return sceKernelGetDirectMemoryType_nid_no_patch(offset, memory_type, start, end);
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

int APS5_VABI sceKernelGetExtLibcHandle_nid_disambig1_nid_postfix(void) {
    return sceKernelGetExtLibcHandle_nid_no_patch();
}

int APS5_VABI sceKernelGetFakeFinalizeMenuForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetFakeFinalizeMenuForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetFlagedUpdaterForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetFlagedUpdaterForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetForceUpdateModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetForceUpdateModeForRcmgr_nid_no_patch();
}

const char* APS5_VABI sceKernelGetFsSandboxRandomWord_nid_disambig1_nid_postfix(void) {
    return sceKernelGetFsSandboxRandomWord_nid_no_patch();
}

std::uint64_t APS5_VABI sceKernelGetGPI_nid_disambig1_nid_postfix(void) {
    return sceKernelGetGPI_nid_no_patch();
}

int APS5_VABI sceKernelGetGPO_nid_disambig1_nid_postfix(void) {
    return sceKernelGetGPO_nid_no_patch();
}

int APS5_VABI sceKernelGetGenuineProsperoSystemSwVersion_nid_disambig1_nid_postfix(void) {
    return sceKernelGetGenuineProsperoSystemSwVersion_nid_no_patch();
}

int APS5_VABI sceKernelGetHwFeatureInfoForDecid_nid_disambig1_nid_postfix(void) {
    return sceKernelGetHwFeatureInfoForDecid_nid_no_patch();
}

int APS5_VABI sceKernelGetIdPs_nid_disambig1_nid_postfix(void) {
    return sceKernelGetIdPs_nid_no_patch();
}

int APS5_VABI sceKernelGetIdTableCurrentCount_nid_disambig1_nid_postfix(void) {
    return sceKernelGetIdTableCurrentCount_nid_no_patch();
}

int APS5_VABI sceKernelGetIpcPath_nid_disambig1_nid_postfix(void) {
    return sceKernelGetIpcPath_nid_no_patch();
}

int APS5_VABI sceKernelGetKnobs_nid_disambig1_nid_postfix(void) {
    return sceKernelGetKnobs_nid_no_patch();
}

std::int32_t APS5_VABI sceKernelGetLibkernelTextLocation_nid_disambig1_nid_postfix(void** address, std::size_t* size) {
    return sceKernelGetLibkernelTextLocation_nid_no_patch(address, size);
}

int APS5_VABI sceKernelGetMainSocId_nid_disambig1_nid_postfix(void) {
    return sceKernelGetMainSocId_nid_no_patch();
}

int APS5_VABI sceKernelGetModuleInfo_nid_disambig1_nid_postfix(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo_nid_no_patch(handle, info);
}

int APS5_VABI sceKernelGetModuleInfo2_nid_disambig1_nid_postfix(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo2_nid_no_patch(handle, info);
}

int APS5_VABI sceKernelGetModuleInfoForUnwind_nid_disambig1_nid_postfix(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    return sceKernelGetModuleInfoForUnwind_nid_no_patch(addr, flags, info);
}

int APS5_VABI sceKernelGetModuleInfoFromAddr_nid_disambig1_nid_postfix(std::uint64_t address, int flags, ModuleInfoEx* info) {
    return sceKernelGetModuleInfoFromAddr_nid_no_patch(address, flags, info);
}

int APS5_VABI sceKernelGetModuleInfoInternal_nid_disambig1_nid_postfix(KernelModule handle, ModuleInfoEx* info) {
    return sceKernelGetModuleInfoInternal_nid_no_patch(handle, info);
}

int APS5_VABI sceKernelGetModuleList_nid_disambig1_nid_postfix(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList_nid_no_patch(handles, count, actual);
}

int APS5_VABI sceKernelGetModuleList2_nid_disambig1_nid_postfix(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList2_nid_no_patch(handles, count, actual);
}

int APS5_VABI sceKernelGetModuleListInternal_nid_disambig1_nid_postfix(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleListInternal_nid_no_patch(handles, count, actual);
}

int APS5_VABI sceKernelGetOpenPsId_nid_disambig1_nid_postfix(void* openPsIdOutput) {
    return sceKernelGetOpenPsId_nid_no_patch(openPsIdOutput);
}

int APS5_VABI sceKernelGetOpenPsIdForSystem_nid_disambig1_nid_postfix(void* id) {
    return sceKernelGetOpenPsIdForSystem_nid_no_patch(id);
}

int APS5_VABI sceKernelGetPageTableStats_nid_disambig1_nid_postfix(int* cpu_total, int* cpu_available, int* gpu_total, int* gpu_available) {
    return sceKernelGetPageTableStats_nid_no_patch(cpu_total, cpu_available, gpu_total, gpu_available);
}

int APS5_VABI sceKernelGetPagingStatsOfAllObjects_nid_disambig1_nid_postfix(void) {
    return sceKernelGetPagingStatsOfAllObjects_nid_no_patch();
}

int APS5_VABI sceKernelGetPagingStatsOfAllThreads_nid_disambig1_nid_postfix(void) {
    return sceKernelGetPagingStatsOfAllThreads_nid_no_patch();
}

int APS5_VABI sceKernelGetPhysPageSize_nid_disambig1_nid_postfix(void) {
    return sceKernelGetPhysPageSize_nid_no_patch();
}

void* APS5_VABI sceKernelGetProcParam_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProcParam_nid_no_patch();
}

int APS5_VABI sceKernelGetProcessAbiType_nid_disambig1_nid_postfix(int pid) {
    return sceKernelGetProcessAbiType_nid_no_patch(pid);
}

int APS5_VABI sceKernelGetProcessName_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProcessName_nid_no_patch();
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

int APS5_VABI sceKernelGetProcessType_nid_disambig1_nid_postfix(int pid) {
    return sceKernelGetProcessType_nid_no_patch(pid);
}

int APS5_VABI sceKernelGetProductCode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProductCode_nid_no_patch();
}

int APS5_VABI sceKernelGetProductStr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetProductStr_nid_no_patch();
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersion_nid_disambig1_nid_postfix(std::int32_t* version) {
    return sceKernelGetProsperoCompiledSdkVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersionByPid_nid_disambig1_nid_postfix(int pid, std::int32_t* version) {
    return sceKernelGetProsperoCompiledSdkVersionByPid_nid_no_patch(pid, version);
}

int APS5_VABI sceKernelGetProsperoSystemSwVersion_nid_disambig1_nid_postfix(SceKernelProsperoSystemSwVersion* version) {
    return sceKernelGetProsperoSystemSwVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetPrtAperture_nid_disambig1_nid_postfix(int index, void** addr, size_t* len) {
    return sceKernelGetPrtAperture_nid_no_patch(index, addr, len);
}

int APS5_VABI sceKernelGetPs4CompiledSdkVersion_nid_disambig1_nid_postfix(std::int32_t* version) {
    return sceKernelGetPs4CompiledSdkVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetPs4CompiledSdkVersionByPid_nid_disambig1_nid_postfix(int pid, std::int32_t* version) {
    return sceKernelGetPs4CompiledSdkVersionByPid_nid_no_patch(pid, version);
}

int APS5_VABI sceKernelGetPs4SystemSwVersion_nid_disambig1_nid_postfix(void) {
    return sceKernelGetPs4SystemSwVersion_nid_no_patch();
}

int APS5_VABI sceKernelGetPsmIntdevModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetPsmIntdevModeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetPsnAccessTraceLogForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetPsnAccessTraceLogForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetQafExpirationTimeNotafterForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetQafExpirationTimeNotafterForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetQafExpirationTimeNotbeforeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetQafExpirationTimeNotbeforeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetQafGenerationForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetQafGenerationForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetQafNameForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetQafNameForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetRenderingMode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetRenderingMode_nid_no_patch();
}

int APS5_VABI sceKernelGetResidentCount_nid_disambig1_nid_postfix(void) {
    return sceKernelGetResidentCount_nid_no_patch();
}

int APS5_VABI sceKernelGetResidentFmemCount_nid_disambig1_nid_postfix(int pid, uint64_t* resident, uint64_t* mapped) {
    return sceKernelGetResidentFmemCount_nid_no_patch(pid, resident, mapped);
}

int APS5_VABI sceKernelGetSafemode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSafemode_nid_no_patch();
}

int APS5_VABI sceKernelGetSanitizerMallocReplace_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSanitizerMallocReplace_nid_no_patch();
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSanitizerMallocReplaceExternal_nid_no_patch();
}

int APS5_VABI sceKernelGetSanitizerNewReplace_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSanitizerNewReplace_nid_no_patch();
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSanitizerNewReplaceExternal_nid_no_patch();
}

int APS5_VABI sceKernelGetSocClock_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSocClock_nid_no_patch();
}

int APS5_VABI sceKernelGetSocPowerConsumption_nid_disambig1_nid_postfix(void* consumption) {
    return sceKernelGetSocPowerConsumption_nid_no_patch(consumption);
}

int APS5_VABI sceKernelGetSocSensorTemperature_nid_disambig1_nid_postfix(int sensorId, int* outCelsius) {
    return sceKernelGetSocSensorTemperature_nid_no_patch(sensorId, outCelsius);
}

int APS5_VABI sceKernelGetSpecialIForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSpecialIForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetSubsysId_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSubsysId_nid_no_patch();
}

int APS5_VABI sceKernelGetSubsysSubId_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSubsysSubId_nid_no_patch();
}

int APS5_VABI sceKernelGetSystemExVersion_nid_disambig1_nid_postfix(char* first, char* second) {
    return sceKernelGetSystemExVersion_nid_no_patch(first, second);
}

int APS5_VABI sceKernelGetSystemLevelDebuggerModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSystemLevelDebuggerModeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetSystemSwBeta_nid_disambig1_nid_postfix(void) {
    return sceKernelGetSystemSwBeta_nid_no_patch();
}

int APS5_VABI sceKernelGetSystemSwVersion_nid_disambig1_nid_postfix(SceKernelSystemSwVersion* version) {
    return sceKernelGetSystemSwVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetThreadName_nid_disambig1_nid_postfix(uint32_t id, char* name) {
    return sceKernelGetThreadName_nid_no_patch(id, name);
}

int APS5_VABI sceKernelGetToolMemoryRange_nid_disambig1_nid_postfix(void) {
    return sceKernelGetToolMemoryRange_nid_no_patch();
}

int APS5_VABI sceKernelGetTraceMemoryStats_nid_disambig1_nid_postfix(std::uint64_t* first, std::uint64_t* second) {
    return sceKernelGetTraceMemoryStats_nid_no_patch(first, second);
}

uint64_t APS5_VABI sceKernelGetTscFrequency_nid_disambig1_nid_postfix(void) {
    return sceKernelGetTscFrequency_nid_no_patch();
}

int APS5_VABI sceKernelGetUniversalMode_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUniversalMode_nid_no_patch();
}

int APS5_VABI sceKernelGetUpdVersion_nid_disambig1_nid_postfix(std::uint32_t* version) {
    return sceKernelGetUpdVersion_nid_no_patch(version);
}

int APS5_VABI sceKernelGetUtokenDataExecutionForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenDataExecutionForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenExpirationTimeNotafterForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenExpirationTimeNotafterForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenExpirationTimeNotbeforeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenExpirationTimeNotbeforeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenFakeSharefactoryForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenFakeSharefactoryForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenFlagedUpdaterForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenFlagedUpdaterForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenNpEnvSwitchingForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenNpEnvSwitchingForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenSaveDataRepairForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenSaveDataRepairForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenStoreModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenStoreModeForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenUseSoftwagnerForAcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenUseSoftwagnerForAcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenUseSoftwagnerForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenUseSoftwagnerForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetUtokenWeakenedPortRestrictionForRcmgr_nid_disambig1_nid_postfix(void) {
    return sceKernelGetUtokenWeakenedPortRestrictionForRcmgr_nid_no_patch();
}

int APS5_VABI sceKernelGetVrCaptureSize_nid_disambig1_nid_postfix(void) {
    return sceKernelGetVrCaptureSize_nid_no_patch();
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

int APS5_VABI sceKernelGiveDirectMemoryToMapper_nid_disambig1_nid_postfix(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset) {
    return sceKernelGiveDirectMemoryToMapper_nid_no_patch(searchStart, searchEnd, size, alignment, usage, offset);
}

int APS5_VABI sceKernelHasNeoMode_nid_disambig1_nid_postfix(void) {
    return sceKernelHasNeoMode_nid_no_patch();
}

int APS5_VABI sceKernelHwGetBdDriveInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelHwGetBdDriveInfo_nid_no_patch();
}

int APS5_VABI sceKernelHwHasOpticalOut_nid_disambig1_nid_postfix(void) {
    return sceKernelHwHasOpticalOut_nid_no_patch();
}

int APS5_VABI sceKernelHwHasWlanBt_nid_disambig1_nid_postfix(void) {
    return sceKernelHwHasWlanBt_nid_no_patch();
}

int APS5_VABI sceKernelIccControlBDPowerState_nid_disambig1_nid_postfix(void) {
    return sceKernelIccControlBDPowerState_nid_no_patch();
}

int APS5_VABI sceKernelIccControlUSBPowerState_nid_disambig1_nid_postfix(void) {
    return sceKernelIccControlUSBPowerState_nid_no_patch();
}

int APS5_VABI sceKernelIccGetBDDriveChuckingState_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetBDDriveChuckingState_nid_no_patch();
}

int APS5_VABI sceKernelIccGetBDPowerState_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetBDPowerState_nid_no_patch();
}

int APS5_VABI sceKernelIccGetCPMode_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetCPMode_nid_no_patch();
}

int APS5_VABI sceKernelIccGetCountTime_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetCountTime_nid_no_patch();
}

int APS5_VABI sceKernelIccGetCpuInfoBit_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetCpuInfoBit_nid_no_patch();
}

int APS5_VABI sceKernelIccGetErrLog_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetErrLog_nid_no_patch();
}

int APS5_VABI sceKernelIccGetHwInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetHwInfo_nid_no_patch();
}

int APS5_VABI sceKernelIccGetPowerNumberOfBootShutdown_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetPowerNumberOfBootShutdown_nid_no_patch();
}

int APS5_VABI sceKernelIccGetPowerOperatingTime_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetPowerOperatingTime_nid_no_patch();
}

int APS5_VABI sceKernelIccGetPowerUpCause_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetPowerUpCause_nid_no_patch();
}

int APS5_VABI sceKernelIccGetSysEventLog_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetSysEventLog_nid_no_patch();
}

int APS5_VABI sceKernelIccGetUSBPowerState_nid_disambig1_nid_postfix(void) {
    return sceKernelIccGetUSBPowerState_nid_no_patch();
}

int APS5_VABI sceKernelIccIndicatorBootDone_nid_disambig1_nid_postfix(void) {
    return sceKernelIccIndicatorBootDone_nid_no_patch();
}

int APS5_VABI sceKernelIccIndicatorShutdown_nid_disambig1_nid_postfix(void) {
    return sceKernelIccIndicatorShutdown_nid_no_patch();
}

int APS5_VABI sceKernelIccIndicatorStandby_nid_disambig1_nid_postfix(void) {
    return sceKernelIccIndicatorStandby_nid_no_patch();
}

int APS5_VABI sceKernelIccIndicatorStandbyBoot_nid_disambig1_nid_postfix(void) {
    return sceKernelIccIndicatorStandbyBoot_nid_no_patch();
}

int APS5_VABI sceKernelIccIndicatorStandbyShutdown_nid_disambig1_nid_postfix(void) {
    return sceKernelIccIndicatorStandbyShutdown_nid_no_patch();
}

int APS5_VABI sceKernelIccNotifyBootStatus_nid_disambig1_nid_postfix(void) {
    return sceKernelIccNotifyBootStatus_nid_no_patch();
}

int APS5_VABI sceKernelIccNvsFlush_nid_disambig1_nid_postfix(void) {
    return sceKernelIccNvsFlush_nid_no_patch();
}

int APS5_VABI sceKernelIccReadPowerBootMessage_nid_disambig1_nid_postfix(void) {
    return sceKernelIccReadPowerBootMessage_nid_no_patch();
}

int APS5_VABI sceKernelIccSetBuzzer_nid_disambig1_nid_postfix(void) {
    return sceKernelIccSetBuzzer_nid_no_patch();
}

int APS5_VABI sceKernelIccSetCPMode_nid_disambig1_nid_postfix(void) {
    return sceKernelIccSetCPMode_nid_no_patch();
}

int APS5_VABI sceKernelIccSetCpuInfoBit_nid_disambig1_nid_postfix(void) {
    return sceKernelIccSetCpuInfoBit_nid_no_patch();
}

int APS5_VABI sceKernelIccSetDownloadMode_nid_disambig1_nid_postfix(void) {
    return sceKernelIccSetDownloadMode_nid_no_patch();
}

int APS5_VABI sceKernelIccSetPriorityBuzzer_nid_disambig1_nid_postfix(void) {
    return sceKernelIccSetPriorityBuzzer_nid_no_patch();
}

int APS5_VABI sceKernelInstallExceptionHandler_nid_disambig1_nid_postfix(int signum, void* handler) {
    return sceKernelInstallExceptionHandler_nid_no_patch(signum, handler);
}

int APS5_VABI sceKernelInternalCreateMapperSharedMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalCreateMapperSharedMemory_nid_no_patch();
}

int APS5_VABI sceKernelInternalGetKmemStatistics_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalGetKmemStatistics_nid_no_patch();
}

int APS5_VABI sceKernelInternalGetMapStatistics_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalGetMapStatistics_nid_no_patch();
}

int APS5_VABI sceKernelInternalHeapPrintBacktraceWithModuleInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalHeapPrintBacktraceWithModuleInfo_nid_no_patch();
}

int APS5_VABI sceKernelInternalLockMapperMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalLockMapperMemory_nid_no_patch();
}

int APS5_VABI sceKernelInternalMapDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalMapDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelInternalMapNamedDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalMapNamedDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelInternalMemoryGetAvailableSize_nid_disambig1_nid_postfix(std::size_t* size) {
    return sceKernelInternalMemoryGetAvailableSize_nid_no_patch(size);
}

std::int32_t APS5_VABI sceKernelInternalMemoryGetModuleSegmentInfo_nid_disambig1_nid_postfix(InternalSegmentInfo* result) {
    return sceKernelInternalMemoryGetModuleSegmentInfo_nid_no_patch(result);
}

int APS5_VABI sceKernelInternalResumeDirectMemoryRelease_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalResumeDirectMemoryRelease_nid_no_patch();
}

int APS5_VABI sceKernelInternalSuspendDirectMemoryRelease_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalSuspendDirectMemoryRelease_nid_no_patch();
}

int APS5_VABI sceKernelInternalUnlockMapperMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalUnlockMapperMemory_nid_no_patch();
}

int APS5_VABI sceKernelInternalUnmapMapperSharedMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelInternalUnmapMapperSharedMemory_nid_no_patch();
}

int APS5_VABI sceKernelIsAddressSanitizerEnabled_nid_disambig1_nid_postfix(void) {
    return sceKernelIsAddressSanitizerEnabled_nid_no_patch();
}

int APS5_VABI sceKernelIsAllowedToSelectDvdRegion_nid_disambig1_nid_postfix(void) {
    return sceKernelIsAllowedToSelectDvdRegion_nid_no_patch();
}

int APS5_VABI sceKernelIsAuthenticNeo_nid_disambig1_nid_postfix(void) {
    return sceKernelIsAuthenticNeo_nid_no_patch();
}

int APS5_VABI sceKernelIsCEX_nid_disambig1_nid_postfix(void) {
    return sceKernelIsCEX_nid_no_patch();
}

int APS5_VABI sceKernelIsCronos_nid_disambig1_nid_postfix(void) {
    return sceKernelIsCronos_nid_no_patch();
}

int APS5_VABI sceKernelIsDebuggerAttached_nid_disambig1_nid_postfix(void) {
    return sceKernelIsDebuggerAttached_nid_no_patch();
}

int APS5_VABI sceKernelIsDevKit_nid_disambig1_nid_postfix(void) {
    return sceKernelIsDevKit_nid_no_patch();
}

int APS5_VABI sceKernelIsExperimentalBeta_nid_disambig1_nid_postfix(void) {
    return sceKernelIsExperimentalBeta_nid_no_patch();
}

int APS5_VABI sceKernelIsGenuineCEX_nid_disambig1_nid_postfix(void) {
    return sceKernelIsGenuineCEX_nid_no_patch();
}

int APS5_VABI sceKernelIsGenuineCronosCex_nid_disambig1_nid_postfix(void) {
    return sceKernelIsGenuineCronosCex_nid_no_patch();
}

int APS5_VABI sceKernelIsGenuineDevKit_nid_disambig1_nid_postfix(void) {
    return sceKernelIsGenuineDevKit_nid_no_patch();
}

int APS5_VABI sceKernelIsGenuineKratosCex_nid_disambig1_nid_postfix(void) {
    return sceKernelIsGenuineKratosCex_nid_no_patch();
}

int APS5_VABI sceKernelIsGenuineN_nid_disambig1_nid_postfix(void) {
    return sceKernelIsGenuineN_nid_no_patch();
}

int APS5_VABI sceKernelIsGenuineTestKit_nid_disambig1_nid_postfix(void) {
    return sceKernelIsGenuineTestKit_nid_no_patch();
}

int APS5_VABI sceKernelIsInSandbox_nid_disambig1_nid_postfix(void) {
    return sceKernelIsInSandbox_nid_no_patch();
}

int APS5_VABI sceKernelIsKratos_nid_disambig1_nid_postfix(void) {
    return sceKernelIsKratos_nid_no_patch();
}

int APS5_VABI sceKernelIsMainOnStanbyMode_nid_disambig1_nid_postfix(void) {
    return sceKernelIsMainOnStanbyMode_nid_no_patch();
}

int APS5_VABI sceKernelIsMainOnStandbyMode_nid_disambig1_nid_postfix(void) {
    return sceKernelIsMainOnStandbyMode_nid_no_patch();
}

int APS5_VABI sceKernelIsModuleLoaded_nid_disambig1_nid_postfix(const char* name) {
    return sceKernelIsModuleLoaded_nid_no_patch(name);
}

int APS5_VABI sceKernelIsNeoMode_nid_disambig1_nid_postfix(void) {
    return sceKernelIsNeoMode_nid_no_patch();
}

int APS5_VABI sceKernelIsProspero_nid_disambig1_nid_postfix(void) {
    return sceKernelIsProspero_nid_no_patch();
}

int APS5_VABI sceKernelIsPs4Process_nid_disambig1_nid_postfix(void) {
    return sceKernelIsPs4Process_nid_no_patch();
}

int APS5_VABI sceKernelIsStack_nid_disambig1_nid_postfix(void* addr, void** start, void** end) {
    return sceKernelIsStack_nid_no_patch(addr, start, end);
}

int APS5_VABI sceKernelIsTestKit_nid_disambig1_nid_postfix(void) {
    return sceKernelIsTestKit_nid_no_patch();
}

int APS5_VABI sceKernelIsThreadSanitizerEnabled_nid_disambig1_nid_postfix(void) {
    return sceKernelIsThreadSanitizerEnabled_nid_no_patch();
}

int APS5_VABI sceKernelJitCreateAliasOfSharedMemory_nid_disambig1_nid_postfix(int fd, int maxProt, int* fdOut) {
    return sceKernelJitCreateAliasOfSharedMemory_nid_no_patch(fd, maxProt, fdOut);
}

int APS5_VABI sceKernelJitCreateSharedMemory_nid_disambig1_nid_postfix(const char* name, std::size_t len, int maxProt, int* fdOut) {
    return sceKernelJitCreateSharedMemory_nid_no_patch(name, len, maxProt, fdOut);
}

int APS5_VABI sceKernelJitGetSharedMemoryInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelJitGetSharedMemoryInfo_nid_no_patch();
}

int APS5_VABI sceKernelJitMapSharedMemory_nid_disambig1_nid_postfix(int fd, int prot, void** startOut) {
    return sceKernelJitMapSharedMemory_nid_no_patch(fd, prot, startOut);
}

int APS5_VABI sceKernelKernelHeapUsage_nid_disambig1_nid_postfix(void) {
    return sceKernelKernelHeapUsage_nid_no_patch();
}

KernelModule APS5_VABI sceKernelLoadStartModule_nid_disambig1_nid_postfix(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModule_nid_no_patch(module_file_name, args, argp, flags, opt, res);
}

KernelModule APS5_VABI sceKernelLoadStartModuleForSysmodule_nid_disambig1_nid_postfix(const char* moduleFileName, std::size_t args, const void* argp, std::uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModuleForSysmodule_nid_no_patch(moduleFileName, args, argp, flags, opt, res);
}

std::int64_t APS5_VABI sceKernelLseek_nid_disambig1_nid_postfix(int d, std::int64_t offset, int whence) {
    return sceKernelLseek_nid_no_patch(d, offset, whence);
}

int APS5_VABI sceKernelLwfsAllocateBlock_nid_disambig1_nid_postfix(int32_t fd, int64_t size) {
    return sceKernelLwfsAllocateBlock_nid_no_patch(fd, size);
}

int64_t APS5_VABI sceKernelLwfsLseek_nid_disambig1_nid_postfix(int32_t fd, int64_t offset, int32_t whence) {
    return sceKernelLwfsLseek_nid_no_patch(fd, offset, whence);
}

int APS5_VABI sceKernelLwfsSetAttribute_nid_disambig1_nid_postfix(int32_t fd, int32_t attribute) {
    return sceKernelLwfsSetAttribute_nid_no_patch(fd, attribute);
}

int APS5_VABI sceKernelLwfsTrimBlock_nid_disambig1_nid_postfix(void) {
    return sceKernelLwfsTrimBlock_nid_no_patch();
}

int64_t APS5_VABI sceKernelLwfsWrite_nid_disambig1_nid_postfix(int32_t fd, const void* data, std::size_t size) {
    return sceKernelLwfsWrite_nid_no_patch(fd, data, size);
}

int APS5_VABI sceKernelMapDirectMemory_nid_disambig1_nid_postfix(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
    return sceKernelMapDirectMemory_nid_no_patch(addr, len, prot, flags, direct_memory_start, alignment);
}

int APS5_VABI sceKernelMapDirectMemory2_nid_disambig1_nid_postfix(void** addr, size_t len, int type, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
    return sceKernelMapDirectMemory2_nid_no_patch(addr, len, type, prot, flags, direct_memory_start, alignment);
}

int APS5_VABI sceKernelMapFlexibleMemory_nid_disambig1_nid_postfix(void** addr_in_out, size_t len, int prot, int flags) {
    return sceKernelMapFlexibleMemory_nid_no_patch(addr_in_out, len, prot, flags);
}

int APS5_VABI sceKernelMapNamedDirectMemory_nid_disambig1_nid_postfix(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment, const char* name) {
    return sceKernelMapNamedDirectMemory_nid_no_patch(addr, len, prot, flags, direct_memory_start, alignment, name);
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory_nid_disambig1_nid_postfix(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedFlexibleMemory_nid_no_patch(addr_in_out, len, prot, flags, name);
}

int32_t APS5_VABI sceKernelMapNamedSystemFlexibleMemory_nid_disambig1_nid_postfix(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedSystemFlexibleMemory_nid_no_patch(addr_in_out, len, prot, flags, name);
}

int APS5_VABI sceKernelMapSanitizerShadowMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelMapSanitizerShadowMemory_nid_no_patch();
}

int APS5_VABI sceKernelMapToolMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelMapToolMemory_nid_no_patch();
}

int APS5_VABI sceKernelMapTraceMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelMapTraceMemory_nid_no_patch();
}

int APS5_VABI sceKernelMapperGetIndirectBufferInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelMapperGetIndirectBufferInfo_nid_no_patch();
}

int APS5_VABI sceKernelMapperGetParam_nid_disambig1_nid_postfix(std::uint64_t* param) {
    return sceKernelMapperGetParam_nid_no_patch(param);
}

int APS5_VABI sceKernelMapperGetUsageStatsData_nid_disambig1_nid_postfix(void) {
    return sceKernelMapperGetUsageStatsData_nid_no_patch();
}

int APS5_VABI sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold_nid_disambig1_nid_postfix(void) {
    return sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold_nid_no_patch();
}

int APS5_VABI sceKernelMemoryPoolBatch_nid_disambig1_nid_postfix(const KernelMemoryPoolBatchEntry* entries, int num_entries, int* num_entries_out, int flags) {
    return sceKernelMemoryPoolBatch_nid_no_patch(entries, num_entries, num_entries_out, flags);
}

int APS5_VABI sceKernelMemoryPoolCommit_nid_disambig1_nid_postfix(void* addr, size_t len, int type, int prot, int flags) {
    return sceKernelMemoryPoolCommit_nid_no_patch(addr, len, type, prot, flags);
}

int APS5_VABI sceKernelMemoryPoolDecommit_nid_disambig1_nid_postfix(void* addr, size_t len, int flags) {
    return sceKernelMemoryPoolDecommit_nid_no_patch(addr, len, flags);
}

int APS5_VABI sceKernelMemoryPoolExpand_nid_disambig1_nid_postfix(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int64_t* phys_addr_out) {
    return sceKernelMemoryPoolExpand_nid_no_patch(search_start, search_end, len, alignment, phys_addr_out);
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats_nid_disambig1_nid_postfix(KernelMemoryPoolBlockStats* output, size_t output_size) {
    return sceKernelMemoryPoolGetBlockStats_nid_no_patch(output, output_size);
}

int APS5_VABI sceKernelMemoryPoolMove_nid_disambig1_nid_postfix(void) {
    return sceKernelMemoryPoolMove_nid_no_patch();
}

int APS5_VABI sceKernelMemoryPoolReserve_nid_disambig1_nid_postfix(void* addr_in, size_t len, size_t alignment, int flags, void** addr_out) {
    return sceKernelMemoryPoolReserve_nid_no_patch(addr_in, len, alignment, flags, addr_out);
}

int APS5_VABI sceKernelMkdir_nid_disambig1_nid_postfix(const char* path, uint16_t mode) {
    return sceKernelMkdir_nid_no_patch(path, mode);
}

int APS5_VABI sceKernelMlock_nid_disambig1_nid_postfix(void* address, std::uint64_t length) {
    return sceKernelMlock_nid_no_patch(address, length);
}

int APS5_VABI sceKernelMlockall_nid_disambig1_nid_postfix(void) {
    return sceKernelMlockall_nid_no_patch();
}

int APS5_VABI sceKernelMmap_nid_disambig1_nid_postfix(void* addr, size_t len, int prot, int flags, int fd, int64_t offset, void** res) {
    return sceKernelMmap_nid_no_patch(addr, len, prot, flags, fd, offset, res);
}

int APS5_VABI sceKernelMprotect_nid_disambig1_nid_postfix(const void* addr, size_t len, int prot) {
    return sceKernelMprotect_nid_no_patch(addr, len, prot);
}

int APS5_VABI sceKernelMsync_nid_disambig1_nid_postfix(void* address, std::size_t length, int flags) {
    return sceKernelMsync_nid_no_patch(address, length, flags);
}

int APS5_VABI sceKernelMtypeprotect_nid_disambig1_nid_postfix(const void* addr, size_t len, int type, int prot) {
    return sceKernelMtypeprotect_nid_no_patch(addr, len, type, prot);
}

int APS5_VABI sceKernelMunlock_nid_disambig1_nid_postfix(void* address, std::uint64_t length) {
    return sceKernelMunlock_nid_no_patch(address, length);
}

int APS5_VABI sceKernelMunlockall_nid_disambig1_nid_postfix(void) {
    return sceKernelMunlockall_nid_no_patch();
}

int APS5_VABI sceKernelMunmap_nid_disambig1_nid_postfix(uint64_t vaddr, size_t len) {
    return sceKernelMunmap_nid_no_patch(vaddr, len);
}

int APS5_VABI sceKernelNanosleep_nid_disambig1_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep_nid_no_patch(rqtp, rmtp);
}

int APS5_VABI sceKernelNormalizePath_nid_disambig1_nid_postfix(void) {
    return sceKernelNormalizePath_nid_no_patch();
}

int APS5_VABI sceKernelNotifyAppStateChanged_nid_disambig1_nid_postfix(void) {
    return sceKernelNotifyAppStateChanged_nid_no_patch();
}

int APS5_VABI sceKernelNotifySystemSuspendResumeProgress_nid_disambig1_nid_postfix(void) {
    return sceKernelNotifySystemSuspendResumeProgress_nid_no_patch();
}

int APS5_VABI sceKernelNotifySystemSuspendStart_nid_disambig1_nid_postfix(void) {
    return sceKernelNotifySystemSuspendStart_nid_no_patch();
}

int APS5_VABI sceKernelOpen_nid_disambig1_nid_postfix(const char* path, int flags, std::uint16_t mode) {
    return sceKernelOpen_nid_no_patch(path, flags, mode);
}

int APS5_VABI sceKernelOpenEport_nid_disambig1_nid_postfix(void) {
    return sceKernelOpenEport_nid_no_patch();
}

int APS5_VABI sceKernelOpenEventFlag_nid_disambig1_nid_postfix(KernelEventFlag* ef, const char* name) {
    return sceKernelOpenEventFlag_nid_no_patch(ef, name);
}

int APS5_VABI sceKernelOpenInternal_nid_disambig1_nid_postfix(void) {
    return sceKernelOpenInternal_nid_no_patch();
}

int APS5_VABI sceKernelOpenSema_nid_disambig1_nid_postfix(void) {
    return sceKernelOpenSema_nid_no_patch();
}

int APS5_VABI sceKernelPltTrap_nid_disambig1_nid_postfix(void) {
    return sceKernelPltTrap_nid_no_patch();
}

int APS5_VABI sceKernelPollEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
    return sceKernelPollEventFlag_nid_no_patch(ef, bit_pattern, wait_mode, result_pat);
}

int APS5_VABI sceKernelPollSema_nid_disambig1_nid_postfix(KernelSema sem, int need) {
    return sceKernelPollSema_nid_no_patch(sem, need);
}

int64_t APS5_VABI sceKernelPread_nid_disambig1_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPread_nid_no_patch(d, buf, nbytes, offset);
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelPrintBacktraceWithModuleInfo_nid_no_patch();
}

int APS5_VABI sceKernelProtectDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelProtectDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelProtectDirectMemoryForPID_nid_disambig1_nid_postfix(int64_t offset, size_t len, int prot, int pid) {
    return sceKernelProtectDirectMemoryForPID_nid_no_patch(offset, len, prot, pid);
}

int64_t APS5_VABI sceKernelPwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPwrite_nid_no_patch(d, buf, nbytes, offset);
}

int APS5_VABI sceKernelQueryMemoryProtection_nid_disambig1_nid_postfix(void* addr, void** start, void** end, int* prot) {
    return sceKernelQueryMemoryProtection_nid_no_patch(addr, start, end, prot);
}

int APS5_VABI sceKernelQueryToolMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelQueryToolMemory_nid_no_patch();
}

int APS5_VABI sceKernelQueryTraceMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelQueryTraceMemory_nid_no_patch();
}

int APS5_VABI sceKernelRaiseException_nid_disambig1_nid_postfix(Pthread thread, int signum) {
    return sceKernelRaiseException_nid_no_patch(thread, signum);
}

int APS5_VABI sceKernelRandomizedPath_nid_disambig1_nid_postfix(void) {
    return sceKernelRandomizedPath_nid_no_patch();
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

int APS5_VABI sceKernelReboot_nid_disambig1_nid_postfix(void) {
    return sceKernelReboot_nid_no_patch();
}

int APS5_VABI sceKernelReleaseDirectMemory_nid_disambig1_nid_postfix(int64_t start, size_t len) {
    return sceKernelReleaseDirectMemory_nid_no_patch(start, len);
}

int APS5_VABI sceKernelReleaseFlexibleMemory_nid_disambig1_nid_postfix(void* addr, size_t len) {
    return sceKernelReleaseFlexibleMemory_nid_no_patch(addr, len);
}

int APS5_VABI sceKernelReleaseToolMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelReleaseToolMemory_nid_no_patch();
}

int APS5_VABI sceKernelReleaseTraceDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelReleaseTraceDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelRemoveExceptionHandler_nid_disambig1_nid_postfix(int signum) {
    return sceKernelRemoveExceptionHandler_nid_no_patch(signum);
}

int APS5_VABI sceKernelRename_nid_disambig1_nid_postfix(const char* from, const char* to) {
    return sceKernelRename_nid_no_patch(from, to);
}

int APS5_VABI sceKernelReportUnpatchedFunctionCall_nid_disambig1_nid_postfix(void) {
    return sceKernelReportUnpatchedFunctionCall_nid_no_patch();
}

int APS5_VABI sceKernelReserve2mbPage_nid_disambig1_nid_postfix(void) {
    return sceKernelReserve2mbPage_nid_no_patch();
}

int APS5_VABI sceKernelReserveSystemDirectMemory_nid_disambig1_nid_postfix(void) {
    return sceKernelReserveSystemDirectMemory_nid_no_patch();
}

int APS5_VABI sceKernelReserveVirtualRange_nid_disambig1_nid_postfix(void** addr, size_t len, int flags, size_t alignment) {
    return sceKernelReserveVirtualRange_nid_no_patch(addr, len, flags, alignment);
}

int APS5_VABI sceKernelResumeDirectMemoryRelease_nid_disambig1_nid_postfix(void) {
    return sceKernelResumeDirectMemoryRelease_nid_no_patch();
}

int APS5_VABI sceKernelRmdir_nid_disambig1_nid_postfix(const char* path) {
    return sceKernelRmdir_nid_no_patch(path);
}

int APS5_VABI sceKernelRtldControl_nid_disambig1_nid_postfix(void) {
    return sceKernelRtldControl_nid_no_patch();
}

int APS5_VABI sceKernelSandboxPath_nid_disambig1_nid_postfix(void) {
    return sceKernelSandboxPath_nid_no_patch();
}

int APS5_VABI sceKernelSendNotificationRequest_nid_disambig1_nid_postfix(int device, const void* request, std::size_t size, int flags) {
    return sceKernelSendNotificationRequest_nid_no_patch(device, request, size, flags);
}

int APS5_VABI sceKernelSetAppInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelSetAppInfo_nid_no_patch();
}

int APS5_VABI sceKernelSetAppLibcData_nid_disambig1_nid_postfix(void) {
    return sceKernelSetAppLibcData_nid_no_patch();
}

int APS5_VABI sceKernelSetBackupRestoreMode_nid_disambig1_nid_postfix(void) {
    return sceKernelSetBackupRestoreMode_nid_no_patch();
}

int APS5_VABI sceKernelSetBaseModeClock_nid_disambig1_nid_postfix(void) {
    return sceKernelSetBaseModeClock_nid_no_patch();
}

int APS5_VABI sceKernelSetBesteffort_nid_disambig1_nid_postfix(void) {
    return sceKernelSetBesteffort_nid_no_patch();
}

int APS5_VABI sceKernelSetBootReqNotify_nid_disambig1_nid_postfix(void) {
    return sceKernelSetBootReqNotify_nid_no_patch();
}

int APS5_VABI sceKernelSetCallRecord_nid_disambig1_nid_postfix(void) {
    return sceKernelSetCallRecord_nid_no_patch();
}

int APS5_VABI sceKernelSetCanvasHandle_nid_disambig1_nid_postfix(void) {
    return sceKernelSetCanvasHandle_nid_no_patch();
}

int APS5_VABI sceKernelSetCompressionAttribute_nid_disambig1_nid_postfix(int32_t fd, int32_t attribute) {
    return sceKernelSetCompressionAttribute_nid_no_patch(fd, attribute);
}

int APS5_VABI sceKernelSetCpumodeGame_nid_disambig1_nid_postfix(int mode) {
    return sceKernelSetCpumodeGame_nid_no_patch(mode);
}

int APS5_VABI sceKernelSetDataTransferMode_nid_disambig1_nid_postfix(void) {
    return sceKernelSetDataTransferMode_nid_no_patch();
}

int APS5_VABI sceKernelSetEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bit_pattern) {
    return sceKernelSetEventFlag_nid_no_patch(ef, bit_pattern);
}

int APS5_VABI sceKernelSetFsstParam_nid_disambig1_nid_postfix(int32_t fd, void* param) {
    return sceKernelSetFsstParam_nid_no_patch(fd, param);
}

int APS5_VABI sceKernelSetGPI_nid_disambig1_nid_postfix(void) {
    return sceKernelSetGPI_nid_no_patch();
}

void APS5_VABI sceKernelSetGPO_nid_disambig1_nid_postfix(std::uint32_t bits) {
    sceKernelSetGPO_nid_no_patch(bits);
}

int APS5_VABI sceKernelSetGameDirectMemoryLimit_nid_disambig1_nid_postfix(void) {
    return sceKernelSetGameDirectMemoryLimit_nid_no_patch();
}

int APS5_VABI sceKernelSetGpuCu_nid_disambig1_nid_postfix(void) {
    return sceKernelSetGpuCu_nid_no_patch();
}

int APS5_VABI sceKernelSetKnobs_nid_disambig1_nid_postfix(void) {
    return sceKernelSetKnobs_nid_no_patch();
}

int APS5_VABI sceKernelSetMemoryPstate_nid_disambig1_nid_postfix(void) {
    return sceKernelSetMemoryPstate_nid_no_patch();
}

int APS5_VABI sceKernelSetNeoModeClock_nid_disambig1_nid_postfix(void) {
    return sceKernelSetNeoModeClock_nid_no_patch();
}

int APS5_VABI sceKernelSetPhysFmemLimit_nid_disambig1_nid_postfix(void) {
    return sceKernelSetPhysFmemLimit_nid_no_patch();
}

int APS5_VABI sceKernelSetProcessName_nid_disambig1_nid_postfix(void) {
    return sceKernelSetProcessName_nid_no_patch();
}

int APS5_VABI sceKernelSetProcessProperty_nid_disambig1_nid_postfix(void) {
    return sceKernelSetProcessProperty_nid_no_patch();
}

int APS5_VABI sceKernelSetProcessPropertyString_nid_disambig1_nid_postfix(void) {
    return sceKernelSetProcessPropertyString_nid_no_patch();
}

int APS5_VABI sceKernelSetPrtAperture_nid_disambig1_nid_postfix(int index, void* addr, size_t len) {
    return sceKernelSetPrtAperture_nid_no_patch(index, addr, len);
}

int APS5_VABI sceKernelSetSafemode_nid_disambig1_nid_postfix(void) {
    return sceKernelSetSafemode_nid_no_patch();
}

int APS5_VABI sceKernelSetTimezoneInfo_nid_disambig1_nid_postfix(void) {
    return sceKernelSetTimezoneInfo_nid_no_patch();
}

int APS5_VABI sceKernelSetVirtualRangeName_nid_disambig1_nid_postfix(const void* addr, uint64_t len, const char* name) {
    return sceKernelSetVirtualRangeName_nid_no_patch(addr, len, name);
}

int APS5_VABI sceKernelSetVmContainer_nid_disambig1_nid_postfix(void) {
    return sceKernelSetVmContainer_nid_no_patch();
}

int APS5_VABI sceKernelSettimeofday_nid_disambig1_nid_postfix(const KernelTimeval* tv, const KernelTimezone* tz) {
    return sceKernelSettimeofday_nid_no_patch(tv, tz);
}

int APS5_VABI sceKernelSignalSema_nid_disambig1_nid_postfix(KernelSema sem, int count) {
    return sceKernelSignalSema_nid_no_patch(sem, count);
}

unsigned int APS5_VABI sceKernelSleep_nid_disambig1_nid_postfix(unsigned int seconds) {
    return sceKernelSleep_nid_no_patch(seconds);
}

int APS5_VABI sceKernelSlvNotifyError_nid_disambig1_nid_postfix(void) {
    return sceKernelSlvNotifyError_nid_no_patch();
}

int APS5_VABI sceKernelStat_nid_disambig1_nid_postfix(const char* path, FileStat* sb) {
    return sceKernelStat_nid_no_patch(path, sb);
}

int APS5_VABI sceKernelStopUnloadModule_nid_disambig1_nid_postfix(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
    return sceKernelStopUnloadModule_nid_no_patch(handle, args, argp, flags, opt, res);
}

int APS5_VABI sceKernelSuspendDirectMemoryRelease_nid_disambig1_nid_postfix(void) {
    return sceKernelSuspendDirectMemoryRelease_nid_no_patch();
}

void APS5_VABI sceKernelSync_nid_disambig1_nid_postfix(void) {
    sceKernelSync_nid_no_patch();
}

int APS5_VABI sceKernelTerminateSysCore_nid_disambig1_nid_postfix(void) {
    return sceKernelTerminateSysCore_nid_no_patch();
}

int APS5_VABI sceKernelTitleWorkaroundIsEnabled_nid_disambig1_nid_postfix(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    return sceKernelTitleWorkaroundIsEnabled_nid_no_patch(workaround, bit, enabled);
}

int APS5_VABI sceKernelTitleWorkdaroundIsEnabled_nid_disambig1_nid_postfix(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    return sceKernelTitleWorkdaroundIsEnabled_nid_no_patch(workaround, bit, enabled);
}

int APS5_VABI sceKernelTraceMemoryTypeProtect_nid_disambig1_nid_postfix(void) {
    return sceKernelTraceMemoryTypeProtect_nid_no_patch();
}

int APS5_VABI sceKernelTriggerEport_nid_disambig1_nid_postfix(void) {
    return sceKernelTriggerEport_nid_no_patch();
}

int APS5_VABI sceKernelTriggerUserEvent_nid_disambig1_nid_postfix(KernelEqueue eq, int id, void* udata) {
    return sceKernelTriggerUserEvent_nid_no_patch(eq, id, udata);
}

int APS5_VABI sceKernelTruncate_nid_disambig1_nid_postfix(const char* path, std::int64_t length) {
    return sceKernelTruncate_nid_no_patch(path, length);
}

int APS5_VABI sceKernelUnlink_nid_disambig1_nid_postfix(const char* path) {
    return sceKernelUnlink_nid_no_patch(path);
}

int APS5_VABI sceKernelUseCronosKura_nid_disambig1_nid_postfix(void) {
    return sceKernelUseCronosKura_nid_no_patch();
}

int APS5_VABI sceKernelUseCronosStreaming_nid_disambig1_nid_postfix(void) {
    return sceKernelUseCronosStreaming_nid_no_patch();
}

int APS5_VABI sceKernelUsleep_nid_disambig1_nid_postfix(KernelUseconds microseconds) {
    return sceKernelUsleep_nid_no_patch(microseconds);
}

int APS5_VABI sceKernelUtimes_nid_disambig1_nid_postfix(const char* path, const KernelTimeval* times) {
    return sceKernelUtimes_nid_no_patch(path, times);
}

int APS5_VABI sceKernelUuidCreate_nid_disambig1_nid_postfix(std::uint32_t* uuid) {
    return sceKernelUuidCreate_nid_no_patch(uuid);
}

int APS5_VABI sceKernelVirtualQuery_nid_disambig1_nid_postfix(const void* addr, int flags, VirtualQueryInfo* info, uint64_t info_size) {
    return sceKernelVirtualQuery_nid_no_patch(addr, flags, info, info_size);
}

int APS5_VABI sceKernelVirtualQueryAll_nid_disambig1_nid_postfix(void) {
    return sceKernelVirtualQueryAll_nid_no_patch();
}

int APS5_VABI sceKernelWaitCommandBufferCompletion_nid_disambig1_nid_postfix(std::uint32_t id) {
    return sceKernelWaitCommandBufferCompletion_nid_no_patch(id);
}

int APS5_VABI sceKernelWaitEqueue_nid_disambig1_nid_postfix(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    return sceKernelWaitEqueue_nid_no_patch(eq, ev, num, out, timo);
}

int APS5_VABI sceKernelWaitEventFlag_nid_disambig1_nid_postfix(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
    return sceKernelWaitEventFlag_nid_no_patch(ef, bit_pattern, wait_mode, result_pat, timeout);
}

int APS5_VABI sceKernelWaitSema_nid_disambig1_nid_postfix(KernelSema sem, int need, KernelUseconds* time) {
    return sceKernelWaitSema_nid_no_patch(sem, need, time);
}

std::int64_t APS5_VABI sceKernelWrite_nid_disambig1_nid_postfix(int d, const void* buf, std::size_t nbytes) {
    return sceKernelWrite_nid_no_patch(d, buf, nbytes);
}

int APS5_VABI sceKernelWriteEventQueueOnCompletionCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t equeue, std::int32_t ident, std::uint64_t data, std::uint64_t* outSize) {
    return sceKernelWriteEventQueueOnCompletionCommand_nid_no_patch(dst, equeue, ident, data, outSize);
}

int APS5_VABI sceKernelWriteMapCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteMapCommand_nid_no_patch(dst, address, size, type, protection, outSize);
}

int APS5_VABI sceKernelWriteMapCommand2_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t prt, std::uint64_t* outSize) {
    return sceKernelWriteMapCommand2_nid_no_patch(dst, address, size, type, protection, prt, outSize);
}

int APS5_VABI sceKernelWriteMapDirectCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteMapDirectCommand_nid_no_patch(dst, address, directOffset, size, type, protection, outSize);
}

int APS5_VABI sceKernelWriteMapDirectWithGpuMaskIdCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteMapDirectWithGpuMaskIdCommand_nid_no_patch(dst, address, directOffset, size, type, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteMapWithGpuMaskIdCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteMapWithGpuMaskIdCommand_nid_no_patch(dst, address, size, type, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return sceKernelWriteModifyMtypeProtectCommand_nid_no_patch(dst, address, size, type, protection, mask, outSize);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand_nid_no_patch(dst, address, size, type, protection, mask, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return sceKernelWriteModifyProtectCommand_nid_no_patch(dst, address, size, protection, mask, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectWithGpuMaskIdCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteModifyProtectWithGpuMaskIdCommand_nid_no_patch(dst, address, size, protection, mask, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteMultiMapCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteMultiMapCommand_nid_no_patch(dst, address, alias, size, protection, outSize);
}

int APS5_VABI sceKernelWriteMultiMapWithGpuMaskIdCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteMultiMapWithGpuMaskIdCommand_nid_no_patch(dst, address, alias, size, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteRemapCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteRemapCommand_nid_no_patch(dst, address, source, size, protection, outSize);
}

int APS5_VABI sceKernelWriteRemapIntoPrtCommand_nid_disambig1_nid_postfix(std::uint32_t* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, int protection, int opcode, std::uint64_t* outSize) {
    return sceKernelWriteRemapIntoPrtCommand_nid_no_patch(dst, address, source, size, protection, opcode, outSize);
}

int APS5_VABI sceKernelWriteRemapWithGpuMaskIdCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteRemapWithGpuMaskIdCommand_nid_no_patch(dst, address, source, size, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteSdkEventLog_nid_disambig1_nid_postfix(void) {
    return sceKernelWriteSdkEventLog_nid_no_patch();
}

int APS5_VABI sceKernelWriteSdkEventLogV_nid_disambig1_nid_postfix(void) {
    return sceKernelWriteSdkEventLogV_nid_no_patch();
}

int APS5_VABI sceKernelWriteUnmapCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t* outSize) {
    return sceKernelWriteUnmapCommand_nid_no_patch(dst, address, size, outSize);
}

int APS5_VABI sceKernelWriteWaitOnAddressCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t reference, std::int32_t compare, std::int32_t flush, std::uint64_t* outSize) {
    return sceKernelWriteWaitOnAddressCommand_nid_no_patch(dst, address, reference, compare, flush, outSize);
}

int APS5_VABI sceKernelWriteWaitOnCounterCommand_nid_disambig1_nid_postfix(void* dst, std::uint8_t counter, std::uint32_t reference, std::int32_t compare, std::int32_t flush, std::uint64_t* outSize) {
    return sceKernelWriteWaitOnCounterCommand_nid_no_patch(dst, counter, reference, compare, flush, outSize);
}

int APS5_VABI sceKernelWriteWriteAddressCommand_nid_disambig1_nid_postfix(void* dst, std::uint64_t address, std::uint64_t value, std::uint64_t* outSize) {
    return sceKernelWriteWriteAddressCommand_nid_no_patch(dst, address, value, outSize);
}

int APS5_VABI sceKernelWriteWriteCounterCommand_nid_disambig1_nid_postfix(void* dst, std::uint8_t counter, std::uint32_t value, std::uint64_t* outSize) {
    return sceKernelWriteWriteCounterCommand_nid_no_patch(dst, counter, value, outSize);
}

int APS5_VABI sceKernelYieldCpumode_nid_disambig1_nid_postfix(void) {
    return sceKernelYieldCpumode_nid_no_patch();
}

int APS5_VABI sceLibcMspaceCreateForMonoMutex_nid_disambig1_nid_postfix(void) {
    return sceLibcMspaceCreateForMonoMutex_nid_no_patch();
}

int APS5_VABI scePthreadAtfork_nid_disambig1_nid_postfix(void (APS5_VABI* prepare)(), void (APS5_VABI* parent)(), void (APS5_VABI* child)()) {
    return scePthreadAtfork_nid_no_patch(prepare, parent, child);
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

int APS5_VABI scePthreadAttrGetguardsize_nid_disambig1_nid_postfix(const PthreadAttr* attr, size_t* guard_size) {
    return scePthreadAttrGetguardsize_nid_no_patch(attr, guard_size);
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

int APS5_VABI scePthreadAttrGetscope_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* scope) {
    return scePthreadAttrGetscope_nid_no_patch(attr, scope);
}

int APS5_VABI scePthreadAttrGetsolosched_nid_disambig1_nid_postfix(const PthreadAttr* attr, int* solosched) {
    return scePthreadAttrGetsolosched_nid_no_patch(attr, solosched);
}

int APS5_VABI scePthreadAttrGetstack_nid_disambig1_nid_postfix(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    return scePthreadAttrGetstack_nid_no_patch(attr, stackaddr, stacksize);
}

int APS5_VABI scePthreadAttrGetstackaddr_nid_disambig1_nid_postfix(const PthreadAttr* attr, void** stack_addr) {
    return scePthreadAttrGetstackaddr_nid_no_patch(attr, stack_addr);
}

int APS5_VABI scePthreadAttrGetstacksize_nid_disambig1_nid_postfix(const PthreadAttr* attr, size_t* stack_size) {
    return scePthreadAttrGetstacksize_nid_no_patch(attr, stack_size);
}

int APS5_VABI scePthreadAttrInit_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return scePthreadAttrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadAttrSetaffinity_nid_disambig1_nid_postfix(PthreadAttr* attr, KernelCpumask mask) {
    return scePthreadAttrSetaffinity_nid_no_patch(attr, mask);
}

int APS5_VABI scePthreadAttrSetcreatesuspend_nid_disambig1_nid_postfix(PthreadAttr* attr) {
    return scePthreadAttrSetcreatesuspend_nid_no_patch(attr);
}

int APS5_VABI scePthreadAttrSetdetachstate_nid_disambig1_nid_postfix(PthreadAttr* attr, int detachstate) {
    return scePthreadAttrSetdetachstate_nid_no_patch(attr, detachstate);
}

int APS5_VABI scePthreadAttrSetguardsize_nid_disambig1_nid_postfix(PthreadAttr* attr, size_t guard_size) {
    return scePthreadAttrSetguardsize_nid_no_patch(attr, guard_size);
}

int APS5_VABI scePthreadAttrSetinheritsched_nid_disambig1_nid_postfix(PthreadAttr* attr, int inherit_sched) {
    return scePthreadAttrSetinheritsched_nid_no_patch(attr, inherit_sched);
}

int APS5_VABI scePthreadAttrSetschedparam_nid_disambig1_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    return scePthreadAttrSetschedparam_nid_no_patch(attr, param);
}

int APS5_VABI scePthreadAttrSetschedpolicy_nid_disambig1_nid_postfix(PthreadAttr* attr, int policy) {
    return scePthreadAttrSetschedpolicy_nid_no_patch(attr, policy);
}

int APS5_VABI scePthreadAttrSetscope_nid_disambig1_nid_postfix(PthreadAttr* attr, int scope) {
    return scePthreadAttrSetscope_nid_no_patch(attr, scope);
}

int APS5_VABI scePthreadAttrSetsolosched_nid_disambig1_nid_postfix(PthreadAttr* attr, int solosched) {
    return scePthreadAttrSetsolosched_nid_no_patch(attr, solosched);
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

int APS5_VABI scePthreadBarrierDestroy_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier) {
    return scePthreadBarrierDestroy_nid_no_patch(barrier);
}

int APS5_VABI scePthreadBarrierInit_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count, const char* name) {
    return scePthreadBarrierInit_nid_no_patch(barrier, attr, count, name);
}

int APS5_VABI scePthreadBarrierWait_nid_disambig1_nid_postfix(PthreadBarrierPrivate** barrier) {
    return scePthreadBarrierWait_nid_no_patch(barrier);
}

int APS5_VABI scePthreadBarrierattrDestroy_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate** attr) {
    return scePthreadBarrierattrDestroy_nid_no_patch(attr);
}

int APS5_VABI scePthreadBarrierattrGetpshared_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    return scePthreadBarrierattrGetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadBarrierattrInit_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate** attr) {
    return scePthreadBarrierattrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadBarrierattrSetpshared_nid_disambig1_nid_postfix(PthreadBarrierattrPrivate** attr, int pshared) {
    return scePthreadBarrierattrSetpshared_nid_no_patch(attr, pshared);
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

int APS5_VABI scePthreadCondInit_nid_disambig1_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr, const char* a2) {
    return scePthreadCondInit_nid_no_patch(cond, attr, a2);
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

int APS5_VABI scePthreadCondattrGetclock_nid_disambig1_nid_postfix(const PthreadCondattr* attr, KernelClockid* clockId) {
    return scePthreadCondattrGetclock_nid_no_patch(attr, clockId);
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

int APS5_VABI scePthreadGetconcurrency_nid_disambig1_nid_postfix(void) {
    return scePthreadGetconcurrency_nid_no_patch();
}

int APS5_VABI scePthreadGetcpuclockid_nid_disambig1_nid_postfix(Pthread thread, int* clockId) {
    return scePthreadGetcpuclockid_nid_no_patch(thread, clockId);
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

int APS5_VABI scePthreadMain_nid_disambig1_nid_postfix(void) {
    return scePthreadMain_nid_no_patch();
}

int APS5_VABI scePthreadMulti_nid_disambig1_nid_postfix(void) {
    return scePthreadMulti_nid_no_patch();
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

int APS5_VABI scePthreadMutexInit_nid_disambig1_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* a2) {
    return scePthreadMutexInit_nid_no_patch(mutex, attr, a2);
}

int APS5_VABI scePthreadMutexInitForInternalLibc_nid_disambig1_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return scePthreadMutexInitForInternalLibc_nid_no_patch(mutex, attr);
}

int APS5_VABI scePthreadMutexIsowned_nid_disambig1_nid_postfix(PthreadMutex* mutex) {
    return scePthreadMutexIsowned_nid_no_patch(mutex);
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

int APS5_VABI scePthreadMutexattrGetgen_nid_disambig1_nid_postfix(const PthreadMutexattr* attr, int* generation) {
    return scePthreadMutexattrGetgen_nid_no_patch(attr, generation);
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

int APS5_VABI scePthreadMutexattrInitForInternalLibc_nid_disambig1_nid_postfix(PthreadMutexattr* attr) {
    return scePthreadMutexattrInitForInternalLibc_nid_no_patch(attr);
}

int APS5_VABI scePthreadMutexattrSetgen_nid_disambig1_nid_postfix(PthreadMutexattr* attr, int generation) {
    return scePthreadMutexattrSetgen_nid_no_patch(attr, generation);
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

int APS5_VABI scePthreadRegisterThreadDeathHook_nid_disambig1_nid_postfix(void) {
    return scePthreadRegisterThreadDeathHook_nid_no_patch();
}

int APS5_VABI scePthreadRename_nid_disambig1_nid_postfix(Pthread thread, const char* name) {
    return scePthreadRename_nid_no_patch(thread, name);
}

int APS5_VABI scePthreadResume_nid_disambig1_nid_postfix(Pthread thread) {
    return scePthreadResume_nid_no_patch(thread);
}

void APS5_VABI scePthreadResumeAll_nid_disambig1_nid_postfix(void) {
    scePthreadResumeAll_nid_no_patch();
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

int APS5_VABI scePthreadRwlockattrGettype_nid_disambig1_nid_postfix(const PthreadRwlockattr* attr, int* type) {
    return scePthreadRwlockattrGettype_nid_no_patch(attr, type);
}

int APS5_VABI scePthreadRwlockattrInit_nid_disambig1_nid_postfix(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrInit_nid_no_patch(attr);
}

int APS5_VABI scePthreadRwlockattrSetpshared_nid_disambig1_nid_postfix(PthreadRwlockattr* attr, int pshared) {
    return scePthreadRwlockattrSetpshared_nid_no_patch(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrSettype_nid_disambig1_nid_postfix(PthreadRwlockattr* attr, int type) {
    return scePthreadRwlockattrSettype_nid_no_patch(attr, type);
}

Pthread APS5_VABI scePthreadSelf_nid_disambig1_nid_postfix(void) {
    return scePthreadSelf_nid_no_patch();
}

int APS5_VABI scePthreadSemDestroy_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemDestroy_nid_no_patch(sem);
}

int APS5_VABI scePthreadSemGetvalue_nid_disambig1_nid_postfix(PthreadSem* sem, int* value) {
    return scePthreadSemGetvalue_nid_no_patch(sem, value);
}

int APS5_VABI scePthreadSemInit_nid_disambig1_nid_postfix(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    return scePthreadSemInit_nid_no_patch(sem, flag, value, name);
}

int APS5_VABI scePthreadSemPost_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemPost_nid_no_patch(sem);
}

int APS5_VABI scePthreadSemTimedwait_nid_disambig1_nid_postfix(PthreadSem* sem, KernelUseconds usec) {
    return scePthreadSemTimedwait_nid_no_patch(sem, usec);
}

int APS5_VABI scePthreadSemTrywait_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemTrywait_nid_no_patch(sem);
}

int APS5_VABI scePthreadSemWait_nid_disambig1_nid_postfix(PthreadSem* sem) {
    return scePthreadSemWait_nid_no_patch(sem);
}

int APS5_VABI scePthreadSetBesteffort_nid_disambig1_nid_postfix(void) {
    return scePthreadSetBesteffort_nid_no_patch();
}

int APS5_VABI scePthreadSetDefaultstacksize_nid_disambig1_nid_postfix(std::size_t size) {
    return scePthreadSetDefaultstacksize_nid_no_patch(size);
}

void APS5_VABI scePthreadSetName_nid_disambig1_nid_postfix(Pthread thread, const char* name) {
    scePthreadSetName_nid_no_patch(thread, name);
}

int APS5_VABI scePthreadSetaffinity_nid_disambig1_nid_postfix(Pthread thread, KernelCpumask mask) {
    return scePthreadSetaffinity_nid_no_patch(thread, mask);
}

int APS5_VABI scePthreadSetcancelstate_nid_disambig1_nid_postfix(int state, int* old_state) {
    return scePthreadSetcancelstate_nid_no_patch(state, old_state);
}

int APS5_VABI scePthreadSetcanceltype_nid_disambig1_nid_postfix(int type, int* old_type) {
    return scePthreadSetcanceltype_nid_no_patch(type, old_type);
}

int APS5_VABI scePthreadSetconcurrency_nid_disambig1_nid_postfix(int level) {
    return scePthreadSetconcurrency_nid_no_patch(level);
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

int APS5_VABI scePthreadSingle_nid_disambig1_nid_postfix(void) {
    return scePthreadSingle_nid_no_patch();
}

int APS5_VABI scePthreadSuspend_nid_disambig1_nid_postfix(void) {
    return scePthreadSuspend_nid_no_patch();
}

int APS5_VABI scePthreadSuspendAll_nid_disambig1_nid_postfix(void) {
    return scePthreadSuspendAll_nid_no_patch();
}

void APS5_VABI scePthreadTestcancel_nid_disambig1_nid_postfix(void) {
    scePthreadTestcancel_nid_no_patch();
}

int APS5_VABI scePthreadTimedjoin_nid_disambig1_nid_postfix(Pthread thread, void** value, KernelUseconds usec) {
    return scePthreadTimedjoin_nid_no_patch(thread, value, usec);
}

void APS5_VABI scePthreadYield_nid_disambig1_nid_postfix(void) {
    scePthreadYield_nid_no_patch();
}

int APS5_VABI sched_get_priority_max_nid_disambig1_nid_postfix(int policy) {
    return sched_get_priority_max_nid_no_patch(policy);
}

int APS5_VABI sched_get_priority_min_nid_disambig1_nid_postfix(int policy) {
    return sched_get_priority_min_nid_no_patch(policy);
}

int APS5_VABI sched_getparam_nid_disambig1_nid_postfix(int pid, KernelSchedParam* param) {
    return sched_getparam_nid_no_patch(pid, param);
}

int APS5_VABI sched_getscheduler_nid_disambig1_nid_postfix(int pid) {
    return sched_getscheduler_nid_no_patch(pid);
}

int APS5_VABI sched_rr_get_interval_nid_disambig1_nid_postfix(int pid, KernelTimespec* interval) {
    return sched_rr_get_interval_nid_no_patch(pid, interval);
}

int APS5_VABI sched_setparam_nid_disambig1_nid_postfix(int pid, const KernelSchedParam* param) {
    return sched_setparam_nid_no_patch(pid, param);
}

int APS5_VABI sched_setscheduler_nid_disambig1_nid_postfix(int pid, int a1, const KernelSchedParam* param) {
    return sched_setscheduler_nid_no_patch(pid, a1, param);
}

int APS5_VABI sched_yield_nid_disambig1_nid_postfix(void) {
    return sched_yield_nid_no_patch();
}

int APS5_VABI select_nid_disambig1_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    return select_nid_no_patch(nfds, readfds, writefds, exceptfds, timeout);
}

int APS5_VABI sem_close_nid_disambig1_nid_postfix(void* sem) {
    return sem_close_nid_no_patch(sem);
}

int APS5_VABI sem_destroy_nid_disambig1_nid_postfix(void* sem) {
    return sem_destroy_nid_no_patch(sem);
}

int APS5_VABI sem_getvalue_nid_disambig1_nid_postfix(void* sem, int* value) {
    return sem_getvalue_nid_no_patch(sem, value);
}

int APS5_VABI sem_init_nid_disambig1_nid_postfix(void* sem, int pshared, unsigned int value) {
    return sem_init_nid_no_patch(sem, pshared, value);
}

void* APS5_VABI sem_open_nid_disambig1_nid_postfix(const char* name, int flags, std::uint64_t v0, std::uint64_t v1) {
    return sem_open_nid_no_patch(name, flags, v0, v1);
}

int APS5_VABI sem_post_nid_disambig1_nid_postfix(void* sem) {
    return sem_post_nid_no_patch(sem);
}

int APS5_VABI sem_reltimedwait_np_nid_disambig1_nid_postfix(void* sem, uint32_t usec) {
    return sem_reltimedwait_np_nid_no_patch(sem, usec);
}

int APS5_VABI sem_setname_nid_disambig1_nid_postfix(void* sem, const char* name) {
    return sem_setname_nid_no_patch(sem, name);
}

int APS5_VABI sem_timedwait_nid_disambig1_nid_postfix(void* sem, const KernelTimespec* abstime) {
    return sem_timedwait_nid_no_patch(sem, abstime);
}

int APS5_VABI sem_trywait_nid_disambig1_nid_postfix(void* sem) {
    return sem_trywait_nid_no_patch(sem);
}

int APS5_VABI sem_unlink_nid_disambig1_nid_postfix(const char* name) {
    return sem_unlink_nid_no_patch(name);
}

int APS5_VABI sem_wait_nid_disambig1_nid_postfix(void* sem) {
    return sem_wait_nid_no_patch(sem);
}

std::int64_t APS5_VABI send_nid_disambig1_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags) {
    return send_nid_no_patch(descriptor, buffer, length, flags);
}

int APS5_VABI sendfile_nid_disambig1_nid_postfix(int file, int descriptor, std::int64_t offset, std::uint64_t length, const GuestSendfileHeaders* headers, std::int64_t* sbytes, int flags) {
    return sendfile_nid_no_patch(file, descriptor, offset, length, headers, sbytes, flags);
}

std::int64_t APS5_VABI sendmsg_nid_disambig1_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    return sendmsg_nid_no_patch(descriptor, message, flags);
}

std::int64_t APS5_VABI sendto_nid_disambig1_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags, const void* address, std::uint32_t addressLength) {
    return sendto_nid_no_patch(descriptor, buffer, length, flags, address, addressLength);
}

int APS5_VABI set_phys_fmem_limit_nid_disambig1_nid_postfix(void) {
    return set_phys_fmem_limit_nid_no_patch();
}

int APS5_VABI setcontext_nid_disambig1_nid_postfix(const UserUcontext* context) {
    return setcontext_nid_no_patch(context);
}

int APS5_VABI setegid_nid_disambig1_nid_postfix(std::uint32_t egid) {
    return setegid_nid_no_patch(egid);
}

int APS5_VABI seteuid_nid_disambig1_nid_postfix(std::uint32_t euid) {
    return seteuid_nid_no_patch(euid);
}

int APS5_VABI setgroups_nid_disambig1_nid_postfix(int count, const std::uint32_t* groups) {
    return setgroups_nid_no_patch(count, groups);
}

int APS5_VABI setitimer_nid_disambig1_nid_postfix(int which, const GuestItimerval* value, GuestItimerval* previous) {
    return setitimer_nid_no_patch(which, value, previous);
}

int APS5_VABI setpriority_nid_disambig1_nid_postfix(int which, int who, int niceval) {
    return setpriority_nid_no_patch(which, who, niceval);
}

int APS5_VABI setregid_nid_disambig1_nid_postfix(std::uint32_t rgid, std::uint32_t egid) {
    return setregid_nid_no_patch(rgid, egid);
}

int APS5_VABI setreuid_nid_disambig1_nid_postfix(std::uint32_t ruid, std::uint32_t euid) {
    return setreuid_nid_no_patch(ruid, euid);
}

int APS5_VABI setrlimit_nid_disambig1_nid_postfix(int resource, const GuestResourceLimit* limit) {
    return setrlimit_nid_no_patch(resource, limit);
}

int APS5_VABI setsockopt_nid_disambig1_nid_postfix(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return setsockopt_nid_no_patch(descriptor, level, option, value, length);
}

int APS5_VABI settimeofday_nid_disambig1_nid_postfix(const KernelTimeval* tv, const KernelTimezone* tz) {
    return settimeofday_nid_no_patch(tv, tz);
}

int APS5_VABI setuid_nid_disambig1_nid_postfix(std::uint32_t uid) {
    return setuid_nid_no_patch(uid);
}

int APS5_VABI shm_open_nid_disambig1_nid_postfix(const char* path, int flags, int mode) {
    return shm_open_nid_no_patch(path, flags, mode);
}

int APS5_VABI shm_unlink_nid_disambig1_nid_postfix(const char* path) {
    return shm_unlink_nid_no_patch(path);
}

int APS5_VABI shutdown_nid_disambig1_nid_postfix(int descriptor, int how) {
    return shutdown_nid_no_patch(descriptor, how);
}

int APS5_VABI sigaction_nid_disambig1_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    return sigaction_nid_no_patch(guest, action, previous);
}

int APS5_VABI sigaddset_nid_disambig1_nid_postfix(GuestSignalSet* set, int signal) {
    return sigaddset_nid_no_patch(set, signal);
}

int APS5_VABI sigaltstack_nid_disambig1_nid_postfix(const GuestStack* stack, GuestStack* previous) {
    return sigaltstack_nid_no_patch(stack, previous);
}

int APS5_VABI sigdelset_nid_disambig1_nid_postfix(GuestSignalSet* set, int signal) {
    return sigdelset_nid_no_patch(set, signal);
}

int APS5_VABI sigemptyset_nid_disambig1_nid_postfix(GuestSignalSet* set) {
    return sigemptyset_nid_no_patch(set);
}

int APS5_VABI sigfillset_nid_disambig1_nid_postfix(GuestSignalSet* set) {
    return sigfillset_nid_no_patch(set);
}

int APS5_VABI sigismember_nid_disambig1_nid_postfix(const GuestSignalSet* set, int signal) {
    return sigismember_nid_no_patch(set, signal);
}

GuestHandler APS5_VABI signal_nid_disambig1_nid_postfix(int guest, GuestHandler handler) {
    return signal_nid_no_patch(guest, handler);
}

int APS5_VABI sigpending_nid_disambig1_nid_postfix(GuestSignalSet* set) {
    return sigpending_nid_no_patch(set);
}

int APS5_VABI sigprocmask_nid_disambig1_nid_postfix(int how, const void* set, void* previousSet) {
    return sigprocmask_nid_no_patch(how, set, previousSet);
}

int APS5_VABI sigqueue_nid_disambig1_nid_postfix(void) {
    return sigqueue_nid_no_patch();
}

int APS5_VABI sigreturn_nid_disambig1_nid_postfix(const UserUcontext* context) {
    return sigreturn_nid_no_patch(context);
}

int APS5_VABI sigsuspend_nid_disambig1_nid_postfix(const GuestSignalSet* set) {
    return sigsuspend_nid_no_patch(set);
}

int APS5_VABI sigtimedwait_nid_disambig1_nid_postfix(const GuestSignalSet* set, void* info, const KernelTimespec* timeout) {
    return sigtimedwait_nid_no_patch(set, info, timeout);
}

int APS5_VABI sigwait_nid_disambig1_nid_postfix(const GuestSignalSet* set, int* received) {
    return sigwait_nid_no_patch(set, received);
}

int APS5_VABI sigwaitinfo_nid_disambig1_nid_postfix(const GuestSignalSet* set, void* info) {
    return sigwaitinfo_nid_no_patch(set, info);
}

unsigned int APS5_VABI sleep_nid_disambig1_nid_postfix(unsigned int seconds) {
    return sleep_nid_no_patch(seconds);
}

int APS5_VABI socket_nid_disambig1_nid_postfix(int family, int type, int protocol) {
    return socket_nid_no_patch(family, type, protocol);
}

int APS5_VABI socketpair_nid_disambig1_nid_postfix(int family, int type, int protocol, int* descriptors) {
    return socketpair_nid_no_patch(family, type, protocol, descriptors);
}

int APS5_VABI stat_nid_disambig1_nid_postfix(const char* path, FileStat* sb) {
    return stat_nid_no_patch(path, sb);
}

void APS5_VABI sync_nid_disambig1_nid_postfix(void) {
    sync_nid_no_patch();
}

int APS5_VABI sysKernelGetIntdevModeForInternalLibc_nid_disambig1_nid_postfix(void) {
    return sysKernelGetIntdevModeForInternalLibc_nid_no_patch();
}

int APS5_VABI sysKernelGetIntdevModeForRcmgr_nid_disambig1_nid_postfix(void) {
    return sysKernelGetIntdevModeForRcmgr_nid_no_patch();
}

int APS5_VABI sysKernelGetLowerLimitSysexVersion_nid_disambig1_nid_postfix(char* version) {
    return sysKernelGetLowerLimitSysexVersion_nid_no_patch(version);
}

int APS5_VABI sysKernelGetLowerLimitUpdVersion_nid_disambig1_nid_postfix(std::uint32_t* version) {
    return sysKernelGetLowerLimitUpdVersion_nid_no_patch(version);
}

int APS5_VABI sysKernelGetManufacturingMode_nid_disambig1_nid_postfix(void) {
    return sysKernelGetManufacturingMode_nid_no_patch();
}

int APS5_VABI sysKernelGetUpdVersion_nid_disambig1_nid_postfix(std::uint32_t* version) {
    return sysKernelGetUpdVersion_nid_no_patch(version);
}

int APS5_VABI sysarch_nid_disambig1_nid_postfix(int operation, void* parameters) {
    return sysarch_nid_no_patch(operation, parameters);
}

int APS5_VABI sysc_e00_nid_disambig1_nid_postfix(void) {
    return sysc_e00_nid_no_patch();
}

int APS5_VABI sysc_m00_nid_disambig1_nid_postfix(void) {
    return sysc_m00_nid_no_patch();
}

int APS5_VABI sysc_s00_nid_disambig1_nid_postfix(void) {
    return sysc_s00_nid_no_patch();
}

std::int64_t APS5_VABI sysconf_nid_disambig1_nid_postfix(int name) {
    return sysconf_nid_no_patch(name);
}

int APS5_VABI sysctl_nid_disambig1_nid_postfix(const int* name, std::uint32_t nameLength, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    return sysctl_nid_no_patch(name, nameLength, oldValue, oldLength, newValue, newLength);
}

int APS5_VABI sysctlbyname_nid_disambig1_nid_postfix(const char* name, void* oldValue, std::size_t* oldLength, const void* newValue, std::size_t newLength) {
    return sysctlbyname_nid_no_patch(name, oldValue, oldLength, newValue, newLength);
}

int APS5_VABI sysctlnametomib_nid_disambig1_nid_postfix(const char* name, int* mib, std::size_t* size) {
    return sysctlnametomib_nid_no_patch(name, mib, size);
}

int APS5_VABI system_nid_disambig1_nid_postfix(const char* command) {
    return system_nid_no_patch(command);
}

int APS5_VABI tcdrain_nid_disambig1_nid_postfix(int descriptor) {
    return tcdrain_nid_no_patch(descriptor);
}

int APS5_VABI tcflow_nid_disambig1_nid_postfix(int descriptor, int action) {
    return tcflow_nid_no_patch(descriptor, action);
}

int APS5_VABI tcflush_nid_disambig1_nid_postfix(int descriptor, int queue) {
    return tcflush_nid_no_patch(descriptor, queue);
}

int APS5_VABI tcgetattr_nid_disambig1_nid_postfix(int descriptor, void* attributes) {
    return tcgetattr_nid_no_patch(descriptor, attributes);
}

int APS5_VABI tcgetpgrp_nid_disambig1_nid_postfix(int descriptor) {
    return tcgetpgrp_nid_no_patch(descriptor);
}

int APS5_VABI tcgetsid_nid_disambig1_nid_postfix(int descriptor) {
    return tcgetsid_nid_no_patch(descriptor);
}

int APS5_VABI tcsendbreak_nid_disambig1_nid_postfix(int descriptor, int duration) {
    return tcsendbreak_nid_no_patch(descriptor, duration);
}

int APS5_VABI tcsetattr_nid_disambig1_nid_postfix(int descriptor, int action, const void* attributes) {
    return tcsetattr_nid_no_patch(descriptor, action, attributes);
}

int APS5_VABI tcsetpgrp_nid_disambig1_nid_postfix(int descriptor, int group) {
    return tcsetpgrp_nid_no_patch(descriptor, group);
}

int APS5_VABI tcsetsid_nid_disambig1_nid_postfix(int fd, int pid) {
    return tcsetsid_nid_no_patch(fd, pid);
}

int APS5_VABI truncate_nid_disambig1_nid_postfix(const char* path, std::int64_t length) {
    return truncate_nid_no_patch(path, length);
}

int APS5_VABI unlink_nid_disambig1_nid_postfix(const char* path) {
    return unlink_nid_no_patch(path);
}

int APS5_VABI usleep_nid_disambig1_nid_postfix(KernelUseconds microseconds) {
    return usleep_nid_no_patch(microseconds);
}

int APS5_VABI utimes_nid_disambig1_nid_postfix(const char* path, const KernelTimeval* times) {
    return utimes_nid_no_patch(path, times);
}

int APS5_VABI uuidgen_nid_disambig1_nid_postfix(GuestUuid* store, int count) {
    return uuidgen_nid_no_patch(store, count);
}

int APS5_VABI wait_nid_disambig1_nid_postfix(int* status) {
    return wait_nid_no_patch(status);
}

int APS5_VABI wait3_nid_disambig1_nid_postfix(void) {
    return wait3_nid_no_patch();
}

int APS5_VABI waitpid_nid_disambig1_nid_postfix(int pid, int* status, int options) {
    return waitpid_nid_no_patch(pid, status, options);
}

int64_t APS5_VABI write_nid_disambig1_nid_postfix(int d, const char* str, int64_t size) {
    return write_nid_no_patch(d, str, size);
}

int64_t APS5_VABI writev_nid_disambig1_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return writev_nid_no_patch(d, iov, iovcnt);
}

int APS5_VABI libkernel_webUnknown__plus_MZfwzH65JQ_nid_no_patch() {
    return libkernel_sysUnknown__plus_MZfwzH65JQ_nid_no_patch();
}
APS5_EXPORT("+MZfwzH65JQ", libkernel_webUnknown__plus_MZfwzH65JQ_nid_no_patch);

int APS5_VABI libkernel_webUnknown__minus_PNB3tfnD8c_nid_no_patch() {
    return libkernel_sysUnknown__minus_PNB3tfnD8c_nid_no_patch();
}
APS5_EXPORT("-PNB3tfnD8c", libkernel_webUnknown__minus_PNB3tfnD8c_nid_no_patch);

std::uint64_t APS5_VABI libkernel_webUnknown_1WXQlxMpnNA_nid_no_patch() {
    return libkernel_sysUnknown_1WXQlxMpnNA_nid_no_patch();
}
APS5_EXPORT("1WXQlxMpnNA", libkernel_webUnknown_1WXQlxMpnNA_nid_no_patch);

int APS5_VABI libkernel_webUnknown_1v23w7Rnkic_nid_no_patch() {
    return libkernel_sysUnknown_1v23w7Rnkic_nid_no_patch();
}
APS5_EXPORT("1v23w7Rnkic", libkernel_webUnknown_1v23w7Rnkic_nid_no_patch);

int APS5_VABI libkernel_webUnknown_1v6DjTPunjs_nid_no_patch() {
    return libkernel_sysUnknown_1v6DjTPunjs_nid_no_patch();
}
APS5_EXPORT("1v6DjTPunjs", libkernel_webUnknown_1v6DjTPunjs_nid_no_patch);

int APS5_VABI libkernel_webUnknown_2_plus_KZ4mZKp6E_nid_no_patch() {
    return libkernel_sysUnknown_2_plus_KZ4mZKp6E_nid_no_patch();
}
APS5_EXPORT("2+KZ4mZKp6E", libkernel_webUnknown_2_plus_KZ4mZKp6E_nid_no_patch);

int APS5_VABI libkernel_webUnknown_2wKjID0iGVM_nid_no_patch() {
    return libkernel_sysUnknown_2wKjID0iGVM_nid_no_patch();
}
APS5_EXPORT("2wKjID0iGVM", libkernel_webUnknown_2wKjID0iGVM_nid_no_patch);

int APS5_VABI libkernel_webUnknown_2xg4JXvaJ6k_nid_no_patch() {
    return libkernel_sysUnknown_2xg4JXvaJ6k_nid_no_patch();
}
APS5_EXPORT("2xg4JXvaJ6k", libkernel_webUnknown_2xg4JXvaJ6k_nid_no_patch);

int APS5_VABI libkernel_webUnknown_3BwLcdWeZLs_nid_no_patch() {
    return libkernel_sysUnknown_3BwLcdWeZLs_nid_no_patch();
}
APS5_EXPORT("3BwLcdWeZLs", libkernel_webUnknown_3BwLcdWeZLs_nid_no_patch);

int APS5_VABI libkernel_webUnknown_3jw_minus_vq4gYLs_nid_no_patch() {
    return libkernel_sysUnknown_3jw_minus_vq4gYLs_nid_no_patch();
}
APS5_EXPORT("3jw-vq4gYLs", libkernel_webUnknown_3jw_minus_vq4gYLs_nid_no_patch);

void APS5_VABI _exit_nid_disambig1_nid_postfix(int status) {
    _exit_nid_no_patch(status);
}

int APS5_VABI libkernel_webUnknown_7BUHmsuhcq4_nid_no_patch() {
    return libkernel_sysUnknown_7BUHmsuhcq4_nid_no_patch();
}
APS5_EXPORT("7BUHmsuhcq4", libkernel_webUnknown_7BUHmsuhcq4_nid_no_patch);

int APS5_VABI libkernel_webUnknown_7Sd1vnnqEqU_nid_no_patch() {
    return libkernel_sysUnknown_7Sd1vnnqEqU_nid_no_patch();
}
APS5_EXPORT("7Sd1vnnqEqU", libkernel_webUnknown_7Sd1vnnqEqU_nid_no_patch);

int APS5_VABI libkernel_webUnknown_8U0nlN0TpDU_nid_no_patch() {
    return libkernel_sysUnknown_8U0nlN0TpDU_nid_no_patch();
}
APS5_EXPORT("8U0nlN0TpDU", libkernel_webUnknown_8U0nlN0TpDU_nid_no_patch);

int APS5_VABI libkernel_webUnknown_9ncS0kCRRSA_nid_no_patch() {
    return libkernel_sysUnknown_9ncS0kCRRSA_nid_no_patch();
}
APS5_EXPORT("9ncS0kCRRSA", libkernel_webUnknown_9ncS0kCRRSA_nid_no_patch);

int APS5_VABI libkernel_webUnknown_A_plus_S5E4RCh7Y_nid_no_patch() {
    return libkernel_sysUnknown_A_plus_S5E4RCh7Y_nid_no_patch();
}
APS5_EXPORT("A+S5E4RCh7Y", libkernel_webUnknown_A_plus_S5E4RCh7Y_nid_no_patch);

int APS5_VABI libkernel_webUnknown_B70jHzr4Jj4_nid_no_patch() {
    return libkernel_sysUnknown_B70jHzr4Jj4_nid_no_patch();
}
APS5_EXPORT("B70jHzr4Jj4", libkernel_webUnknown_B70jHzr4Jj4_nid_no_patch);

int APS5_VABI libkernel_webUnknown_BJvWKKfmj4w_nid_no_patch() {
    return libkernel_sysUnknown_BJvWKKfmj4w_nid_no_patch();
}
APS5_EXPORT("BJvWKKfmj4w", libkernel_webUnknown_BJvWKKfmj4w_nid_no_patch);

void* APS5_VABI mmap_nid_disambig1_nid_postfix(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) {
    return mmap_nid_no_patch(address, length, protection, flags, descriptor, offset);
}

int APS5_VABI libkernel_webUnknown_BjmZn9pYSuk_nid_no_patch() {
    return libkernel_sysUnknown_BjmZn9pYSuk_nid_no_patch();
}
APS5_EXPORT("BjmZn9pYSuk", libkernel_webUnknown_BjmZn9pYSuk_nid_no_patch);

int APS5_VABI libkernel_webUnknown_C3ToW2CA_plus_hM_nid_no_patch() {
    return libkernel_sysUnknown_C3ToW2CA_plus_hM_nid_no_patch();
}
APS5_EXPORT("C3ToW2CA+hM", libkernel_webUnknown_C3ToW2CA_plus_hM_nid_no_patch);

int APS5_VABI libkernel_webUnknown_E58GHGcQT7I_nid_no_patch() {
    return libkernel_sysUnknown_E58GHGcQT7I_nid_no_patch();
}
APS5_EXPORT("E58GHGcQT7I", libkernel_webUnknown_E58GHGcQT7I_nid_no_patch);

std::uint64_t APS5_VABI libkernel_webUnknown_FR0odl35yQY_nid_no_patch() {
    return libkernel_sysUnknown_FR0odl35yQY_nid_no_patch();
}
APS5_EXPORT("FR0odl35yQY", libkernel_webUnknown_FR0odl35yQY_nid_no_patch);

int APS5_VABI libkernel_webUnknown_IJQeoUQ3pb4_nid_no_patch() {
    return libkernel_sysUnknown_IJQeoUQ3pb4_nid_no_patch();
}
APS5_EXPORT("IJQeoUQ3pb4", libkernel_webUnknown_IJQeoUQ3pb4_nid_no_patch);

int APS5_VABI libkernel_webUnknown_Ipau2b5eaHM_nid_no_patch() {
    return libkernel_sysUnknown_Ipau2b5eaHM_nid_no_patch();
}
APS5_EXPORT("Ipau2b5eaHM", libkernel_webUnknown_Ipau2b5eaHM_nid_no_patch);

int APS5_VABI libkernel_webUnknown_Ishr1Sdu024_nid_no_patch() {
    return libkernel_sysUnknown_Ishr1Sdu024_nid_no_patch();
}
APS5_EXPORT("Ishr1Sdu024", libkernel_webUnknown_Ishr1Sdu024_nid_no_patch);

int APS5_VABI sceCoredumpGetStopInfoGpu_Agc_nid_disambig1_nid_postfix(void* info, size_t size) {
    return sceCoredumpGetStopInfoGpu_Agc_nid_no_patch(info, size);
}

int APS5_VABI libkernel_webUnknown_KoDy4Jazz40_nid_no_patch() {
    return libkernel_sysUnknown_KoDy4Jazz40_nid_no_patch();
}
APS5_EXPORT("KoDy4Jazz40", libkernel_webUnknown_KoDy4Jazz40_nid_no_patch);

int APS5_VABI libkernel_webUnknown_MKlp_minus_bW1bz8_nid_no_patch() {
    return libkernel_sysUnknown_MKlp_minus_bW1bz8_nid_no_patch();
}
APS5_EXPORT("MKlp-bW1bz8", libkernel_webUnknown_MKlp_minus_bW1bz8_nid_no_patch);

std::uint64_t APS5_VABI libkernel_webUnknown_N94a0T4F4_plus_U_nid_no_patch() {
    return libkernel_sysUnknown_N94a0T4F4_plus_U_nid_no_patch();
}
APS5_EXPORT("N94a0T4F4+U", libkernel_webUnknown_N94a0T4F4_plus_U_nid_no_patch);

int APS5_VABI rename_nid_disambig1_nid_postfix(const char* from, const char* to) {
    return rename_nid_no_patch(from, to);
}

[[noreturn]] void APS5_VABI __stack_chk_fail_nid_disambig2_nid_postfix(void) {
    __stack_chk_fail_nid_no_patch();
}

int APS5_VABI libkernel_webUnknown_R3QLLyp9MEU_nid_no_patch() {
    return libkernel_sysUnknown_R3QLLyp9MEU_nid_no_patch();
}
APS5_EXPORT("R3QLLyp9MEU", libkernel_webUnknown_R3QLLyp9MEU_nid_no_patch);

int APS5_VABI libkernel_webUnknown_RA5mXLPd5V4_nid_no_patch() {
    return libkernel_sysUnknown_RA5mXLPd5V4_nid_no_patch();
}
APS5_EXPORT("RA5mXLPd5V4", libkernel_webUnknown_RA5mXLPd5V4_nid_no_patch);

int APS5_VABI libkernel_webUnknown_RDjusrvQ_minus_44_nid_no_patch() {
    return libkernel_sysUnknown_RDjusrvQ_minus_44_nid_no_patch();
}
APS5_EXPORT("RDjusrvQ-44", libkernel_webUnknown_RDjusrvQ_minus_44_nid_no_patch);

int APS5_VABI libkernel_webUnknown_RfzgytiWey0_nid_no_patch() {
    return libkernel_sysUnknown_RfzgytiWey0_nid_no_patch();
}
APS5_EXPORT("RfzgytiWey0", libkernel_webUnknown_RfzgytiWey0_nid_no_patch);

int APS5_VABI pthread_atfork_nid_disambig1_nid_postfix(PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child) {
    return pthread_atfork_nid_no_patch(prepare, parent, child);
}

int APS5_VABI munmap_nid_disambig1_nid_postfix(void* address, std::size_t length) {
    return munmap_nid_no_patch(address, length);
}

int APS5_VABI libkernel_webUnknown_V4f3CEBQyuM_nid_no_patch() {
    return libkernel_sysUnknown_V4f3CEBQyuM_nid_no_patch();
}
APS5_EXPORT("V4f3CEBQyuM", libkernel_webUnknown_V4f3CEBQyuM_nid_no_patch);

int APS5_VABI libkernel_webUnknown_VU1YkdCmYto_nid_no_patch() {
    return libkernel_sysUnknown_VU1YkdCmYto_nid_no_patch();
}
APS5_EXPORT("VU1YkdCmYto", libkernel_webUnknown_VU1YkdCmYto_nid_no_patch);

int APS5_VABI libkernel_webUnknown_WtA8Aq_minus_0cyY_nid_no_patch() {
    return libkernel_sysUnknown_WtA8Aq_minus_0cyY_nid_no_patch();
}
APS5_EXPORT("WtA8Aq-0cyY", libkernel_webUnknown_WtA8Aq_minus_0cyY_nid_no_patch);

int APS5_VABI libkernel_webUnknown_WyZULCQhjdY_nid_no_patch() {
    return libkernel_sysUnknown_WyZULCQhjdY_nid_no_patch();
}
APS5_EXPORT("WyZULCQhjdY", libkernel_webUnknown_WyZULCQhjdY_nid_no_patch);

int APS5_VABI libkernel_webUnknown_X4R_minus_jzzFYac_nid_no_patch() {
    return libkernel_sysUnknown_X4R_minus_jzzFYac_nid_no_patch();
}
APS5_EXPORT("X4R-jzzFYac", libkernel_webUnknown_X4R_minus_jzzFYac_nid_no_patch);

int APS5_VABI mprotect_nid_disambig1_nid_postfix(void* address, std::size_t length, int protection) {
    return mprotect_nid_no_patch(address, length, protection);
}

int APS5_VABI libkernel_webUnknown_aL9DwvoSDzs_nid_no_patch() {
    return libkernel_sysUnknown_aL9DwvoSDzs_nid_no_patch();
}
APS5_EXPORT("aL9DwvoSDzs", libkernel_webUnknown_aL9DwvoSDzs_nid_no_patch);

int APS5_VABI libkernel_webUnknown_ak6t3JzhOtg_nid_no_patch() {
    return libkernel_sysUnknown_ak6t3JzhOtg_nid_no_patch();
}
APS5_EXPORT("ak6t3JzhOtg", libkernel_webUnknown_ak6t3JzhOtg_nid_no_patch);

int APS5_VABI libkernel_webUnknown_bQ_plus_8uJVyxII_nid_no_patch() {
    return libkernel_sysUnknown_bQ_plus_8uJVyxII_nid_no_patch();
}
APS5_EXPORT("bQ+8uJVyxII", libkernel_webUnknown_bQ_plus_8uJVyxII_nid_no_patch);

int APS5_VABI libkernel_webUnknown_cH6vM6sF5YA_nid_no_patch() {
    return libkernel_sysUnknown_cH6vM6sF5YA_nid_no_patch();
}
APS5_EXPORT("cH6vM6sF5YA", libkernel_webUnknown_cH6vM6sF5YA_nid_no_patch);

int APS5_VABI libkernel_webUnknown_cReByBlM5XE_nid_no_patch() {
    return libkernel_sysUnknown_cReByBlM5XE_nid_no_patch();
}
APS5_EXPORT("cReByBlM5XE", libkernel_webUnknown_cReByBlM5XE_nid_no_patch);

int APS5_VABI libkernel_webUnknown_cafDxa64sO4_nid_no_patch() {
    return libkernel_sysUnknown_cafDxa64sO4_nid_no_patch();
}
APS5_EXPORT("cafDxa64sO4", libkernel_webUnknown_cafDxa64sO4_nid_no_patch);

int APS5_VABI libkernel_webUnknown_culoiinnxDA_nid_no_patch() {
    return libkernel_sysUnknown_culoiinnxDA_nid_no_patch();
}
APS5_EXPORT("culoiinnxDA", libkernel_webUnknown_culoiinnxDA_nid_no_patch);

int APS5_VABI libkernel_webUnknown_gSIiye3ERqo_nid_no_patch() {
    return libkernel_sysUnknown_gSIiye3ERqo_nid_no_patch();
}
APS5_EXPORT("gSIiye3ERqo", libkernel_webUnknown_gSIiye3ERqo_nid_no_patch);

int APS5_VABI libkernel_webUnknown_gpgi_minus_GwE2Is_nid_no_patch() {
    return libkernel_sysUnknown_gpgi_minus_GwE2Is_nid_no_patch();
}
APS5_EXPORT("gpgi-GwE2Is", libkernel_webUnknown_gpgi_minus_GwE2Is_nid_no_patch);

int APS5_VABI libkernel_webUnknown_gvq6XRoDogE_nid_no_patch() {
    return libkernel_sysUnknown_gvq6XRoDogE_nid_no_patch();
}
APS5_EXPORT("gvq6XRoDogE", libkernel_webUnknown_gvq6XRoDogE_nid_no_patch);

int APS5_VABI libkernel_webUnknown_h26nEKnW8VI_nid_no_patch() {
    return libkernel_sysUnknown_h26nEKnW8VI_nid_no_patch();
}
APS5_EXPORT("h26nEKnW8VI", libkernel_webUnknown_h26nEKnW8VI_nid_no_patch);

int APS5_VABI libkernel_webUnknown_hmqw8GlN_plus_tI_nid_no_patch() {
    return libkernel_sysUnknown_hmqw8GlN_plus_tI_nid_no_patch();
}
APS5_EXPORT("hmqw8GlN+tI", libkernel_webUnknown_hmqw8GlN_plus_tI_nid_no_patch);

int APS5_VABI libkernel_webUnknown_jESbu_plus_uFJYU_nid_no_patch() {
    return libkernel_sysUnknown_jESbu_plus_uFJYU_nid_no_patch();
}
APS5_EXPORT("jESbu+uFJYU", libkernel_webUnknown_jESbu_plus_uFJYU_nid_no_patch);

int APS5_VABI libkernel_webUnknown_jMuNoBRCPUg_nid_no_patch() {
    return libkernel_sysUnknown_jMuNoBRCPUg_nid_no_patch();
}
APS5_EXPORT("jMuNoBRCPUg", libkernel_webUnknown_jMuNoBRCPUg_nid_no_patch);

int APS5_VABI libkernel_webUnknown_k_plus_J3KBjfkNk_nid_no_patch() {
    return libkernel_sysUnknown_k_plus_J3KBjfkNk_nid_no_patch();
}
APS5_EXPORT("k+J3KBjfkNk", libkernel_webUnknown_k_plus_J3KBjfkNk_nid_no_patch);

int64_t APS5_VABI sceKernelWritev_nid_disambig1_nid_postfix(int d, const KernelIovec* guestIov, int iovcnt) {
    return sceKernelWritev_nid_no_patch(d, guestIov, iovcnt);
}

int APS5_VABI libkernel_webUnknown_km8Fr2Jn4Gg_nid_no_patch() {
    return libkernel_sysUnknown_km8Fr2Jn4Gg_nid_no_patch();
}
APS5_EXPORT("km8Fr2Jn4Gg", libkernel_webUnknown_km8Fr2Jn4Gg_nid_no_patch);

int64_t APS5_VABI sceKernelPwritev_nid_disambig1_nid_postfix(int d, const KernelIovec* guestIov, int iovcnt, int64_t offset) {
    return sceKernelPwritev_nid_no_patch(d, guestIov, iovcnt, offset);
}

int APS5_VABI libkernel_webUnknown_mIkJUpP5vKo_nid_no_patch() {
    return libkernel_sysUnknown_mIkJUpP5vKo_nid_no_patch();
}
APS5_EXPORT("mIkJUpP5vKo", libkernel_webUnknown_mIkJUpP5vKo_nid_no_patch);

int APS5_VABI libkernel_webUnknown_ncrollDo5u4_nid_no_patch() {
    return libkernel_sysUnknown_ncrollDo5u4_nid_no_patch();
}
APS5_EXPORT("ncrollDo5u4", libkernel_webUnknown_ncrollDo5u4_nid_no_patch);

int APS5_VABI libkernel_webUnknown_nsq826qQ2EI_nid_no_patch() {
    return libkernel_sysUnknown_nsq826qQ2EI_nid_no_patch();
}
APS5_EXPORT("nsq826qQ2EI", libkernel_webUnknown_nsq826qQ2EI_nid_no_patch);

void APS5_VABI siglongjmp_nid_disambig1_nid_postfix(std::uint64_t* buffer, int value) {
    siglongjmp_nid_no_patch(buffer, value);
}

int APS5_VABI libkernel_webUnknown_q_minus_2Uxlg_minus_faU_nid_no_patch() {
    return libkernel_sysUnknown_q_minus_2Uxlg_minus_faU_nid_no_patch();
}
APS5_EXPORT("q-2Uxlg-faU", libkernel_webUnknown_q_minus_2Uxlg_minus_faU_nid_no_patch);

int APS5_VABI libkernel_webUnknown_qGzuTSD9cPQ_nid_no_patch() {
    return libkernel_sysUnknown_qGzuTSD9cPQ_nid_no_patch();
}
APS5_EXPORT("qGzuTSD9cPQ", libkernel_webUnknown_qGzuTSD9cPQ_nid_no_patch);

int APS5_VABI libkernel_webUnknown_qHVs_plus_dxCumg_nid_no_patch() {
    return libkernel_sysUnknown_qHVs_plus_dxCumg_nid_no_patch();
}
APS5_EXPORT("qHVs+dxCumg", libkernel_webUnknown_qHVs_plus_dxCumg_nid_no_patch);

int APS5_VABI libkernel_webUnknown_qtHOLtFWq7Y_nid_no_patch() {
    return libkernel_sysUnknown_qtHOLtFWq7Y_nid_no_patch();
}
APS5_EXPORT("qtHOLtFWq7Y", libkernel_webUnknown_qtHOLtFWq7Y_nid_no_patch);

int APS5_VABI libkernel_webUnknown_rLBfsmdAuvo_nid_no_patch() {
    return libkernel_sysUnknown_rLBfsmdAuvo_nid_no_patch();
}
APS5_EXPORT("rLBfsmdAuvo", libkernel_webUnknown_rLBfsmdAuvo_nid_no_patch);

int APS5_VABI libkernel_webUnknown_tkZiGUoRJok_nid_no_patch() {
    return libkernel_sysUnknown_tkZiGUoRJok_nid_no_patch();
}
APS5_EXPORT("tkZiGUoRJok", libkernel_webUnknown_tkZiGUoRJok_nid_no_patch);

int APS5_VABI libkernel_webUnknown_uV_plus_YilcHqAs_nid_no_patch() {
    return libkernel_sysUnknown_uV_plus_YilcHqAs_nid_no_patch();
}
APS5_EXPORT("uV+YilcHqAs", libkernel_webUnknown_uV_plus_YilcHqAs_nid_no_patch);

int APS5_VABI libkernel_webUnknown_ulVjluL9TzM_nid_no_patch() {
    return libkernel_sysUnknown_ulVjluL9TzM_nid_no_patch();
}
APS5_EXPORT("ulVjluL9TzM", libkernel_webUnknown_ulVjluL9TzM_nid_no_patch);

int APS5_VABI libkernel_webUnknown_v_plus_qTlNjF6kE_nid_no_patch() {
    return libkernel_sysUnknown_v_plus_qTlNjF6kE_nid_no_patch();
}
APS5_EXPORT("v+qTlNjF6kE", libkernel_webUnknown_v_plus_qTlNjF6kE_nid_no_patch);

int APS5_VABI libkernel_webUnknown_vfYaxkxnJXQ_nid_no_patch() {
    return libkernel_sysUnknown_vfYaxkxnJXQ_nid_no_patch();
}
APS5_EXPORT("vfYaxkxnJXQ", libkernel_webUnknown_vfYaxkxnJXQ_nid_no_patch);

int APS5_VABI libkernel_webUnknown_xpB6yv8NhhE_nid_no_patch() {
    return libkernel_sysUnknown_xpB6yv8NhhE_nid_no_patch();
}
APS5_EXPORT("xpB6yv8NhhE", libkernel_webUnknown_xpB6yv8NhhE_nid_no_patch);

int64_t APS5_VABI sceKernelPreadv_nid_disambig1_nid_postfix(int d, const KernelIovec* guestIov, int iovcnt, int64_t offset) {
    return sceKernelPreadv_nid_no_patch(d, guestIov, iovcnt, offset);
}

int APS5_VABI pthread_setschedother_np_nid_disambig1_nid_postfix(void) {
    return pthread_setschedother_np_nid_no_patch();
}

int APS5_VABI libkernel_webUnknown_z0qKN5kkVj0_nid_no_patch() {
    return libkernel_sysUnknown_z0qKN5kkVj0_nid_no_patch();
}
APS5_EXPORT("z0qKN5kkVj0", libkernel_webUnknown_z0qKN5kkVj0_nid_no_patch);

}
