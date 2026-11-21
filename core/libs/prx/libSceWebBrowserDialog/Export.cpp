#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceWebBrowserDialog/include/InventoryExports.hpp"
#include "prx/libSceWebBrowserDialog/include/Api.hpp"
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

bool CopyIn(void* dst, const void* src, std::size_t size) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* dst, const void* src, std::size_t size) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = savedErrno;
    return copied;
}

bool ReservedClear(const std::uint8_t (&reserved)[256]) {
    for (const auto byte : reserved)
        if (byte != 0) return false;
    return true;
}

bool LengthWithin(const char* text, std::size_t limit) {
    if (!text) return false;
    std::size_t length = 0;
    while (length < limit) {
        char c = '\0';
        if (!CopyIn(&c, text + length, sizeof(c))) return false;
        if (c == '\0') break;
        ++length;
    }
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
    return COMMON_DIALOG_ERROR_RESULT_NONE;
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
    WebBrowserDialogResult local{};
    if (!CopyIn(&local, result, sizeof(local))) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (std::any_of(std::begin(local.reserved), std::end(local.reserved), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
    if (local.extended != nullptr) {
        WebBrowserDialogExtendedResult extended{};
        if (!CopyIn(&extended, local.extended, sizeof(extended))) return COMMON_DIALOG_ERROR_ARG_NULL;
        if (extended.size != sizeof(extended)) return COMMON_DIALOG_ERROR_PARAMETER;
        if (std::any_of(std::begin(extended.reserved0), std::end(extended.reserved0), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
        if (std::any_of(std::begin(extended.reserved1), std::end(extended.reserved1), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
    }
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const std::int32_t canceled = COMMON_DIALOG_RESULT_USER_CANCELED;
    if (!CopyOut(&result->result, &canceled, sizeof(canceled))) return COMMON_DIALOG_ERROR_ARG_NULL;
    return 0;
}

int APS5_VABI sceWebBrowserDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceWebBrowserDialogOpen(const void* param) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_INVALID_STATE;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    std::uint8_t probe = 0;
    if (!CopyIn(&probe, param, 1)) return COMMON_DIALOG_ERROR_ARG_NULL;
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
    WebBrowserDialogCookieParam local{};
    if (!CopyIn(&local, param, sizeof(local))) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (local.size != sizeof(WebBrowserDialogCookieParam) || !LengthWithin(local.address, 0x800) || !LengthWithin(local.cookie, 0x1000) ||
        !ReservedClear(local.reserved))
        return COMMON_DIALOG_ERROR_PARAMETER;
    return 0;
}

int APS5_VABI sceWebBrowserDialogOpenForPredeterminedContent(const void* param, const WebBrowserDialogAllowList* allowList) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (allowList == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    WebBrowserDialogAllowList localList{};
    if (!CopyIn(&localList, allowList, sizeof(localList))) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (localList.size != sizeof(localList)) return COMMON_DIALOG_ERROR_PARAMETER;
    bool hasAddress = false;
    for (const char* address : localList.addresses) {
        if (address == nullptr) continue;
        hasAddress = true;
        std::size_t length = 0;
        while (length < 256) {
            char c = '\0';
            if (!CopyIn(&c, address + length, sizeof(c))) return COMMON_DIALOG_ERROR_PARAMETER;
            if (c == '\0') break;
            ++length;
        }
        if (length == 0 || length == 256) return COMMON_DIALOG_ERROR_PARAMETER;
    }
    if (!hasAddress || std::any_of(std::begin(localList.reserved), std::end(localList.reserved), [](std::uint8_t value) { return value != 0; })) return COMMON_DIALOG_ERROR_PARAMETER;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    std::uint8_t probe = 0;
    if (!CopyIn(&probe, param, 1)) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_INVALID_STATE;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    g_open = true;
    PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::GenericFeature);
    return 0;
}

int APS5_VABI sceWebBrowserDialogResetCookie(const WebBrowserDialogResetCookieParam* param) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (g_open.load()) return COMMON_DIALOG_ERROR_INVALID_STATE;
    WebBrowserDialogResetCookieParam local{};
    if (!CopyIn(&local, param, sizeof(local))) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (local.size != sizeof(WebBrowserDialogResetCookieParam) || !ReservedClear(local.reserved)) return COMMON_DIALOG_ERROR_PARAMETER;
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
