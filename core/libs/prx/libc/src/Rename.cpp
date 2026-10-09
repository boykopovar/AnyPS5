#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <filesystem>
#include <new>

namespace {
int RenameError_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination) {
    constexpr int GuestEacces = 13;
    constexpr int GuestEisdir = 21;
    namespace fs = std::filesystem;
    const auto missing = [](const fs::file_status& status, const std::error_code& error) {
        if (error == std::errc::not_a_directory) return 20;
        if (status.type() == fs::file_type::not_found) return 2;
        if (error == std::errc::permission_denied) return GuestEacces;
        return 5;
    };
    std::error_code error;
    const auto sourceStatus = fs::symlink_status(source, error);
    if (!fs::exists(sourceStatus)) return missing(sourceStatus, error);
    const auto parent = destination.parent_path();
    if (!parent.empty()) {
        const auto parentStatus = fs::status(parent, error);
        if (!fs::exists(parentStatus)) return missing(parentStatus, error);
        if (!fs::is_directory(parentStatus)) return 20;
    }
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && destinationStatus.type() != fs::file_type::not_found) return missing(destinationStatus, error);
    if (!fs::exists(destinationStatus) || !fs::equivalent(source, destination, error)) {
        const bool sourceIsDirectory = fs::is_directory(sourceStatus);
        if (fs::exists(destinationStatus)) {
            const bool destinationIsDirectory = fs::is_directory(destinationStatus);
            if (sourceIsDirectory && !destinationIsDirectory) return 20;
            if (!sourceIsDirectory && destinationIsDirectory) return GuestEisdir;
            if (destinationIsDirectory && !fs::is_empty(destination, error)) return 66;
        }
        if (sourceIsDirectory) {
            const auto inside = fs::weakly_canonical(destination, error).lexically_relative(fs::weakly_canonical(source, error));
            if (!inside.empty() && *inside.begin() != "..") return 22;
        }
#ifdef _WIN32
        if (sourceIsDirectory && fs::exists(destinationStatus) && !fs::remove(destination, error)) return 5;
#endif
    }
    fs::rename(source, destination, error);
    return error ? 5 : 0;
}
}

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    if (!from || !to) { errno = 14; return -1; }
    if (!*from || !*to) { errno = 2; return -1; }
    try {
        const auto source = ResolvePath_nid_no_patch(from);
        const auto destination = ResolvePath_nid_no_patch(to);
        if (const int error = RenameError_nid_no_patch(source, destination)) { errno = error; return -1; }
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}