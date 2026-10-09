#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <new>

#ifdef _WIN32
namespace {

int renameWindows(const std::filesystem::path& source, const std::filesystem::path& destination,
                  std::error_code& error) {
    namespace fs = std::filesystem;
    const auto inspectionError = [](const fs::file_status& status, const std::error_code& statusError) {
        if (statusError == std::errc::not_a_directory) return 20;
        if (status.type() == fs::file_type::not_found) return 2;
        if (statusError == std::errc::permission_denied) return 13;
        return 5;
    };

    const auto sourceStatus = fs::symlink_status(source, error);
    if (!fs::exists(sourceStatus)) return inspectionError(sourceStatus, error);

    const auto parent = destination.parent_path();
    if (!parent.empty()) {
        const auto parentStatus = fs::status(parent, error);
        if (!fs::exists(parentStatus)) return inspectionError(parentStatus, error);
        if (!fs::is_directory(parentStatus)) return 20;
    }

    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory)
        return inspectionError(destinationStatus, error);

    bool samePath = false;
    if (fs::exists(destinationStatus)) {
        samePath = fs::equivalent(source, destination, error);
        if (error) return FilesystemError(error);
    }
    if (!fs::exists(destinationStatus) || !samePath) {
        const bool sourceIsDirectory = fs::is_directory(sourceStatus);
        if (fs::exists(destinationStatus)) {
            const bool destinationIsDirectory = fs::is_directory(destinationStatus);
            if (sourceIsDirectory && !destinationIsDirectory) return 20;
            if (!sourceIsDirectory && destinationIsDirectory) return 21;
            if (destinationIsDirectory) {
                const bool destinationIsEmpty = fs::is_empty(destination, error);
                if (error) return FilesystemError(error);
                if (!destinationIsEmpty) return 66;
            }
        }
        if (sourceIsDirectory) {
            const auto sourcePath = fs::weakly_canonical(source, error);
            if (error) return 5;
            const auto destinationPath = fs::weakly_canonical(destination, error);
            if (error) return 5;
            const auto relativeDestination = destinationPath.lexically_relative(sourcePath);
            if (!relativeDestination.empty() && *relativeDestination.begin() != "..") return 22;
        }
        if (sourceIsDirectory && fs::exists(destinationStatus) && !fs::remove(destination, error))
            return error ? FilesystemError(error) : 5;
    }

    fs::rename(source, destination, error);
    return error ? FilesystemError(error) : 0;
}

}
#endif

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    if (!from || !to) { errno = 14; return -1; }
    if (!*from || !*to) { errno = 2; return -1; }
    try {
        const auto source = ResolvePath_nid_no_patch(from);
        const auto destination = ResolvePath_nid_no_patch(to);
        std::error_code error;
#ifdef _WIN32
        if (const int result = renameWindows(source, destination, error)) { errno = result; return -1; }
#else
        std::filesystem::rename(source, destination, error);
        if (error) { errno = FilesystemError(error); return -1; }
#endif
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}
