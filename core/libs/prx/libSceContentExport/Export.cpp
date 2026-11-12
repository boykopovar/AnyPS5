#include "prx/libSceContentExport/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <atomic>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

namespace {
constexpr std::int32_t MaxExecution = 10;
constexpr std::uint32_t ContentTypeSdkVersion = 0x02500000;
constexpr std::uint32_t MemoryPoolSdkVersion = 0x04000000;
constexpr std::size_t MaximumPathLength = 0x400;
constexpr int ErrorGeneric = static_cast<int>(0x809D3001u);
constexpr int ErrorInvalidState = static_cast<int>(0x809D3002u);
constexpr int ErrorBusy = static_cast<int>(0x809D3003u);
constexpr int ErrorNotInitialized = static_cast<int>(0x809D3004u);
constexpr int ErrorNoMemory = static_cast<int>(0x809D3006u);
constexpr int ErrorInvalidContentType = static_cast<int>(0x809D3012u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x809D3016u);
constexpr int ErrorStorage = static_cast<int>(0x809D3017u);
constexpr int ErrorExecutionMax = static_cast<int>(0x809D3018u);
constexpr int ErrorDataCallback = static_cast<int>(0x809D301Bu);
std::atomic<bool> g_initialized{false};
std::mutex g_sessionMutex;
std::set<std::int32_t> g_sessions;

std::uint32_t CompiledSdkVersion() {
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, process + 16, sizeof(version));
    return version;
}

void RequireSession(const char* function, std::int32_t expId) {
    if (!g_initialized.load()) throw std::logic_error(std::string(function) + ": not initialized");
    if (!g_sessions.contains(expId)) throw std::invalid_argument(std::string(function) + ": unknown export session");
}

bool SupportedContentType(const char* contentType, std::int32_t sdkVersion) {
    if (sdkVersion < static_cast<std::int32_t>(ContentTypeSdkVersion)) return true;
    return std::strcmp(contentType, "video/mp4") == 0 || std::strcmp(contentType, "image/jpeg") == 0 ||
        std::strcmp(contentType, "image/png") == 0 || std::strcmp(contentType, "image/gif") == 0;
}
}

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    ContentExportInitParam2 localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, init_param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) APS5_INVALID_ARG_EX;
    if (localParam.malloc_func == nullptr || localParam.free_func == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    if (localParam.reserved0 != 0 || localParam.reserved1 != 0) throw std::invalid_argument(std::string(__func__) + ": reserved fields are not zero");
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentExportFinish(std::int32_t expId) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    g_sessions.erase(expId);
    return 0;
}

int APS5_VABI sceContentExportFromData(std::int32_t expId, const ContentExportParam* param, std::size_t contentLength, ContentExportDataCallback callback, void* userData, char* path, std::size_t pathBufferLength) {
    const std::int32_t sdkVersion = static_cast<std::int32_t>(CompiledSdkVersion());
    if (sdkVersion >= static_cast<std::int32_t>(MemoryPoolSdkVersion) && !g_initialized.load()) return ErrorNotInitialized;
    if (expId < 0 || expId >= MaxExecution || param == nullptr || callback == nullptr) return ErrorInvalidArgument;
    {
        std::lock_guard lock(g_sessionMutex);
        if (!g_sessions.contains(expId)) return ErrorInvalidArgument;
    }
    if (!SupportedContentType(param->content_type, sdkVersion)) return ErrorInvalidContentType;
    const char* subtype = std::strrchr(param->content_type, '/');
    if (subtype == nullptr || std::strlen(subtype + 1) < 2) return ErrorInvalidArgument;
    std::array<char, MaximumPathLength + 1> guestPath{};
    std::snprintf(guestPath.data(), guestPath.size(), "/av_contents/content_tmp/sce_cmgcnt%p.%s",
        reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&g_sessionMutex) + static_cast<std::uintptr_t>(expId)), subtype + 1);
    try {
        const auto hostPath = ResolveDiskPath_nid_no_patch(guestPath.data());
        std::error_code directoryError;
        std::filesystem::create_directories(hostPath.parent_path(), directoryError);
        if (directoryError) return ErrorStorage;
        std::ofstream file(hostPath, std::ios::binary | std::ios::trunc);
        if (!file) return ErrorGeneric;
        const void* data = nullptr;
        std::size_t size = 0;
        std::size_t written = 0;
        const bool modernCallback = sdkVersion >= static_cast<std::int32_t>(MemoryPoolSdkVersion);
        for (;;) {
            const std::int32_t result = callback(&data, &size, userData);
            if (modernCallback && result != 0 && result != 1) return ErrorDataCallback;
            if (size != 0) {
                if (data == nullptr) return ErrorStorage;
                file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
                if (!file) return ErrorStorage;
                written += size;
            }
            if (result == 1 || (!modernCallback && result < 0)) break;
        }
        file.close();
        if (!file) return ErrorStorage;
        if (written != contentLength) return ErrorInvalidArgument;
    } catch (const std::bad_alloc&) {
        return ErrorNoMemory;
    } catch (const std::filesystem::filesystem_error&) {
        return ErrorGeneric;
    }
    if (path != nullptr) {
        const std::size_t length = std::min(pathBufferLength, MaximumPathLength);
        std::memcpy(path, guestPath.data(), length);
    }
    return 0;
}

int APS5_VABI sceContentExportFromFile(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportAddToFavorites(std::int64_t contentId) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportDataProvideFunction(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFree(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFrom(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFromFileForSf(std::int32_t expId, const ContentExportParam* param, const ContentExportSfParam* sfParam, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFileWithContentIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const std::int64_t* contentIds, std::uint32_t contentCount, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFileWithTitleIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const char* titleIdList, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportInit(const ContentExportInitParam* param) {
    if (param == nullptr) APS5_INVALID_ARG_EX;
    if (param->mallocFunc == nullptr || param->freeFunc == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentExportMalloc(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportRemoveFromFavorites(std::int64_t contentId) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportRenameContent(std::int64_t contentId, const char* name) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportValidateContents(const std::int64_t* contentIds, std::uint32_t count) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportStart(void) {
    std::lock_guard lock(g_sessionMutex);
    if (!g_initialized.load()) return ErrorNotInitialized;
    for (std::int32_t expId = 0; expId < MaxExecution; ++expId) {
        if (g_sessions.insert(expId).second) return expId;
    }
    return ErrorExecutionMax;
}

int APS5_VABI sceContentExportGetProgress(std::int32_t expId) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportCancel(std::int32_t expId) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    return ErrorInvalidState;
}

int APS5_VABI sceContentExportFromDataWithThumbnail(std::int32_t expId, const ContentExportParam* param, const ContentExportCallbackParam* contentCallbackParam, const char* thumbnailType, const ContentExportCallbackParam* thumbnailCallbackParam, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportTerm(void) {
    std::lock_guard lock(g_sessionMutex);
    if (!g_sessions.empty()) return ErrorBusy;
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) return ErrorNotInitialized;
    return 0;
}

}
