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
int APS5_VABI sceWebBrowserDialogGetEvent();
int APS5_VABI sceWebBrowserDialogNavigate();
int APS5_VABI sceWebBrowserDialogSetZoom();

}
