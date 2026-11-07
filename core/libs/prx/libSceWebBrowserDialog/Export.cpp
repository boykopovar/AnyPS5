#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceWebBrowserDialog/include/InventoryExports.hpp"
#include "prx/libSceWebBrowserDialog/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"

static_assert(sizeof(WebBrowserDialogCookieParam) == 280);
static_assert(offsetof(WebBrowserDialogCookieParam, address) == 8);
static_assert(offsetof(WebBrowserDialogCookieParam, cookie) == 16);
static_assert(sizeof(WebBrowserDialogResetCookieParam) == 264);

namespace {
std::atomic<int> g_status{0};
std::atomic<bool> g_open{false};

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_INVALID_STATE = static_cast<int>(0x80B80006u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_PARAMETER = static_cast<int>(0x80B8000Au);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);

bool ReservedClear(const std::uint8_t (&reserved)[256]) {
    for (const auto byte : reserved)
        if (byte != 0) return false;
    return true;
}

bool LengthWithin(const char* text, std::size_t limit) {
    if (!text) return false;
    const auto length = strnlen(text, limit);
    return length != 0 && length < limit;
}
}

extern "C" {

int APS5_VABI sceWebBrowserDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, 1)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceWebBrowserDialogTerminate(void) {
    int expected = 1;
    if (g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_NONE)) {
        g_open = false;
        return 0;
    }
    expected = COMMON_DIALOG_STATUS_FINISHED;
    if (g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_NONE)) {
        g_open = false;
        return 0;
    }
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return COMMON_DIALOG_ERROR_BUSY;
}

int APS5_VABI sceWebBrowserDialogClose(void) {
    g_open = false;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceWebBrowserDialogGetResult(WebBrowserDialogResult* result) {
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    WebBrowserDialogResult inResult{};
    const int savedErrno = errno;
    const bool readResult = GuestCopyIn_nid_no_patch(&inResult, result, sizeof(inResult));
    errno = savedErrno;
    if (!readResult) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (std::any_of(std::begin(inResult.reserved), std::end(inResult.reserved), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
    WebBrowserDialogExtendedResult inExtended{};
    if (inResult.extended != nullptr) {
        const bool readExtended = GuestCopyIn_nid_no_patch(&inExtended, inResult.extended, sizeof(inExtended));
        errno = savedErrno;
        if (!readExtended) return COMMON_DIALOG_ERROR_ARG_NULL;
        if (inExtended.size != sizeof(inExtended)) return COMMON_DIALOG_ERROR_PARAMETER;
        if (std::any_of(std::begin(inExtended.reserved0), std::end(inExtended.reserved0), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
        if (std::any_of(std::begin(inExtended.reserved1), std::end(inExtended.reserved1), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
    }
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    if (inResult.extended != nullptr) {
        const bool copiedExtended = GuestCopyOut_nid_no_patch(inResult.extended, &inExtended, sizeof(inExtended));
        errno = savedErrno;
        if (!copiedExtended) return COMMON_DIALOG_ERROR_ARG_NULL;
    }
    const std::int32_t cancelResult = COMMON_DIALOG_RESULT_USER_CANCELED;
    const bool copiedResult = GuestCopyOut_nid_no_patch(&result->result, &cancelResult, sizeof(cancelResult));
    errno = savedErrno;
    if (!copiedResult) return COMMON_DIALOG_ERROR_ARG_NULL;
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
 g_open = true;
 PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::GenericFeature);
 return 0;
}

int APS5_VABI sceWebBrowserDialogUpdateStatus(void) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_FINISHED) g_open = false;
    return status;
}

int APS5_VABI sceWebBrowserDialogSetCookie(const WebBrowserDialogCookieParam* param) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (g_open.load()) return COMMON_DIALOG_ERROR_INVALID_STATE;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (param->size != sizeof(WebBrowserDialogCookieParam) || !LengthWithin(param->address, 0x800) || !LengthWithin(param->cookie, 0x1000) ||
        !ReservedClear(param->reserved))
        return COMMON_DIALOG_ERROR_PARAMETER;
    return 0;
}

int APS5_VABI sceWebBrowserDialogOpenForPredeterminedContent(const void* param, const WebBrowserDialogAllowList* allowList) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (allowList == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (allowList->size != sizeof(*allowList)) return COMMON_DIALOG_ERROR_PARAMETER;
    bool hasAddress = false;
    for (const char* address : allowList->addresses) {
        if (address == nullptr) continue;
        hasAddress = true;
        std::size_t length = 0;
        while (length < 256 && address[length] != '\0') ++length;
        if (length == 0 || length == 256) return COMMON_DIALOG_ERROR_PARAMETER;
    }
    if (!hasAddress || std::any_of(std::begin(allowList->reserved), std::end(allowList->reserved), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    g_open = true;
    PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::GenericFeature);
    return 0;
}

int APS5_VABI sceWebBrowserDialogResetCookie(const WebBrowserDialogResetCookieParam* param) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (g_open.load()) return COMMON_DIALOG_ERROR_INVALID_STATE;
    if (param->size != sizeof(WebBrowserDialogResetCookieParam) || !ReservedClear(param->reserved)) return COMMON_DIALOG_ERROR_PARAMETER;
    return 0;
}

int APS5_VABI sceWebBrowserDialogGetEvent() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWebBrowserDialogNavigate() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWebBrowserDialogSetZoom() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceWebBrowserDialogUnknown_8r4EJ3FiX4w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8r4EJ3FiX4w", libSceWebBrowserDialogUnknown_8r4EJ3FiX4w_nid_no_patch);

}
