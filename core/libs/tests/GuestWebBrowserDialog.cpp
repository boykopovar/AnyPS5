#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceWebBrowserDialogInitialize(void);
int APS5_VABI sceWebBrowserDialogTerminate(void);
int APS5_VABI sceWebBrowserDialogOpen(const void* param);
int APS5_VABI sceWebBrowserDialogGetResult(void* result);
int APS5_VABI sceWebBrowserDialogSetCookie(const void* param);
int APS5_VABI sceWebBrowserDialogResetCookie(const void* param);
int APS5_VABI sceWebBrowserDialogOpenForPredeterminedContent(const void* param, const void* contentParam);
int APS5_VABI sceWebBrowserDialogGetStatus(void);
int APS5_VABI sceWebBrowserDialogUpdateStatus(void);
}

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);

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

struct PredeterminedContentParam {
    std::size_t size;
    const char* domain[20];
    char reserved[256];
};
static_assert(sizeof(PredeterminedContentParam) == 424);

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

}

int main() {
    std::uint8_t param[64] = {};
    SetCookieParam validCookie = {};
    validCookie.size = sizeof(SetCookieParam);
    validCookie.url = "https://example.com/";
    validCookie.cookie = "key0=value0";
    ResetCookieParam validReset = {};
    validReset.size = sizeof(ResetCookieParam);
    PredeterminedContentParam content = {};
    content.size = sizeof(PredeterminedContentParam);
    content.domain[0] = "example.com";
    Require(sceWebBrowserDialogSetCookie(&validCookie) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogResetCookie(&validReset) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &content) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceWebBrowserDialogSetCookie(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogResetCookie(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogSetCookie(&validCookie) == 0);
    Require(sceWebBrowserDialogResetCookie(&validReset) == 0);
    SetCookieParam badSize = validCookie;
    badSize.size = 0;
    Require(Throws([&] { sceWebBrowserDialogSetCookie(&badSize); }));
    SetCookieParam nullUrl = validCookie;
    nullUrl.url = nullptr;
    Require(Throws([&] { sceWebBrowserDialogSetCookie(&nullUrl); }));
    const std::string longUrl(2048, 'a');
    SetCookieParam unterminatedUrl = validCookie;
    unterminatedUrl.url = longUrl.c_str();
    Require(Throws([&] { sceWebBrowserDialogSetCookie(&unterminatedUrl); }));
    const std::string longCookie(4096, 'b');
    SetCookieParam unterminatedCookie = validCookie;
    unterminatedCookie.cookie = longCookie.c_str();
    Require(Throws([&] { sceWebBrowserDialogSetCookie(&unterminatedCookie); }));
    ResetCookieParam badReset = validReset;
    badReset.size = 0;
    Require(Throws([&] { sceWebBrowserDialogResetCookie(&badReset); }));
    const std::string maxUrl(2047, 'a');
    const std::string maxCookie(4095, 'b');
    SetCookieParam boundaryCookie = validCookie;
    boundaryCookie.url = maxUrl.c_str();
    boundaryCookie.cookie = maxCookie.c_str();
    Require(sceWebBrowserDialogSetCookie(&boundaryCookie) == 0);
    Require(sceWebBrowserDialogTerminate() == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceWebBrowserDialogInitialize() == 0);
    Require(sceWebBrowserDialogOpen(param) == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(nullptr, &content) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceWebBrowserDialogOpenForPredeterminedContent(param, &content) == 0);
    Require(sceWebBrowserDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    std::int32_t result = 0;
    Require(sceWebBrowserDialogGetResult(&result) == 0);
    Require(result == COMMON_DIALOG_RESULT_USER_CANCELED);
    Require(sceWebBrowserDialogSetCookie(&validCookie) == 0);
    Require(sceWebBrowserDialogResetCookie(&validReset) == 0);
}
