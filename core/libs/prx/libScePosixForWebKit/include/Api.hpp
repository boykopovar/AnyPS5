#pragma once

#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include "prx/libSceLibcInternal/include/Statvfs.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct PosixForWebKitSysinfo {
    std::uint64_t total;
    std::uint32_t one;
};

struct PosixForWebKitPasswd {
    char* pw_name;
    char* pw_passwd;
    std::uint32_t pw_uid;
    std::uint32_t pw_gid;
    std::int64_t pw_change;
    char* pw_class;
    char* pw_gecos;
    char* pw_dir;
    char* pw_shell;
    std::int64_t pw_expire;
    int pw_fields;
};

static_assert(sizeof(PosixForWebKitPasswd) == 80);
static_assert(offsetof(PosixForWebKitPasswd, pw_change) == 24);
static_assert(offsetof(PosixForWebKitPasswd, pw_expire) == 64);
static_assert(offsetof(PosixForWebKitPasswd, pw_fields) == 72);

extern "C" {

PosixForWebKitPasswd* APS5_VABI getpwuid_nid_postfix(std::uint32_t uid);
PosixForWebKitPasswd* APS5_VABI getpwent_nid_postfix();
void APS5_VABI setpwent_nid_postfix();
void APS5_VABI endpwent_nid_postfix();

void* APS5_VABI opendir_nid_disambig1_nid_postfix(const char* path);
int APS5_VABI closedir_nid_disambig1_nid_postfix(void* handle);
GuestDirectoryEntry* APS5_VABI readdir_nid_disambig1_nid_postfix(void* handle);
int APS5_VABI readdir_r_nid_disambig1_nid_postfix(void* handle, GuestDirectoryEntry* entry, GuestDirectoryEntry** result);
int APS5_VABI fnmatch_nid_postfix(const char* pattern, const char* string, int flags);
std::uint32_t APS5_VABI ualarm_nid_postfix(std::uint32_t usecs, std::uint32_t reload);
int APS5_VABI nftw_nid_postfix(const char* path, int (APS5_VABI *fn)(const char*, const void*, int, void*), int nopenfd, int flags);
char* APS5_VABI strcasestr_nid_postfix(const char* text, const char* needle);
void APS5_VABI arc4random_buf_nid_postfix(void* buffer, std::size_t size);
std::uint32_t APS5_VABI arc4random_nid_postfix();
std::uint32_t APS5_VABI arc4random_uniform_nid_postfix(std::uint32_t upper_bound);
int APS5_VABI __wrap_access(const char* path, int mode);
int APS5_VABI __wrap_clock_gettime(int clockId, KernelTimespec* tp);
int APS5_VABI __wrap_fchown(int descriptor, std::uint32_t owner, std::uint32_t group);
char* APS5_VABI __wrap_getcwd(char* buffer, std::size_t size);
char* APS5_VABI __wrap_getenv(const char* name);
int APS5_VABI __wrap_link(const char* existing, const char* created);
int APS5_VABI __wrap_lstat(const char* path, FileStat* sb);
void* APS5_VABI __wrap_mmap(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset);
std::int64_t APS5_VABI __wrap_readlink(const char* path, char* buffer, std::size_t size);
int APS5_VABI __wrap_statvfs(const char* path, LibcStatvfs* status);
int APS5_VABI __wrap_symlink(const char* target, const char* created);
int APS5_VABI clock_gettime_np(int clockId, KernelTimespec* tp);
int APS5_VABI getenv_np(const char* name, char* buffer, std::size_t size);
int GetenvNp_nid_no_patch(const char* name, char* buffer, std::size_t size);
int APS5_VABI pthread_setname_np_nid_postfix(Pthread thread, const char* name);
void* APS5_VABI mmap_np(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset, const char* name);
int APS5_VABI msetname(void* address, std::size_t length, const char* name);
int APS5_VABI pthread_setschedparam_np(Pthread thread, int policy, const KernelSchedParam* param);
int APS5_VABI setenv_np(const char* name, const char* value, int overwrite);
int APS5_VABI scePosixForWebKit_sysinfo(PosixForWebKitSysinfo* info);
int APS5_VABI mkostemp_nid_postfix(char* pattern, int flags);
int APS5_VABI mkstemp_nid_postfix(char* pattern);
int APS5_VABI isatty_nid_postfix(int descriptor);
GuestResolver::HostEntry* APS5_VABI gethostbyname_nid_postfix(const char* name);
int APS5_VABI scePosixForWebKit_gethostbyname_r(const char* name, GuestResolver::HostEntry* entry, char* buffer, std::size_t length, GuestResolver::HostEntry** result, int* hostError);
GuestResolver::ServiceEntry* APS5_VABI getservbyname_nid_postfix(const char* name, const char* protocol);
void APS5_VABI freeaddrinfo_nid_postfix(GuestResolver::AddressInfo* first);
int APS5_VABI getaddrinfo_nid_postfix(const char* node, const char* service, const GuestResolver::AddressInfo* hints, GuestResolver::AddressInfo** output);
int APS5_VABI getnameinfo_nid_postfix(const void* address, std::uint32_t length, char* host, std::uint32_t hostLength, char* service, std::uint32_t serviceLength, int flags);
const char* APS5_VABI gai_strerror_nid_postfix(int error);

struct PosixForWebKitTms;
std::int32_t APS5_VABI times_nid_postfix(PosixForWebKitTms* output);

}
