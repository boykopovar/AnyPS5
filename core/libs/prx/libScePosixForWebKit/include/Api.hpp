#pragma once

#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include "prx/libSceLibcInternal/include/Statvfs.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI fnmatch_nid_postfix(const char* pattern, const char* string, int flags);
char* APS5_VABI strcasestr_nid_postfix(const char* text, const char* needle);
void APS5_VABI arc4random_buf_nid_postfix(void* buffer, std::size_t size);
std::uint32_t APS5_VABI arc4random_nid_postfix();
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
int APS5_VABI clock_gettime_np();
int APS5_VABI getenv_np();
int APS5_VABI scePosixForWebKit_gethostbyname_r();
int APS5_VABI pthread_setname_np_nid_postfix(Pthread thread, const char* name);
int APS5_VABI mmap_np();
int APS5_VABI msetname();
int APS5_VABI pthread_setschedparam_np();
int APS5_VABI setenv_np();
int APS5_VABI scePosixForWebKit_sysinfo();
int APS5_VABI mkstemp_nid_postfix(char* pattern);
int APS5_VABI isatty_nid_postfix(int descriptor);
GuestResolver::HostEntry* APS5_VABI gethostbyname_nid_postfix(const char* name);
GuestResolver::ServiceEntry* APS5_VABI getservbyname_nid_postfix(const char* name, const char* protocol);
void APS5_VABI freeaddrinfo_nid_postfix(GuestResolver::AddressInfo* first);
int APS5_VABI getaddrinfo_nid_postfix(const char* node, const char* service, const GuestResolver::AddressInfo* hints, GuestResolver::AddressInfo** output);
int APS5_VABI getnameinfo_nid_postfix(const void* address, std::uint32_t length, char* host, std::uint32_t hostLength, char* service, std::uint32_t serviceLength, int flags);
const char* APS5_VABI gai_strerror_nid_postfix(int error);

}
