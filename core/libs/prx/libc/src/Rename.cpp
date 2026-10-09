#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <filesystem>
#include <new>

namespace {

int RenameError(const std::filesystem::path& source, const std::filesystem::path& destination) {
    constexpr int GuestEnoent = 2;
    constexpr int GuestEio = 5;
    constexpr int GuestEacces = 13;
    constexpr int GuestEnotdir = 20;
    constexpr int GuestEisdir = 21;
    constexpr int GuestEinval = 22;
    constexpr int GuestEnotempty = 66;
    namespace fs = std::filesystem;
    const auto inspectionError = [](const fs::file_status& status, const std::error_code& error) {
        if (error == std::errc::not_a_directory) return GuestEnotdir;
        if (status.type() == fs::file_type::not_found) return GuestEnoent;
        if (error == std::errc::permission_denied) return GuestEacces;
        return GuestEio;
    };
    std::error_code error;
    const auto sourceStatus = fs::symlink_status(source, error);
    if (!fs::exists(sourceStatus)) return inspectionError(sourceStatus, error);
    const auto parent = destination.parent_path();
    if (!parent.empty()) {
        const auto parentStatus = fs::status(parent, error);
        if (!fs::exists(parentStatus)) return inspectionError(parentStatus, error);
        if (!fs::is_directory(parentStatus)) return GuestEnotdir;
    }
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && destinationStatus.type() != fs::file_type::not_found) return inspectionError(destinationStatus, error);
    if (!fs::exists(destinationStatus) || !fs::equivalent(source, destination, error)) {
        const bool sourceIsDirectory = fs::is_directory(sourceStatus);
        if (fs::exists(destinationStatus)) {
            const bool destinationIsDirectory = fs::is_directory(destinationStatus);
            if (sourceIsDirectory && !destinationIsDirectory) return GuestEnotdir;
            if (!sourceIsDirectory && destinationIsDirectory) return GuestEisdir;
            if (destinationIsDirectory && !fs::is_empty(destination, error)) return GuestEnotempty;
        }
        if (sourceIsDirectory) {
            const auto inside = fs::weakly_canonical(destination, error).lexically_relative(fs::weakly_canonical(source, error));
            if (!inside.empty() && *inside.begin() != "..") return GuestEinval;
        }
#ifdef _WIN32
        if (sourceIsDirectory && fs::exists(destinationStatus) && !fs::remove(destination, error)) return GuestEio;
#endif
    }
    fs::rename(source, destination, error);
    return error ? FilesystemError(error) : 0;
}

}

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    if (!from || !to) { errno = 14; return -1; }
    if (!*from || !*to) { errno = 2; return -1; }
    try {
        const int saved = errno;
        const auto source = ResolvePath_nid_no_patch(from);
        const auto destination = ResolvePath_nid_no_patch(to);
        const int error = RenameError(source, destination);
        if (error != 0) { errno = error; return -1; }
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        errno = saved;
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}
