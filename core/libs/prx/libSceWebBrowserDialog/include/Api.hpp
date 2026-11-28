#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

struct WebBrowserDialogCookieParam {
    std::uint64_t size;
    const char* address;
    const char* cookie;
    std::uint8_t reserved[256];
};

struct WebBrowserDialogResetCookieParam {
    std::uint64_t size;
    std::uint8_t reserved[256];
};

struct WebBrowserDialogNavigateParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    const char* url;
    std::uint8_t reserved[256];
};

struct WebBrowserDialogZoomParam {
    CommonDialogBaseParam base;
    std::uint64_t size;
    float zoom;
    std::uint32_t type;
    std::uint32_t width;
    std::uint32_t height;
    std::uint8_t reserved[248];
};

struct WebBrowserDialogEvent {
    std::uint64_t size;
    std::uint8_t untouched0[4];
    std::uint8_t data[16];
    std::uint8_t untouched1[20];
    char firstString[2048];
    char secondString[2048];
    std::uint8_t reserved[256];
};

extern "C" {

int APS5_VABI sceWebBrowserDialogInitialize(void);
int APS5_VABI sceWebBrowserDialogTerminate(void);
int APS5_VABI sceWebBrowserDialogClose(void);
int APS5_VABI sceWebBrowserDialogGetResult(WebBrowserDialogResult* result);
int APS5_VABI sceWebBrowserDialogGetStatus(void);
int APS5_VABI sceWebBrowserDialogOpen(const void* param);
int APS5_VABI sceWebBrowserDialogUpdateStatus(void);
int APS5_VABI sceWebBrowserDialogSetCookie(const WebBrowserDialogCookieParam* param);
int APS5_VABI sceWebBrowserDialogOpenForPredeterminedContent(const void* param, const WebBrowserDialogAllowList* allowList);
int APS5_VABI sceWebBrowserDialogResetCookie(const WebBrowserDialogResetCookieParam* param);
int APS5_VABI sceWebBrowserDialogGetEvent(WebBrowserDialogEvent* event);
int APS5_VABI sceWebBrowserDialogNavigate(const WebBrowserDialogNavigateParam* param);
int APS5_VABI sceWebBrowserDialogSetZoom(const WebBrowserDialogZoomParam* param);

}
