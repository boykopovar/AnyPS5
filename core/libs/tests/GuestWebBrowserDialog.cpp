#include "prx/libSceWebBrowserDialog/include/Api.hpp"
#include "prx/libSceWebBrowserDialog/include/InventoryExports.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) { if (!value) std::abort(); }

bool GenericNoticeShown() {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == PsnDialogNoticeRequest(PsnDialogNoticeKind::GenericFeature).message;
}

void RequireNotices(const void* param) {
    WebBrowserDialogAllowList allowList{};
    allowList.size = sizeof(allowList);
    allowList.addresses[0] = "test.invalid";
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceWebBrowserDialogInitialize() == 0);

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceWebBrowserDialogOpen(param) == 0);
    Require(!GenericNoticeShown() && !ModalDialogCapturesInput_nid_postfix());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(!GenericNoticeShown());
    Require(sceWebBrowserDialogOpen(param) == 0);
    Require(GenericNoticeShown());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == 0);
    Require(GenericNoticeShown());

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceWebBrowserDialogTerminate() == 0);
}

template <typename T>
void SetBase(T& param) {
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
}

bool EventZeroed(const WebBrowserDialogEvent& event) {
    const auto zero = [](auto& bytes) { return std::all_of(std::begin(bytes), std::end(bytes), [](auto value) { return value == 0; }); };
    return zero(event.data) && zero(event.firstString) && zero(event.secondString);
}

bool EventUntouched(const WebBrowserDialogEvent& event) {
    const auto marked = [](auto& bytes) { return std::all_of(std::begin(bytes), std::end(bytes), [](auto value) { return value == 0x5a; }); };
    return marked(event.untouched0) && marked(event.untouched1);
}

template <typename TCall>
void RequireNotImplemented(TCall call) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("not implemented") != std::string::npos;
    }
    Require(threw);
}

