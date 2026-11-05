#ifndef CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_LOADEXEC_HPP
#define CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_LOADEXEC_HPP

#include <filesystem>

extern "C" bool LoadConvertedExecutable_nid_no_patch(const std::filesystem::path& target, const char* const* arguments);

#endif
