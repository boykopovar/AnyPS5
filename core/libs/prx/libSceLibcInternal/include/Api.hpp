#pragma once

#include "prx/libSceLibcInternal/include/Statvfs.hpp"
#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"

using ErrExit = void (APS5_VABI*)(int);

struct GuestOption {
    const char* name;
    int hasArgument;
    int* flag;
    int value;
};

struct LibcUtimbuf {
    std::int64_t actime;
    std::int64_t modtime;
};

using ThreadDestructorFunction = void (APS5_VABI*)(void*);

extern "C" {

void* APS5_VABI fdopendir_nid_postfix(int fd);
void APS5_VABI err_set_file_nid_postfix(FileStream* stream);
void APS5_VABI err_set_exit_nid_postfix(ErrExit function);
void APS5_VABI vwarnc_nid_postfix(int code, const char* format, VaList* args);
void APS5_VABI vwarn_nid_postfix(const char* format, VaList* args);
void APS5_VABI vwarnx_nid_postfix(const char* format, VaList* args);
void APS5_VABI warnc_nid_postfix(int code, const char* format, ...);
void APS5_VABI warn_nid_postfix(const char* format, ...);
void APS5_VABI _warn_nid_postfix(const char* format, ...);
void APS5_VABI warnx_nid_postfix(const char* format, ...);
const char* APS5_VABI getprogname_nid_postfix();
const char* APS5_VABI _getprogname_nid_postfix();
void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle);
void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info);
int APS5_VABI _sceLibcInternalThreadAtexit_nid_postfix(ThreadDestructorFunction destructor, void* object, void* dsoSymbol);
void APS5_VABI _sceLibcInternalThreadDtors_nid_postfix();
int APS5_VABI _sceLibcInternalForceTlsDestructor_nid_postfix(KernelModule handle);
unsigned int APS5_VABI alarm_nid_postfix(unsigned int seconds);
int APS5_VABI fstatvfs_nid_postfix(int descriptor, LibcStatvfs* output);
int APS5_VABI utime_nid_postfix(const char* path, const LibcUtimbuf* times);
int APS5_VABI getopt_nid_postfix(int count, char* const* arguments, const char* options);
int APS5_VABI getopt_long_nid_postfix(int count, char* const* arguments, const char* options, const GuestOption* longOptions, int* index);
int APS5_VABI getopt_long_only_nid_postfix(int count, char* const* arguments, const char* options, const GuestOption* longOptions, int* index);
int APS5_VABI inet_aton_nid_postfix(const char* text, std::uint32_t* address);
std::uint32_t APS5_VABI inet_addr_nid_postfix(const char* text);
char* APS5_VABI inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size);
char* APS5_VABI __inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size);
char* APS5_VABI inet_ntoa_nid_postfix(std::uint32_t address);
int APS5_VABI statvfs_nid_postfix(const char* path, LibcStatvfs* output);
void APS5_VABI psignal_nid_postfix(unsigned int signal, const char* prefix);
int APS5_VABI siginterrupt_nid_postfix(int guest, int flag);
int APS5_VABI posix_spawnattr_init_nid_postfix(PosixSpawnAttributes** attributes);
int APS5_VABI posix_spawnattr_destroy_nid_postfix(PosixSpawnAttributes** attributes);
int APS5_VABI posix_spawnattr_setflags_nid_postfix(PosixSpawnAttributes** attributes, std::int16_t flags);
int APS5_VABI posix_spawnattr_setsigdefault_nid_postfix(PosixSpawnAttributes** attributes, const GuestSignalSet* signalDefault);
int APS5_VABI posix_spawnattr_getflags_nid_postfix(PosixSpawnAttributes* const* attributes, std::int16_t* flags);
int APS5_VABI posix_spawnattr_getpgroup_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* processGroup);
int APS5_VABI posix_spawnattr_getschedparam_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* schedulingParameter);
int APS5_VABI posix_spawnattr_getschedpolicy_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* schedulingPolicy);
int APS5_VABI posix_spawnattr_getsigdefault_nid_postfix(PosixSpawnAttributes* const* attributes, GuestSignalSet* signalDefault);
int APS5_VABI posix_spawnattr_getsigmask_nid_postfix(PosixSpawnAttributes* const* attributes, GuestSignalSet* signalMask);
int APS5_VABI posix_spawnattr_setpgroup_nid_postfix(PosixSpawnAttributes** attributes, std::int32_t processGroup);
int APS5_VABI posix_spawnattr_setschedparam_nid_postfix(PosixSpawnAttributes** attributes, const std::int32_t* schedulingParameter);
int APS5_VABI posix_spawnattr_setschedpolicy_nid_postfix(PosixSpawnAttributes** attributes, std::int32_t schedulingPolicy);
int APS5_VABI posix_spawnattr_setsigmask_nid_postfix(PosixSpawnAttributes** attributes, const GuestSignalSet* signalMask);
int APS5_VABI posix_spawn_file_actions_init_nid_postfix(PosixSpawnFileActions** actions);
int APS5_VABI posix_spawn_file_actions_destroy_nid_postfix(PosixSpawnFileActions** actions);
int APS5_VABI posix_spawn_file_actions_addopen_nid_postfix(PosixSpawnFileActions** actions, int descriptor, const char* path, int openFlags, std::uint16_t mode);
int APS5_VABI posix_spawn_file_actions_adddup2_nid_postfix(PosixSpawnFileActions** actions, int descriptor, int newDescriptor);
int APS5_VABI posix_spawn_file_actions_addclose_nid_postfix(PosixSpawnFileActions** actions, int descriptor);
int APS5_VABI posix_spawn_nid_postfix(std::int32_t* processId, const char* path, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment);
int APS5_VABI posix_spawnp_nid_postfix(std::int32_t* processId, const char* file, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment);
void APS5_VABI makecontext_nid_postfix(UserUcontext* context, void (APS5_VABI* start)(), int count, ...);

