#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/PasteboardAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore10Pasteboard11writeStringERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::Pasteboard* self, const NKWebKit::WtfAbi::String* type, const NKWebKit::WtfAbi::String* data);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(NKWebKit::WebCoreAbi::Pasteboard* self, const NKWebKit::WtfAbi::String* type);
NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZN7WebCore10Pasteboard14readAllStringsERKN3WTF6StringE(NKWebKit::WebCoreAbi::Pasteboard* self, const NKWebKit::WtfAbi::String* type);
NKWebKit::WebCoreAbi::PasteboardUniquePtr APS5_VABI _ZN7WebCore10Pasteboard21createForCopyAndPasteEv();
void APS5_VABI _ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE(NKWebKit::WebCoreAbi::Pasteboard* self, const NKWebKit::WtfAbi::String* text, NKWebKit::WebCoreAbi::SmartReplaceOption smartReplaceOption);

}