void RequireRequests(const void* openParam) {
    static WebBrowserDialogEvent event{};
    WebBrowserDialogNavigateParam navigate{};
    SetBase(navigate);
    navigate.size = sizeof(navigate);
    navigate.url = "http://example.com";
    WebBrowserDialogZoomParam zoom{};
    SetBase(zoom);
    zoom.size = sizeof(zoom);
    zoom.zoom = 1.5f;
    event.size = 1;

    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogSetZoom(&zoom) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    navigate.size = 1;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.size = sizeof(navigate);
    zoom.type = 3;
    Require(sceWebBrowserDialogSetZoom(&zoom) == COMMON_DIALOG_ERROR_PARAMETER);
    zoom.type = 0;
    Require(libSceWebBrowserDialogUnknown_8r4EJ3FiX4w_nid_no_patch(openParam) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);

    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(libSceWebBrowserDialogUnknown_8r4EJ3FiX4w_nid_no_patch(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);

    Require(sceWebBrowserDialogNavigate(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    errno = 71;
    Require(sceWebBrowserDialogNavigate(reinterpret_cast<const WebBrowserDialogNavigateParam*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    navigate.base.magic ^= 1u;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.base.magic ^= 1u;
    navigate.base.reserved[35] = 1;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.base.reserved[35] = 0;
    navigate.size = sizeof(navigate) + 8;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.size = sizeof(navigate);
    navigate.url = nullptr;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.url = "";
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    errno = 71;
    navigate.url = reinterpret_cast<const char*>(0x10);
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(errno == 71);
    static char longUrl[0x801];
    std::fill(std::begin(longUrl), std::end(longUrl), 'a');
    longUrl[0x800] = '\0';
    navigate.url = longUrl;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    longUrl[0x7ff] = '\0';
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    navigate.url = "http://example.com";
    navigate.reserved[255] = 1;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.reserved[255] = 0;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);

    const auto zoomResult = [&zoom](float value, std::uint32_t type, std::uint32_t width, std::uint32_t height) {
        zoom.zoom = value;
        zoom.type = type;
        zoom.width = width;
        zoom.height = height;
        return sceWebBrowserDialogSetZoom(&zoom);
    };
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const std::uint32_t type : {0u, 1u}) {
        Require(zoomResult(0.0f, type, 0, 0) == COMMON_DIALOG_ERROR_PARAMETER);
        Require(zoomResult(-1.0f, type, 0, 0) == COMMON_DIALOG_ERROR_PARAMETER);
        Require(zoomResult(std::nextafter(3.0f, 4.0f), type, 0, 0) == COMMON_DIALOG_ERROR_PARAMETER);
        Require(zoomResult(std::numeric_limits<float>::min(), type, 0, 0) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
        Require(zoomResult(3.0f, type, 0, 0) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
        Require(zoomResult(nan, type, 0, 0) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    }
    Require(zoomResult(1.0f, 3, 1, 1) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(zoomResult(1.0f, 0xffffffffu, 1, 1) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(zoomResult(0.0f, 2, 1, 1) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(zoomResult(0.0f, 2, 1920, 1080) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(zoomResult(0.0f, 2, 0, 1080) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(zoomResult(0.0f, 2, 1921, 1080) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(zoomResult(0.0f, 2, 1920, 0) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(zoomResult(0.0f, 2, 1920, 1081) == COMMON_DIALOG_ERROR_PARAMETER);
    zoom.reserved[247] = 1;
    Require(zoomResult(1.0f, 0, 0, 0) == COMMON_DIALOG_ERROR_PARAMETER);
    zoom.reserved[247] = 0;
    zoom.size = 0;
    Require(zoomResult(1.0f, 0, 0, 0) == COMMON_DIALOG_ERROR_PARAMETER);
    zoom.size = sizeof(zoom);
    zoom.base.size = 0;
    Require(zoomResult(1.0f, 0, 0, 0) == COMMON_DIALOG_ERROR_PARAMETER);
    zoom.base.size = sizeof(zoom.base);
    errno = 71;
    Require(sceWebBrowserDialogSetZoom(reinterpret_cast<const WebBrowserDialogZoomParam*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    Require(zoomResult(1.0f, 0, 0, 0) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);

    Require(sceWebBrowserDialogOpen(openParam) == 0);
    Require(sceWebBrowserDialogGetEvent(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_PARAMETER);
    std::memset(&event, 0x5a, sizeof(event));
    event.size = sizeof(event);
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_PARAMETER);
    std::fill(std::begin(event.reserved), std::end(event.reserved), 0);
    event.reserved[255] = 1;
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_PARAMETER);
    event.reserved[255] = 0;
    errno = 71;
    Require(sceWebBrowserDialogGetEvent(&event) == 0);
    Require(errno == 71);
    Require(EventZeroed(event) && EventUntouched(event) && event.size == sizeof(event));
    errno = 71;
    Require(sceWebBrowserDialogGetEvent(reinterpret_cast<WebBrowserDialogEvent*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    Require(sceWebBrowserDialogNavigate(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    navigate.size = 1;
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_PARAMETER);
    navigate.size = sizeof(navigate);
    zoom.type = 3;
    Require(sceWebBrowserDialogSetZoom(&zoom) == COMMON_DIALOG_ERROR_PARAMETER);
    zoom.type = 0;
    RequireNotImplemented([&navigate] { sceWebBrowserDialogNavigate(&navigate); });
    RequireNotImplemented([&zoom] { sceWebBrowserDialogSetZoom(&zoom); });

    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogSetZoom(&zoom) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    WebBrowserDialogResult result{};
    Require(sceWebBrowserDialogGetResult(&result) == 0);
    Require(result.result == COMMON_DIALOG_RESULT_USER_CANCELED);

    Require(libSceWebBrowserDialogUnknown_8r4EJ3FiX4w_nid_no_patch(openParam) == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogGetEvent(&event) == 0);
    Require(sceWebBrowserDialogSetCookie(nullptr) == COMMON_DIALOG_ERROR_INVALID_STATE);
    Require(sceWebBrowserDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);

    WebBrowserDialogAllowList allowList{};
    allowList.size = sizeof(allowList);
    allowList.addresses[0] = "test.invalid";
    Require(sceWebBrowserDialogOpenForPredeterminedContent(openParam, &allowList) == 0);
    Require(sceWebBrowserDialogGetEvent(&event) == 0);
    RequireNotImplemented([&zoom] { sceWebBrowserDialogSetZoom(&zoom); });
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogNavigate(&navigate) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetEvent(&event) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    std::uint8_t param[64] = {};
    WebBrowserDialogAllowList allowList{};
    allowList.size = sizeof(allowList);
    allowList.addresses[0] = "test.invalid";
    WebBrowserDialogResult result{};
    result.result = 0x5a5a5a5a;

    Require(sceWebBrowserDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogSetCookie(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogResetCookie(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetEvent(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogNavigate(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogSetZoom(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceWebBrowserDialogClose() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpen(param) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    errno = 71;
    Require(sceWebBrowserDialogGetResult(reinterpret_cast<WebBrowserDialogResult*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(result.result == 0x5a5a5a5a);

    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogInitialize() == COMMON_DIALOG_ERROR_ALREADY_INITIALIZED);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_FINISHED);
    Require(result.result == 0x5a5a5a5a);
    Require(sceWebBrowserDialogSetCookie(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogResetCookie(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    WebBrowserDialogCookieParam badCookie{};
    badCookie.size = 1;
    Require(sceWebBrowserDialogSetCookie(&badCookie) == COMMON_DIALOG_ERROR_PARAMETER);
    WebBrowserDialogResetCookieParam badReset{};
    badReset.size = 1;
    errno = 71;
    Require(sceWebBrowserDialogResetCookie(&badReset) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceWebBrowserDialogSetCookie(reinterpret_cast<const WebBrowserDialogCookieParam*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    errno = 71;
    Require(sceWebBrowserDialogResetCookie(reinterpret_cast<const WebBrowserDialogResetCookieParam*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    WebBrowserDialogCookieParam badAddressCookie{};
    badAddressCookie.size = sizeof(badAddressCookie);
    badAddressCookie.address = reinterpret_cast<const char*>(0x10);
    badAddressCookie.cookie = "test=1";
    errno = 71;
    Require(sceWebBrowserDialogSetCookie(&badAddressCookie) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(errno == 71);
    WebBrowserDialogCookieParam badValueCookie{};
    badValueCookie.size = sizeof(badValueCookie);
    badValueCookie.address = "http://example.com";
    badValueCookie.cookie = reinterpret_cast<const char*>(0x10);
    errno = 71;
    Require(sceWebBrowserDialogSetCookie(&badValueCookie) == COMMON_DIALOG_ERROR_PARAMETER);
    Require(errno == 71);

    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetEvent(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogNavigate(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogSetZoom(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    errno = 71;
    Require(sceWebBrowserDialogOpen(reinterpret_cast<const void*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    errno = 71;
    Require(sceWebBrowserDialogOpen(param) == 0);
    Require(errno == 71);
    Require(sceWebBrowserDialogSetCookie(nullptr) == COMMON_DIALOG_ERROR_INVALID_STATE);
    Require(sceWebBrowserDialogResetCookie(&badReset) == COMMON_DIALOG_ERROR_INVALID_STATE);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);

    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    errno = 71;
    Require(sceWebBrowserDialogGetResult(reinterpret_cast<WebBrowserDialogResult*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    WebBrowserDialogResult badExtendedResult{};
    badExtendedResult.extended = reinterpret_cast<WebBrowserDialogExtendedResult*>(0x10);
    errno = 71;
    Require(sceWebBrowserDialogGetResult(&badExtendedResult) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    errno = 71;
    Require(sceWebBrowserDialogGetResult(&result) == 0);
    Require(errno == 71);
    Require(result.result == COMMON_DIALOG_RESULT_USER_CANCELED);

    Require(sceWebBrowserDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(nullptr, &allowList) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    errno = 71;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(reinterpret_cast<const void*>(0x10), &allowList) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    errno = 71;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, reinterpret_cast<const WebBrowserDialogAllowList*>(0x10)) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(errno == 71);
    WebBrowserDialogAllowList badAddressList = allowList;
    badAddressList.addresses[0] = reinterpret_cast<const char*>(0x10);
    errno = 71;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &badAddressList) == static_cast<int>(0x80B8000Au));
    Require(errno == 71);
    WebBrowserDialogAllowList badSizeAllowList = allowList;
    badSizeAllowList.size = sizeof(badSizeAllowList) - 1;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &badSizeAllowList) == static_cast<int>(0x80B8000Au));
    allowList.addresses[0] = nullptr;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == static_cast<int>(0x80B8000Au));
    allowList.addresses[19] = "test.invalid";
    allowList.reserved[255] = 1;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == static_cast<int>(0x80B8000Au));
    allowList.reserved[255] = 0;
    allowList.addresses[19] = "";
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == static_cast<int>(0x80B8000Au));
    char longAddress[257];
    std::fill(std::begin(longAddress), std::end(longAddress), 'a');
    longAddress[256] = '\0';
    allowList.addresses[19] = longAddress;
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == static_cast<int>(0x80B8000Au));
    longAddress[255] = '\0';
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);

    result.result = 0x5a5a5a5a;
    Require(sceWebBrowserDialogGetResult(&result) == 0);
    Require(result.result == COMMON_DIALOG_RESULT_USER_CANCELED);

    Require(sceWebBrowserDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_FINISHED);
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    result.reserved[239] = 1;
    Require(sceWebBrowserDialogGetResult(&result) == static_cast<int>(0x80B8000Au));
    result.reserved[239] = 0;
    WebBrowserDialogExtendedResult extended{};
    result.extended = &extended;
    Require(sceWebBrowserDialogGetResult(&result) == static_cast<int>(0x80B8000Au));
    extended.size = sizeof(extended);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogOpen(param) == 0);
    extended.reserved0[3] = 1;
    Require(sceWebBrowserDialogGetResult(&result) == static_cast<int>(0x80B8000Au));
    extended.reserved0[3] = 0;
    extended.reserved1[19] = 1;
    Require(sceWebBrowserDialogGetResult(&result) == static_cast<int>(0x80B8000Au));
    extended.reserved1[19] = 0;
    extended.type = 0x5a5a5a5a;
    extended.content[2047] = 'x';
    result.padding = 0x5a5a5a5a;
    Require(sceWebBrowserDialogGetResult(&result) == 0);
    Require(result.result == COMMON_DIALOG_RESULT_USER_CANCELED);
    Require(result.padding == 0x5a5a5a5a && result.extended == &extended);
    Require(extended.type == 0x5a5a5a5a && extended.content[2047] == 'x');
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpen(param) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogClose() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_NONE);
    RequireRequests(param);
    RequireNotices(param);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
