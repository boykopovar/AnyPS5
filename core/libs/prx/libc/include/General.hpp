#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP

#include <stdexcept>
#include <filesystem>
#include <vector>

#include "general/LogMacros.hpp"
#include "general/VabiMacros.hpp"
#include "general/ExportMacros.hpp"

extern "C" void NotImplemented_nid_no_patch(const char* funcName);
extern "C" void CxaFinalize_nid_no_patch(void* dsoHandle);
extern "C" bool StopAtPrefixLetter_nid_no_patch(const char* str, char** endptr, int base);
#ifndef _WIN32
extern "C" int KeepHostStandardError_nid_no_patch();
extern "C" int HostStandardError_nid_no_patch();
#endif

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path);
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
extern "C" void RecordWrittenPath_nid_no_patch(const std::filesystem::path& path);
extern "C" std::vector<std::filesystem::path> WrittenPaths_nid_no_patch();
extern "C" void SyncWrittenPaths_nid_no_patch();
extern "C" void BlockPathAlias_nid_no_patch(const char* guestPrefix);
#ifdef _WIN32
extern "C" int WindowsOpenError_nid_no_patch(unsigned long error);
extern "C" void* HostStandardHandle_nid_no_patch(int descriptor);
extern "C" void KeepStandardHandle_nid_no_patch(int descriptor, void* handle);
#endif

#define APS5_INVALID_ARG_EX throw std::invalid_argument(std::string(__func__) + ": invalid argument")

#define APS5_DUMMY_FUN \
int DummyFunction_nid_no_patch() { \
    NotImplemented_nid_no_patch(__func__); \
    return 0; \
}

#endif
