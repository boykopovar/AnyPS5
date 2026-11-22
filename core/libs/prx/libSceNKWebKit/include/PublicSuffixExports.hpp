#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" bool APS5_VABI _ZN7WebCore14isPublicSuffixERKN3WTF6StringE(const NKWebKit::WtfAbi::String* domain);
extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(const NKWebKit::WtfAbi::String* domain);
