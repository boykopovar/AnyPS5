#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
std::atomic<bool> g_initialized{false};
std::mutex g_mutex;
std::set<std::int32_t> g_sessions;
std::int32_t g_nextSession = 1;
std::uint64_t g_nextExport = 1;

constexpr const char* ExportRoot = "/_ce";
constexpr int ErrorFileNotFound = static_cast<int>(0x809D3011u);
constexpr int ErrorNotSupportedFormat = static_cast<int>(0x809D3012u);
constexpr int ErrorLargeTitle = static_cast<int>(0x809D3013u);
constexpr int ErrorLargeComment = static_cast<int>(0x809D3015u);
constexpr int ErrorDiskFull = static_cast<int>(0x809D3017u);
constexpr int ErrorNotSupportedThumbnail = static_cast<int>(0x809D3019u);

template <std::size_t TSize>
bool IsTerminated(const char (&text)[TSize]) {
    return std::memchr(text, 0, TSize) != nullptr;
}

const char* ExtensionFor(std::string_view contentType) {
    if (contentType == "image/jpeg") return ".jpg";
    if (contentType == "image/png") return ".png";
    if (contentType == "image/gif") return ".gif";
    if (contentType == "video/mp4") return ".mp4";
    if (contentType == "video/webm") return ".webm";
    return nullptr;
}

bool IsThumbnailExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}

bool IsRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

int ExportFile(const char* funcName, std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufLen) {
    const std::string prefix = std::string(funcName) + ": ";
    if (param == nullptr || sourcePath == nullptr || path == nullptr || pathBufLen == 0) throw std::invalid_argument(prefix + "invalid argument");
    std::lock_guard lock(g_mutex);
    if (!g_initialized.load()) throw std::logic_error(prefix + "not initialized");
    if (!g_sessions.contains(exportId)) throw std::invalid_argument(prefix + "unknown export session");
    if (!IsTerminated(param->title)) return ErrorLargeTitle;
    if (!IsTerminated(param->comment)) return ErrorLargeComment;
    if (!IsTerminated(param->content_type)) return ErrorNotSupportedFormat;
    const char* extension = ExtensionFor(param->content_type);
    if (extension == nullptr) return ErrorNotSupportedFormat;
    const std::filesystem::path source = ResolvePath_nid_no_patch(sourcePath);
    if (!IsRegularFile(source)) return ErrorFileNotFound;
    std::filesystem::path thumbnail;
    if (thumbnailPath != nullptr) {
        thumbnail = ResolvePath_nid_no_patch(thumbnailPath);
        if (!IsRegularFile(thumbnail)) return ErrorFileNotFound;
        if (!IsThumbnailExtension(thumbnail)) return ErrorNotSupportedThumbnail;
    }
    const std::filesystem::path directory = ResolvePath_nid_no_patch(ExportRoot);
    std::string name;
    do {
        name = "export_" + std::to_string(g_nextExport++);
    } while (std::filesystem::exists(directory / (name + extension)));
    const std::string guestPath = std::string(ExportRoot) + "/" + name + extension;
    if (guestPath.size() + 1 > pathBufLen) throw std::invalid_argument(prefix + "path buffer is too small");
    const std::filesystem::path destination = directory / (name + extension);
    const std::filesystem::path thumbnailDestination = directory / (name + ".thumbnail" + thumbnail.extension().string());
    try {
        std::filesystem::create_directories(directory);
        std::filesystem::copy_file(source, destination);
        RecordWrittenPath_nid_no_patch(destination);
        if (thumbnailPath != nullptr) {
            std::filesystem::copy_file(thumbnail, thumbnailDestination);
            RecordWrittenPath_nid_no_patch(thumbnailDestination);
        }
    } catch (const std::filesystem::filesystem_error& error) {
        if (error.code() == std::errc::no_space_on_device) return ErrorDiskFull;
        throw;
    }
    std::memcpy(path, guestPath.c_str(), guestPath.size() + 1);
    return 0;
}
}

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    if (init_param->malloc_func == nullptr || init_param->free_func == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    if (init_param->reserved0 != 0 || init_param->reserved1 != 0) throw std::invalid_argument(std::string(__func__) + ": reserved fields are not zero");
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentExportFinish(std::int32_t exportId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    if (g_sessions.erase(exportId) == 0) throw std::invalid_argument(std::string(__func__) + ": unknown export session");
    return 0;
}

int APS5_VABI sceContentExportFromData(std::int32_t exportId, const ContentExportParam* param, std::size_t contentLength, void* dataProvider, void* userData, char* path, std::size_t pathBufLen) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFile(std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufLen) {
    return ExportFile(__func__, exportId, param, sourcePath, nullptr, path, pathBufLen);
}

int APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufLen) {
    if (thumbnailPath == nullptr) APS5_INVALID_ARG_EX;
    return ExportFile(__func__, exportId, param, sourcePath, thumbnailPath, path, pathBufLen);
}

int APS5_VABI sceContentExportStart(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    const std::int32_t exportId = g_nextSession++;
    g_sessions.insert(exportId);
    return exportId;
}

int APS5_VABI sceContentExportTerm(void) {
    std::lock_guard lock(g_mutex);
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    g_sessions.clear();
    return 0;
}

}
