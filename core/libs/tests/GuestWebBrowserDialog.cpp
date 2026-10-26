#include "prx/libSceWebBrowserDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint8_t param[64] = {};
    WebBrowserDialogAllowList allowList{};
    allowList.size = sizeof(allowList);
    allowList.addresses[0] = "test.invalid";
    WebBrowserDialogResult result{};
    result.result = 0x5a5a5a5a;

    Require(sceWebBrowserDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogSetCookie(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogResetCookie(nullptr) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceWebBrowserDialogClose() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpen(param) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(result.result == 0x5a5a5a5a);

    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogInitialize() == COMMON_DIALOG_ERROR_ALREADY_INITIALIZED);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceWebBrowserDialogGetResult(&result) == COMMON_DIALOG_ERROR_NOT_FINISHED);
    Require(result.result == 0x5a5a5a5a);

    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogOpen(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogOpen(param) == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);

    Require(sceWebBrowserDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogGetResult(&result) == 0);
    Require(result.result == COMMON_DIALOG_RESULT_USER_CANCELED);

    Require(sceWebBrowserDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &allowList) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(nullptr, &allowList) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
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
    return 0;
}
