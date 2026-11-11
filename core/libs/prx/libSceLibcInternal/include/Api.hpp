#pragma once

#include "prx/libSceLibcInternal/include/Statvfs.hpp"
#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/LegacySignals.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <system_error>

using ErrExit = void (APS5_VABI*)(int);

struct GuestOption {
    const char* name;
    int hasArgument;
    int* flag;
    int value;
};

struct GuestDirectoryEntry;

struct LibcUtimbuf {
    std::int64_t actime;
    std::int64_t modtime;
};

using ThreadDestructorFunction = void (APS5_VABI*)(void*);

extern "C" {

int APS5_VABI sigblock_nid_postfix(int mask);
int APS5_VABI sigsetmask_nid_postfix(int mask);

void* APS5_VABI fdopendir_nid_postfix(int fd);
void* APS5_VABI __opendir2_nid_postfix(const char* name, int flags);
GuestDirectoryEntry* APS5_VABI _readdir_unlocked_nid_postfix(void* handle, int skip);
void APS5_VABI _seekdir_nid_postfix(void* handle, std::int64_t location);
int APS5_VABI __fflush_nid_postfix(FileStream* stream);
int APS5_VABI __vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args);
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
void APS5_VABI setprogname_nid_postfix(const char* name);
void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle);
void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info);
void* APS5_VABI sceLibcInternalPmrNewDeleteResource_nid_no_patch() noexcept;
void* APS5_VABI sceLibcInternalPmrGetDefaultResource_nid_no_patch() noexcept;
void* APS5_VABI sceLibcInternalPmrNullMemoryResource_nid_no_patch() noexcept;
void* APS5_VABI sceLibcInternalPmrSetDefaultResource_nid_no_patch(void* resource) noexcept;
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
std::uint32_t APS5_VABI inet_netof_nid_postfix(std::uint32_t address);
std::uint32_t APS5_VABI __inet_netof_nid_postfix(std::uint32_t address);
std::uint32_t APS5_VABI inet_lnaof_nid_postfix(std::uint32_t address);
std::uint32_t APS5_VABI __inet_lnaof_nid_postfix(std::uint32_t address);
std::uint32_t APS5_VABI inet_makeaddr_nid_postfix(std::uint32_t net, std::uint32_t host);
std::uint32_t APS5_VABI __inet_makeaddr_nid_postfix(std::uint32_t net, std::uint32_t host);
std::uint32_t APS5_VABI inet_network_nid_postfix(const char* text);
std::uint32_t APS5_VABI __inet_network_nid_postfix(const char* text);
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
int APS5_VABI __kernel_rem_pio2_nid_postfix(double* x, double* y, int e0, int nx, int prec);

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
int APS5_VABI _ZNSt10filesystem5_StatEPKcPNS_5permsE_nid_postfix(const char* path, int* perms);
int APS5_VABI _ZNSt10filesystem6_LstatEPKcPNS_5permsE_nid_postfix(const char* path, int* perms);
std::uint64_t APS5_VABI _ZNSt10filesystem10_File_sizeEPKc_nid_postfix(const char* path);
std::int64_t APS5_VABI _ZNSt10filesystem16_Last_write_timeEPKc_nid_postfix(const char* path);
bool APS5_VABI _ZNSt10filesystem20_Set_last_write_timeEPKcl_nid_postfix(const char* path, std::int64_t microseconds);
int APS5_VABI _ZNSt10filesystem7_RenameEPKcS1__nid_postfix(const char* from, const char* to);
int APS5_VABI _ZNSt10filesystem7_ResizeEPKcm_nid_postfix(const char* path, std::uint64_t size);
int APS5_VABI _ZNSt10filesystem10_Copy_fileEPKcS1__nid_postfix(const char* from, const char* to);
int APS5_VABI _ZNSt10filesystem8_StatvfsEPKcRNS_10space_infoE_nid_postfix(const char* path, std::uint64_t* space);
int APS5_VABI _ZNSt10filesystem13_Current_set2EPKc_nid_postfix(const char* path);
int APS5_VABI _ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(const char* path, const char* existing);
bool APS5_VABI _ZNSt10filesystem11_Remove_dirEPKc_nid_postfix(const char* path);
void APS5_VABI sceLibcInternalPadCompleteDestructor_nid_no_patch(void* self);
int APS5_VABI sceLibcInternalCollateDoCompare_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2);
GuestLocale::String* APS5_VABI sceLibcInternalCollateDoTransform_nid_no_patch(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char* first, const char* last);
std::int64_t APS5_VABI sceLibcInternalCollateDoHash_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first, const char* last);
int APS5_VABI sceLibcInternalCollateCompare_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2);
std::int64_t APS5_VABI sceLibcInternalCollateHash_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first, const char* last);
int APS5_VABI _ZNSt10filesystem7_UnlinkEPKc_nid_postfix(const char* path);
int APS5_VABI _ZNSt10filesystem6_ChmodEPKcNS_5permsE_nid_postfix(const char* path, int perms);
bool APS5_VABI _ZNSt10filesystem12_Current_getERA260_c_nid_postfix(char* target);
char* APS5_VABI _ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(char* target, void* handle, int* type);
void* APS5_VABI _ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(char* target, const char* path, int* error, int* type);
int APS5_VABI _ZNSt10filesystem10_Close_dirEPv_nid_postfix(void* handle);
void APS5_VABI sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(void* self);
int APS5_VABI __std_init_once_begin_initialize_clr_nid_postfix(void** once, unsigned long flags, int* pending, void** context);
int APS5_VABI __std_init_once_complete_clr_nid_postfix(void** once, unsigned long flags, void* context);
[[noreturn]] void APS5_VABI __std_init_once_link_alternate_names_and_abort_nid_postfix();
int APS5_VABI _Mtxinit_nid_postfix(PthreadMutex* mutex, const char* name);
int APS5_VABI _ZNSt8__sce_v236_Mtx_init_with_default_name_overrideEPP13pthread_mutexiPKc_nid_postfix(void** mutex, int type, const char* name);
int APS5_VABI _Mtxlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI _Mtxunlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI _Mtxdst_nid_postfix(PthreadMutex* mutex);
void APS5_VABI sceLibcInternalPlacementDelete_nid_no_patch(void* pointer, void* placement) noexcept;
void APS5_VABI sceLibcInternalPlacementArrayDelete_nid_no_patch(void* pointer, void* placement) noexcept;
void* APS5_VABI sceLibcInternalAlignedArrayNewNothrow_nid_no_patch(std::size_t size, std::size_t alignment, const void* tag) noexcept;
void APS5_VABI sceLibcInternalAlignedArrayDelete_nid_no_patch(void* pointer, std::size_t alignment);
void APS5_VABI sceLibcInternalSizedNothrowDelete_nid_no_patch(void* pointer, std::size_t size, const void* tag) noexcept;
void APS5_VABI sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(void* pointer, std::size_t size, const void* tag) noexcept;
const std::error_category* APS5_VABI sceLibcInternalIostreamCategory_nid_no_patch();
void APS5_VABI sceLibcInternalFutureErrorBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalOverflowErrorBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnderflowErrorBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalFutureErrorCategoryDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalSystemErrorCategoryDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalSystemErrorCategoryDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalGenericErrorCategoryDestructor_nid_no_patch(void* self);
[[noreturn]] void APS5_VABI sceLibcInternalXoverflowError_nid_no_patch(const char* message);
[[noreturn]] void APS5_VABI _ZNKSs5_XranEv_nid_postfix(const GuestLocale::String* self);
[[noreturn]] void APS5_VABI _ZNKSs5_XlenEv_nid_postfix(const GuestLocale::String* self);
GuestLocale::String* APS5_VABI _ZNSs5eraseEmm_nid_postfix(GuestLocale::String* self, std::size_t position, std::size_t count);
GuestLocale::String* APS5_VABI _ZNSs6appendERKSsmm_nid_postfix(GuestLocale::String* self, const GuestLocale::String* source, std::size_t position, std::size_t count);
GuestLocale::String* APS5_VABI _ZNSs6appendEmc_nid_postfix(GuestLocale::String* self, std::size_t count, char character);
GuestLocale::String* APS5_VABI _ZNSs6assignEPKcm_nid_postfix(GuestLocale::String* self, const char* source, std::size_t count);
GuestLocale::String* APS5_VABI _ZNSs6assignERKSsmm_nid_postfix(GuestLocale::String* self, const GuestLocale::String* source, std::size_t position, std::size_t count);
GuestLocale::String* APS5_VABI _ZNSs6assignEmc_nid_postfix(GuestLocale::String* self, std::size_t count, char character);
void APS5_VABI sceLibcInternalFundamentalTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalFundamentalTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalFundamentalTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalArrayTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalArrayTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalArrayTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalFunctionTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalFunctionTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalFunctionTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalEnumTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalEnumTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalEnumTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalClassTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalClassTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalClassTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalSiClassTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalSiClassTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalSiClassTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalVmiClassTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalVmiClassTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalVmiClassTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPbaseTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPbaseTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPbaseTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPointerTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPointerTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPointerTypeInfoDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPointerToMemberTypeInfoBaseDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPointerToMemberTypeInfoCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalPointerToMemberTypeInfoDeletingDestructor_nid_no_patch(void* self);
const char* APS5_VABI sceLibcInternalSystemErrorCategoryName_nid_no_patch(const void* self) noexcept;
const char* APS5_VABI sceLibcInternalGenericErrorCategoryName_nid_no_patch(const void* self) noexcept;
const char* APS5_VABI sceLibcInternalIostreamErrorCategoryName_nid_no_patch(const void* self) noexcept;
const char* APS5_VABI sceLibcInternalFutureErrorCategoryName_nid_no_patch(const void* self) noexcept;
int APS5_VABI sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(const void* self) noexcept;
int APS5_VABI sceLibcInternalCodecvtBaseEncoding_nid_no_patch(const void* self) noexcept;
char APS5_VABI sceLibcInternalNumpunctDoDecimalPoint_nid_no_patch(const GuestLocale::NumpunctFacet* self);
char APS5_VABI sceLibcInternalNumpunctDoThousandsSep_nid_no_patch(const GuestLocale::NumpunctFacet* self);
GuestLocale::String* APS5_VABI sceLibcInternalNumpunctDoGrouping_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self);
GuestLocale::String* APS5_VABI sceLibcInternalNumpunctDoFalsename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self);
GuestLocale::String* APS5_VABI sceLibcInternalNumpunctDoTruename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self);
char APS5_VABI sceLibcInternalNumpunctDecimalPoint_nid_no_patch(const GuestLocale::NumpunctFacet* self);
char APS5_VABI sceLibcInternalNumpunctThousandsSep_nid_no_patch(const GuestLocale::NumpunctFacet* self);
GuestLocale::String* APS5_VABI sceLibcInternalNumpunctGrouping_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self);
GuestLocale::String* APS5_VABI sceLibcInternalNumpunctFalsename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self);
GuestLocale::String* APS5_VABI sceLibcInternalNumpunctTruename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self);

