#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/PasteboardWriterDataAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore20PasteboardWriterDataC1Ev(NKWebKit::WebCoreAbi::PasteboardWriterData* self);
void APS5_VABI _ZN7WebCore20PasteboardWriterDataC2Ev(NKWebKit::WebCoreAbi::PasteboardWriterData* self);
void APS5_VABI _ZN7WebCore20PasteboardWriterDataD1Ev(NKWebKit::WebCoreAbi::PasteboardWriterData* self);
void APS5_VABI _ZN7WebCore20PasteboardWriterDataD2Ev(NKWebKit::WebCoreAbi::PasteboardWriterData* self);
bool APS5_VABI _ZNK7WebCore20PasteboardWriterData7isEmptyEv(const NKWebKit::WebCoreAbi::PasteboardWriterData* self);

}