[[noreturn]] void APS5_VABI verrc_nid_postfix(int status, int code, const char* format, VaList* args);
[[noreturn]] void APS5_VABI verr_nid_postfix(int status, const char* format, VaList* args);
[[noreturn]] void APS5_VABI verrx_nid_postfix(int status, const char* format, VaList* args);
[[noreturn]] void APS5_VABI errc_nid_postfix(int status, int code, const char* format, ...);
[[noreturn]] void APS5_VABI err_nid_postfix(int status, const char* format, ...);
[[noreturn]] void APS5_VABI _err_nid_postfix(int status, const char* format, ...);
[[noreturn]] void APS5_VABI errx_nid_postfix(int status, const char* format, ...);

int APS5_VABI _ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(const char* path, int* perms, int* error);
int APS5_VABI _ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix(const char* path, int* perms, int* error);
std::uint64_t APS5_VABI _ZNSt10filesystem10_File_sizeEPKcPi_nid_postfix(const char* path, int* error);
int APS5_VABI _ZNSt10filesystem16_Last_write_timeEPKcPlS2__nid_postfix(const char* path, std::int64_t* seconds, std::int64_t* nanoseconds);
int APS5_VABI _ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(const char* path, const char* existing);
bool APS5_VABI _ZNSt10filesystem11_Remove_dirEPKc_nid_postfix(const char* path);
int APS5_VABI _ZNSt10filesystem7_UnlinkEPKc_nid_postfix(const char* path);
int APS5_VABI _ZNSt10filesystem6_ChmodEPKcNS_5permsE_nid_postfix(const char* path, int perms);
bool APS5_VABI _ZNSt10filesystem12_Current_getERA260_c_nid_postfix(char* target);
char* APS5_VABI _ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(char* target, void* handle, int* type);
void* APS5_VABI _ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(char* target, const char* path, int* error, int* type);
int APS5_VABI _ZNSt10filesystem10_Close_dirEPv_nid_postfix(void* handle);
void APS5_VABI _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix();
void APS5_VABI _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix();
int APS5_VABI __std_init_once_begin_initialize_clr_nid_postfix(void** once, unsigned long flags, int* pending, void** context);
int APS5_VABI __std_init_once_complete_clr_nid_postfix(void** once, unsigned long flags, void* context);
[[noreturn]] void APS5_VABI __std_init_once_link_alternate_names_and_abort_nid_postfix();

}
