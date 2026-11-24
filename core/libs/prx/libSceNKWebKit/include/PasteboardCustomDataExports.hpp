#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/PasteboardCustomDataAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::PasteboardCustomData APS5_VABI _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(const NKWebKit::WebCoreAbi::SharedBuffer* buffer);
NKWebKit::WebCoreAbi::PasteboardCustomData APS5_VABI _ZN7WebCore20PasteboardCustomData22fromPersistenceDecoderEON3WTF11Persistence7DecoderE(NKWebKit::WtfAbi::Decoder* decoder);
void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1EOS1_(NKWebKit::WebCoreAbi::PasteboardEntry* self, NKWebKit::WebCoreAbi::PasteboardEntry* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1ERKS1_(NKWebKit::WebCoreAbi::PasteboardEntry* self, const NKWebKit::WebCoreAbi::PasteboardEntry* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1Ev(NKWebKit::WebCoreAbi::PasteboardEntry* self);
void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2EOS1_(NKWebKit::WebCoreAbi::PasteboardEntry* self, NKWebKit::WebCoreAbi::PasteboardEntry* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2ERKS1_(NKWebKit::WebCoreAbi::PasteboardEntry* self, const NKWebKit::WebCoreAbi::PasteboardEntry* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2Ev(NKWebKit::WebCoreAbi::PasteboardEntry* self);
NKWebKit::WebCoreAbi::PasteboardEntry& APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryaSEOS1_(NKWebKit::WebCoreAbi::PasteboardEntry* self, NKWebKit::WebCoreAbi::PasteboardEntry* other);
NKWebKit::WebCoreAbi::PasteboardEntry& APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryaSERKS1_(NKWebKit::WebCoreAbi::PasteboardEntry* self, const NKWebKit::WebCoreAbi::PasteboardEntry* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::PasteboardCustomData* self, NKWebKit::WtfAbi::String* origin, NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::PasteboardEntry>* data);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1EOS0_(NKWebKit::WebCoreAbi::PasteboardCustomData* self, NKWebKit::WebCoreAbi::PasteboardCustomData* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1ERKS0_(NKWebKit::WebCoreAbi::PasteboardCustomData* self, const NKWebKit::WebCoreAbi::PasteboardCustomData* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1Ev(NKWebKit::WebCoreAbi::PasteboardCustomData* self);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::PasteboardCustomData* self, NKWebKit::WtfAbi::String* origin, NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::PasteboardEntry>* data);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2EOS0_(NKWebKit::WebCoreAbi::PasteboardCustomData* self, NKWebKit::WebCoreAbi::PasteboardCustomData* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2ERKS0_(NKWebKit::WebCoreAbi::PasteboardCustomData* self, const NKWebKit::WebCoreAbi::PasteboardCustomData* other);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2Ev(NKWebKit::WebCoreAbi::PasteboardCustomData* self);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataD1Ev(NKWebKit::WebCoreAbi::PasteboardCustomData* self);
void APS5_VABI _ZN7WebCore20PasteboardCustomDataD2Ev(NKWebKit::WebCoreAbi::PasteboardCustomData* self);
NKWebKit::WebCoreAbi::PasteboardCustomData& APS5_VABI _ZN7WebCore20PasteboardCustomDataaSERKS0_(NKWebKit::WebCoreAbi::PasteboardCustomData* self, const NKWebKit::WebCoreAbi::PasteboardCustomData* other);
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(const NKWebKit::WebCoreAbi::PasteboardCustomData* self);

}
