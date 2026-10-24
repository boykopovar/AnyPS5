#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct ResetCookieParam {
    std::uint64_t size;
    std::uint8_t reserved[256];
};

struct SetCookieParam {
    std::uint64_t size;
    const char* url;
    const char* cookie;
    std::uint8_t reserved[256];
};

extern "C" {
int APS5_VABI sceWebBrowserDialogInitialize(void);
int APS5_VABI sceWebBrowserDialogTerminate(void);
int APS5_VABI sceWebBrowserDialogOpen(const void* param);
int APS5_VABI sceWebBrowserDialogClose(void);
int APS5_VABI sceWebBrowserDialogUpdateStatus(void);
int APS5_VABI sceWebBrowserDialogSetCookie(const SetCookieParam* param);
int APS5_VABI sceWebBrowserDialogResetCookie(const ResetCookieParam* param);
}

namespace {

constexpr int NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int INVALID_STATE = static_cast<int>(0x80B80006u);
constexpr int NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int PARAM_INVALID = static_cast<int>(0x80B8000Au);
constexpr int ARG_NULL = static_cast<int>(0x80B8000Du);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

SetCookieParam Cookie(const char* url, const char* cookie) {
    SetCookieParam param {};
    param.size = sizeof(param);
    param.url = url;
    param.cookie = cookie;
    return param;
}

ResetCookieParam Reset() {
    ResetCookieParam param {};
    param.size = sizeof(param);
    return param;
}

void CheckSetCookieParameters() {
    auto valid = Cookie("https://example.com", "a=b");
    Require(sceWebBrowserDialogSetCookie(&valid) == 0, "valid SetCookie");
    Require(sceWebBrowserDialogSetCookie(nullptr) == ARG_NULL, "SetCookie null");

    auto wrongSize = valid;
    wrongSize.size = sizeof(ResetCookieParam);
    Require(sceWebBrowserDialogSetCookie(&wrongSize) == PARAM_INVALID, "SetCookie size");
    auto noUrl = Cookie(nullptr, "a=b");
    Require(sceWebBrowserDialogSetCookie(&noUrl) == PARAM_INVALID, "SetCookie null url");
    auto emptyUrl = Cookie("", "a=b");
    Require(sceWebBrowserDialogSetCookie(&emptyUrl) == PARAM_INVALID, "SetCookie empty url");
    auto noCookie = Cookie("https://example.com", nullptr);
    Require(sceWebBrowserDialogSetCookie(&noCookie) == PARAM_INVALID, "SetCookie null cookie");
    auto emptyCookie = Cookie("https://example.com", "");
    Require(sceWebBrowserDialogSetCookie(&emptyCookie) == PARAM_INVALID, "SetCookie empty cookie");

    const std::string longestUrl(0x7ff, 'u');
    const std::string longUrl(0x800, 'u');
    auto longest = Cookie(longestUrl.c_str(), "a=b");
    Require(sceWebBrowserDialogSetCookie(&longest) == 0, "SetCookie 2047-byte url");
    auto tooLong = Cookie(longUrl.c_str(), "a=b");
    Require(sceWebBrowserDialogSetCookie(&tooLong) == PARAM_INVALID, "SetCookie 2048-byte url");

    const std::string longestCookie(0xfff, 'c');
    const std::string longCookie(0x1000, 'c');
    auto longestValue = Cookie("https://example.com", longestCookie.c_str());
    Require(sceWebBrowserDialogSetCookie(&longestValue) == 0, "SetCookie 4095-byte cookie");
    auto tooLongValue = Cookie("https://example.com", longCookie.c_str());
    Require(sceWebBrowserDialogSetCookie(&tooLongValue) == PARAM_INVALID, "SetCookie 4096-byte cookie");

    auto reserved = valid;
    reserved.reserved[255] = 1;
    Require(sceWebBrowserDialogSetCookie(&reserved) == PARAM_INVALID, "SetCookie reserved");
}

void CheckResetCookieParameters() {
    auto valid = Reset();
    Require(sceWebBrowserDialogResetCookie(&valid) == 0, "valid ResetCookie");
    Require(sceWebBrowserDialogResetCookie(nullptr) == ARG_NULL, "ResetCookie null");
    auto wrongSize = valid;
    wrongSize.size = sizeof(SetCookieParam);
    Require(sceWebBrowserDialogResetCookie(&wrongSize) == PARAM_INVALID, "ResetCookie size");
    auto reserved = valid;
    reserved.reserved[0] = 1;
    Require(sceWebBrowserDialogResetCookie(&reserved) == PARAM_INVALID, "ResetCookie reserved");
}

void CheckOpenDialog() {
    std::uint8_t open[64] = {};
    auto cookie = Cookie("https://example.com", "a=b");
    auto reset = Reset();
    Require(sceWebBrowserDialogOpen(open) == 0, "Open");
    Require(sceWebBrowserDialogSetCookie(&cookie) == INVALID_STATE, "SetCookie while open");
    Require(sceWebBrowserDialogSetCookie(nullptr) == INVALID_STATE, "SetCookie checks the open dialog before null");
    Require(sceWebBrowserDialogResetCookie(nullptr) == ARG_NULL, "ResetCookie checks null before the open dialog");
    Require(sceWebBrowserDialogResetCookie(&reset) == INVALID_STATE, "ResetCookie while open");
    Require(sceWebBrowserDialogUpdateStatus() == 3, "UpdateStatus finished");
    Require(sceWebBrowserDialogSetCookie(&cookie) == 0, "SetCookie after UpdateStatus");
    Require(sceWebBrowserDialogResetCookie(&reset) == 0, "ResetCookie after UpdateStatus");

    Require(sceWebBrowserDialogOpen(open) == 0, "second Open");
    Require(sceWebBrowserDialogClose() == NOT_RUNNING, "Close of a finished dialog");
    Require(sceWebBrowserDialogSetCookie(&cookie) == 0, "SetCookie after Close");

    Require(sceWebBrowserDialogOpen(open) == 0, "third Open");
    Require(sceWebBrowserDialogTerminate() == 0, "Terminate");
    Require(sceWebBrowserDialogInitialize() == 0, "Initialize again");
    Require(sceWebBrowserDialogResetCookie(&reset) == 0, "ResetCookie after Terminate");
}

}

int main() {
    auto cookie = Cookie("https://example.com", "a=b");
    auto reset = Reset();
    Require(sceWebBrowserDialogSetCookie(&cookie) == NOT_INITIALIZED, "SetCookie before Initialize");
    Require(sceWebBrowserDialogSetCookie(nullptr) == NOT_INITIALIZED, "SetCookie null before Initialize");
    Require(sceWebBrowserDialogResetCookie(&reset) == NOT_INITIALIZED, "ResetCookie before Initialize");
    Require(sceWebBrowserDialogResetCookie(nullptr) == NOT_INITIALIZED, "ResetCookie null before Initialize");

    Require(sceWebBrowserDialogInitialize() == 0, "Initialize");
    CheckSetCookieParameters();
    CheckResetCookieParameters();
    CheckOpenDialog();
    Require(sceWebBrowserDialogTerminate() == 0, "final Terminate");
    std::puts("guest web browser dialog cookies: PASS");
    return 0;
}
