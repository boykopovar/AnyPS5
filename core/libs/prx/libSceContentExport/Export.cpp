#include "prx/libSceContentExport/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

namespace {
constexpr std::int32_t MaxExecution = 10;
constexpr int ErrorInvalidState = static_cast<int>(0x809D3002u);
constexpr int ErrorBusy = static_cast<int>(0x809D3003u);
constexpr int ErrorNotInitialized = static_cast<int>(0x809D3004u);
constexpr int ErrorExecutionMax = static_cast<int>(0x809D3018u);
std::atomic<bool> g_initialized{false};
std::mutex g_sessionMutex;
std::set<std::int32_t> g_sessions;

void RequireSession(const char* function, std::int32_t expId) {
    if (!g_initialized.load()) throw std::logic_error(std::string(function) + ": not initialized");
    if (!g_sessions.contains(expId)) throw std::invalid_argument(std::string(function) + ": unknown export session");
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

int APS5_VABI sceContentExportFinish(std::int32_t expId) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    g_sessions.erase(expId);
    return 0;
}

int APS5_VABI sceContentExportFromData(std::int32_t expId, const ContentExportParam* param, std::size_t contentLength, ContentExportDataCallback callback, void* userData, char* path, std::size_t pathBufferLength) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFromFile(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufferLength) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength) {
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFromFileWithContentIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const std::int64_t* contentIds, std::uint32_t contentCount, char* path, std::size_t pathBufferLength) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFromFileWithTitleIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const char* titleIdList, char* path, std::size_t pathBufferLength) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportInit(const ContentExportInitParam* param) {
 NotImplemented_nid_no_patch(__func__);
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
