#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore17standardUserAgentERKN3WTF6StringES3_(const NKWebKit::WtfAbi::String* applicationName, const NKWebKit::WtfAbi::String* applicationVersion);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore23standardUserAgentForURLERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url);
}
