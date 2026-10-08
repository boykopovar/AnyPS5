#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<int> g_status{0};

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);

constexpr std::size_t COOKIE_URL_SIZE = 2048;
constexpr std::size_t COOKIE_DATA_SIZE = 4096;

struct SetCookieParam {
    std::size_t size;
    const char* url;
    const char* cookie;
    char reserved[256];
};
static_assert(sizeof(SetCookieParam) == 280);

struct ResetCookieParam {
    std::size_t size;
    char reserved[256];
};
static_assert(sizeof(ResetCookieParam) == 264);

struct StoredCookie {
    std::string url;
    std::string cookie;
};

std::mutex g_cookieMutex;
std::vector<StoredCookie> g_cookies;

std::string ReadTerminated(const char* text, std::size_t size, const char* function) {
    const auto* end = static_cast<const char*>(std::memchr(text, '\0', size));
    if (end == nullptr) throw std::length_error(std::string(function) + ": string does not end within its buffer");
    return std::string(text, end);
}
}

extern "C" {

int APS5_VABI sceWebBrowserDialogInitialize(void) {
    int expected = 0;
    if (!g_status.compare_exchange_strong(expected, 1)) throw std::logic_error("sceWebBrowserDialogInitialize: already initialized");
    return 0;
}

int APS5_VABI sceWebBrowserDialogTerminate(void) {
    int expected = 1;
    if (!g_status.compare_exchange_strong(expected, 0)) throw std::logic_error("sceWebBrowserDialogTerminate: not initialized or still running");
    return 0;
}

int APS5_VABI sceWebBrowserDialogClose(void) {
 if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

int APS5_VABI sceWebBrowserDialogGetResult(void* result) {
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 *static_cast<std::int32_t*>(result) = COMMON_DIALOG_RESULT_USER_CANCELED;
 return 0;
}

int APS5_VABI sceWebBrowserDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceWebBrowserDialogOpen(const void* param) {
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceWebBrowserDialogUpdateStatus(void) {
    return g_status.load();
}


int APS5_VABI sceWebBrowserDialogSetCookie(const SetCookieParam* param) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (param->size != sizeof(SetCookieParam)) throw std::invalid_argument("sceWebBrowserDialogSetCookie: unknown param size");
    if (param->url == nullptr || param->cookie == nullptr) throw std::invalid_argument("sceWebBrowserDialogSetCookie: null url or cookie");
    StoredCookie stored{ReadTerminated(param->url, COOKIE_URL_SIZE, __func__), ReadTerminated(param->cookie, COOKIE_DATA_SIZE, __func__)};
    std::lock_guard lock(g_cookieMutex);
    g_cookies.push_back(std::move(stored));
    return 0;
}

int APS5_VABI sceWebBrowserDialogOpenForPredeterminedContent(const void* param, const void* contentParam) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    if (param == nullptr || contentParam == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceWebBrowserDialogResetCookie(const ResetCookieParam* param) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (param->size != sizeof(ResetCookieParam)) throw std::invalid_argument("sceWebBrowserDialogResetCookie: unknown param size");
    std::lock_guard lock(g_cookieMutex);
    g_cookies.clear();
    return 0;
}

}
