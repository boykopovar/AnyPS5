#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreSwitches.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore13AXObjectCache19enableAccessibilityEv();
void APS5_VABI _ZN7WebCore13AXObjectCache20disableAccessibilityEv();
void APS5_VABI _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(bool flag);
void APS5_VABI _ZN7WebCore11FontCascade11setCodePathENS0_8CodePathE(NKWebKit::WebCoreAbi::FontCascadeCodePath codePath);
void APS5_VABI _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(bool shouldUseSmoothing);
bool APS5_VABI _ZN7WebCore11FontCascade18shouldUseSmoothingEv();

}