int APS5_VABI sceLibcMspaceSetMallocCallback();
int APS5_VABI sceLibcPafMspaceReportMemoryBlocks();
int APS5_VABI sceLibcHeapSetAddressRangeCallback();
int APS5_VABI sceLibcPafMspaceTrim();
int APS5_VABI sceLibcHeapMutexCalloc();
int APS5_VABI sceLibcInternalSetMallocCallback();
int APS5_VABI sceLibcHeapSetTraceMarker();
int APS5_VABI sceLibcPafMspaceCheckMemoryBounds();
int APS5_VABI sceLibcHeapUnsetTraceMarker();
int APS5_VABI sceLibcHeapGetAddressRanges();
int APS5_VABI sceLibcHeapMutexFree();
[[noreturn]] void APS5_VABI __longjmp_nid_postfix(std::uint64_t* buffer, int value);
void APS5_VABI sceLibcInternalUnknown6_005_PJWpmO4pj6I_nid_no_patch(void);
void APS5_VABI sceLibcInternalUnknown6_065_1kIpmZX1jw8_nid_no_patch(void);
void APS5_VABI sceLibcInternalUnknown6_082_Sopthb9ztZI_nid_no_patch(void);
void APS5_VABI sceLibcInternalUnknown6_093_R2QKo3hBLkw_nid_no_patch(int* locks);
int APS5_VABI sceLibcInternalUnknown6_098_5FoEpV3Aj0c_nid_no_patch(const void* address, void* info);
void APS5_VABI sceLibcInternalUnknown6_004_V2FICbvPaps_nid_no_patch(void* self, const void* locinfo);
void APS5_VABI sceLibcInternalUnknown6_007_hnGhTkIDFHg_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_026_U4JPmRpm70c_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_029_ZzmRfDtowlo_nid_no_patch(void* self, const void* locinfo);
void APS5_VABI sceLibcInternalUnknown6_034_vu0BpBGlol4_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_041_to7GggwECZU_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_061_8PJ9qmklj2c_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_071_CiD6mBPDZrA_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_083_hpc9OSfCAEg_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_090_Oeo9tUbzW7s_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_140_iZCHNahjpp4_nid_no_patch(void* self, const void* locinfo);
void APS5_VABI sceLibcInternalUnknown6_141_zkCx9c2QKBc_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalUnknown6_150_cXLpLN0lSwc_nid_no_patch(void* self, const void* locinfo);
bool APS5_VABI sceLibcInternalPending3_016_nid_no_patch(const void* self);
int APS5_VABI sceLibcInternalPending3_021_nid_no_patch(const void* self);
bool APS5_VABI sceLibcInternalPending3_023_nid_no_patch(const void* self);
int APS5_VABI sceLibcInternalPending3_037_nid_no_patch(const void* self);
int APS5_VABI sceLibcInternalPending3_038_nid_no_patch(const void* self);
bool APS5_VABI sceLibcInternalPending3_039_nid_no_patch(const void* self);
int APS5_VABI sceLibcInternalPending4_007_nid_no_patch(const void* self, const void* name, const void* locale);
void APS5_VABI sceLibcInternalPending4_008_nid_no_patch(const void* self, int catalog);
int APS5_VABI sceLibcInternalPending4_013_nid_no_patch(const void* self, const void* name, const void* locale);
void APS5_VABI sceLibcInternalPending4_014_nid_no_patch(const void* self, int catalog);

using LibcUnexpectedHandler = void (*)();

LibcUnexpectedHandler APS5_VABI _ZSt14set_unexpectedPFvvE_nid_postfix(LibcUnexpectedHandler handler);
LibcUnexpectedHandler APS5_VABI _ZSt14get_unexpectedv_nid_postfix();
[[noreturn]] void APS5_VABI _ZSt10unexpectedv_nid_postfix();
void APS5_VABI _ZNSt16nested_exceptionD0Ev_nid_postfix(void* self);
void APS5_VABI _ZNSt16nested_exceptionD1Ev_nid_postfix(void* self);
void APS5_VABI _ZNSt16nested_exceptionD2Ev_nid_postfix(void* self);


}
