#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore15localizedStringEPKc(const char* key);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore18pluginTooSmallTextEv();
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore22multipleFileUploadTextEj(unsigned numberOfFiles);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32useBlockedPlugInContextMenuTitleEv();

}
