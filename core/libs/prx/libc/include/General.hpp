#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP

#include <stdexcept>
#include <string>
#include <filesystem>
#include <vector>
#include <cstdio>
#include <cstdint>

#include "general/LogMacros.hpp"
#include "general/VabiMacros.hpp"
#include "general/ExportMacros.hpp"

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* funcName);
extern "C" void CxaFinalize_nid_no_patch(void* dsoHandle);
extern "C" bool StopAtPrefixLetter_nid_no_patch(const char* str, char** endptr, int base);
extern "C" bool RejectIntegerBase_nid_no_patch(const char* str, char** endptr, int base, bool setError);
extern "C" int KeepHostStandardError_nid_no_patch();
extern "C" int KeepHostStandardOutput_nid_no_patch();
#ifndef _WIN32
extern "C" int HostStandardError_nid_no_patch();
#else
extern "C" bool AdoptHostStandardStream_nid_no_patch(int standard, int saved);
extern "C" std::FILE* HostStandardOutputFile_nid_no_patch();
extern "C" std::FILE* HostStandardErrorFile_nid_no_patch();
#endif

extern "C" bool CopyGuestEnvironment_nid_no_patch(const char* name, std::string& value);
extern "C" int CopyGuestString_nid_no_patch(const char* source, std::size_t maximum, std::string& destination);
extern "C" int CopyGuestPath_nid_no_patch(const char* source, std::string& destination);

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path);
extern "C" bool GuestCopyOut_nid_no_patch(void* destination, const void* source, std::size_t bytes);
extern "C" std::filesystem::path ResolvePathOrRelinkedModule_nid_no_patch(const char* path);
extern "C" std::filesystem::path ResolveArchivedPath_nid_no_patch(const char* path, bool module = false);
extern "C" std::filesystem::path ResolveDiskPath_nid_no_patch(const char* path);
extern "C" std::filesystem::path RelinkedModulePath_nid_no_patch(const std::filesystem::path& path);
extern "C" int SetWorkingDirectory_nid_no_patch(const char* resolved);
extern "C" bool FinalPathComponentIsDot_nid_no_patch(const char* path);
extern "C" int RenameGuestPath_nid_no_patch(const char* from, const char* to);
// Guest path aliases: a guest prefix such as "/savedata0" (a save-data mount point) resolves to a host
// directory instead of the run directory; the prefix matches whole path components only.
extern "C" void AddPathAlias_nid_no_patch(const char* guestPrefix, const char* hostPath);
extern "C" void RemovePathAlias_nid_no_patch(const char* guestPrefix);
using PathProvider = std::filesystem::path (*)(const std::filesystem::path& root);
extern "C" void AddPathProvider_nid_no_patch(const char* guestPath, PathProvider provide);
extern "C" void RecordWrittenPath_nid_no_patch(const std::filesystem::path& path);
extern "C" std::vector<std::filesystem::path> WrittenPaths_nid_no_patch();
extern "C" void SyncWrittenPaths_nid_no_patch();
extern "C" void BlockPathAlias_nid_no_patch(const char* guestPrefix);
extern "C" int DuplicateHostDescriptor_nid_no_patch(int descriptor);
#ifdef _WIN32
extern "C" int WindowsOpenError_nid_no_patch(unsigned long error);
extern "C" void* HostStandardHandle_nid_no_patch(int descriptor);
extern "C" void KeepStandardHandle_nid_no_patch(int descriptor, void* handle);
#endif

using PthreadCreateFn_nid_no_patch = int (APS5_VABI*)(void**, const void*, void* (APS5_VABI*)(void*), void*);
using PthreadJoinFn_nid_no_patch = int (APS5_VABI*)(void*, void**);
using PthreadDetachFn_nid_no_patch = int (APS5_VABI*)(void*);
extern "C" void RegisterPthreadCreate_nid_no_patch(PthreadCreateFn_nid_no_patch fn);
extern "C" void RegisterPthreadJoin_nid_no_patch(PthreadJoinFn_nid_no_patch fn);
extern "C" void RegisterPthreadDetach_nid_no_patch(PthreadDetachFn_nid_no_patch fn);
struct ThreadForwards_nid_no_patch {
    int (APS5_VABI* once)(std::int32_t*, void (APS5_VABI*)(void));
    int (APS5_VABI* keyCreate)(int*, void (APS5_VABI*)(void*));
    int (APS5_VABI* keyDelete)(int);
    void* (APS5_VABI* getSpecific)(int);
    int (APS5_VABI* setSpecific)(int, void*);
    int (APS5_VABI* equal)(void*, void*);
    void* (APS5_VABI* self)();
    void (APS5_VABI* exit)(void*);
};
extern "C" void RegisterThreadForwards_nid_no_patch(const ThreadForwards_nid_no_patch* forwards);
struct PthreadSyncForwards_nid_no_patch {
    int (APS5_VABI* mutexattrInit)(void**);
    int (APS5_VABI* mutexattrSettype)(void**, int);
    int (APS5_VABI* mutexattrDestroy)(void**);
    int (APS5_VABI* mutexInit)(void**, void* const*, const char*);
    int (APS5_VABI* mutexDestroy)(void**);
    int (APS5_VABI* mutexLock)(void**);
    int (APS5_VABI* mutexTrylock)(void**);
    int (APS5_VABI* mutexUnlock)(void**);
    int (APS5_VABI* condInit)(void**, void* const*, const char*);
    int (APS5_VABI* condDestroy)(void**);
    int (APS5_VABI* condWait)(void**, void**);
    int (APS5_VABI* condSignal)(void**);
    int (APS5_VABI* condBroadcast)(void**);
    int (APS5_VABI* detach)(void*);
    int (APS5_VABI* join)(void*, void**);
    void (APS5_VABI* yield)();
};
extern "C" void RegisterPthreadSyncForwards_nid_no_patch(const PthreadSyncForwards_nid_no_patch* forwards);

using SigactionFn_nid_no_patch = int (APS5_VABI*)(int, const void*, void*);
extern "C" void RegisterSigaction_nid_no_patch(SigactionFn_nid_no_patch fn, std::uint32_t* interruptSet);

#define APS5_INVALID_ARG_EX throw std::invalid_argument(std::string(__func__) + ": invalid argument")

#define APS5_DUMMY_FUN \
int DummyFunction_nid_no_patch() { \
    NotImplemented_nid_no_patch(__func__); \
    return 0; \
}

#endif
