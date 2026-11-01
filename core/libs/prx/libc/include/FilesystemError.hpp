#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESYSTEMERROR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESYSTEMERROR_HPP

#include <system_error>
#include "General.hpp"

inline int FilesystemError(const std::error_code& error) {
    if (error == std::errc::no_such_file_or_directory) return 2;
    if (error == std::errc::permission_denied) return 13;
    if (error == std::errc::operation_not_permitted) return 1;
    if (error == std::errc::not_a_directory) return 20;
    if (error == std::errc::is_a_directory) return 21;
    if (error == std::errc::directory_not_empty) return 66;
    if (error == std::errc::device_or_resource_busy) return 16;
    if (error == std::errc::read_only_file_system) return 30;
    if (error == std::errc::filename_too_long) return 63;
    if (error == std::errc::too_many_symbolic_link_levels) return 62;
    if (error == std::errc::not_enough_memory) return 12;
    if (error == std::errc::invalid_argument) return 22;
    if (error == std::errc::cross_device_link) return 18;
    if (error == std::errc::file_exists) return 17;
    if (error == std::errc::no_space_on_device) return 28;
    return 5;
}

inline bool HasNonDirectoryPrefix_nid_no_patch(const std::filesystem::path& native) {
    std::error_code error;
    const auto trimmed = native.has_filename() ? native : native.parent_path();
    for (auto prefix = trimmed.parent_path(); prefix.has_relative_path(); prefix = prefix.parent_path()) {
        const auto status = std::filesystem::status(prefix, error);
        if (std::filesystem::exists(status)) return !std::filesystem::is_directory(status);
    }
    return false;
}

inline int NotFoundError_nid_no_patch(const std::filesystem::path& native) {
    return HasNonDirectoryPrefix_nid_no_patch(native) ? 20 : 2;
}

std::error_code RenameNative_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination, bool replacesExisting);

extern "C" bool ExecutePermission_nid_no_patch(const std::filesystem::path& native);

inline int ResolvePathError_nid_no_patch(const char* path, std::filesystem::path& resolved, bool module = false, bool archived = false) {
    try {
        resolved = archived ? ResolveArchivedPath_nid_no_patch(path, module) : module ? ResolvePathOrRelinkedModule_nid_no_patch(path) : ResolvePath_nid_no_patch(path);
        return 0;
    } catch (const std::filesystem::filesystem_error& error) {
        if (error.code() == std::errc::illegal_byte_sequence) return 2;
        return FilesystemError(error.code());
    }
}

#endif
