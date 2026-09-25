#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP

#include <stdexcept>
#include <filesystem>

#include "general/LogMacros.hpp"
#include "general/VabiMacros.hpp"
#include "general/ExportMacros.hpp"

extern "C" void NotImplemented_nid_no_patch(const char* funcName);

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path);
// Guest path aliases: a guest prefix such as "/_sm/0" (a save-data mount point) resolves to a host
// directory instead of the run directory; the prefix matches whole path components only.
extern "C" void AddPathAlias_nid_no_patch(const char* guestPrefix, const char* hostPath);
extern "C" void RemovePathAlias_nid_no_patch(const char* guestPrefix);

#define APS5_INVALID_ARG_EX throw std::invalid_argument(std::string(__func__) + ": invalid argument")

#define APS5_DUMMY_FUN \
int DummyFunction_nid_no_patch() { \
    NotImplemented_nid_no_patch(__func__); \
    return 0; \
}

#endif
