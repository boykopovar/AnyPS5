#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/PasteboardAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::PasteboardUniquePtr APS5_VABI _ZN7WebCore10Pasteboard21createForCopyAndPasteEv();
void APS5_VABI _ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE(NKWebKit::WebCoreAbi::Pasteboard* self, const NKWebKit::WtfAbi::String* text, NKWebKit::WebCoreAbi::SmartReplaceOption smartReplaceOption);

}
