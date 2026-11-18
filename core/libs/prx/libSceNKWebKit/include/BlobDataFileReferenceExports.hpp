#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::BlobDataFileReference* self, const NKWebKit::WtfAbi::String* path, const NKWebKit::WtfAbi::String* replacementPath);
void APS5_VABI _ZN7WebCore21BlobDataFileReferenceC2ERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::BlobDataFileReference* self, const NKWebKit::WtfAbi::String* path, const NKWebKit::WtfAbi::String* replacementPath);
void APS5_VABI _ZN7WebCore21BlobDataFileReferenceD0Ev(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
void APS5_VABI _ZN7WebCore21BlobDataFileReferenceD1Ev(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
void APS5_VABI _ZN7WebCore21BlobDataFileReferenceD2Ev(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
NKWebKit::WebCoreAbi::BlobDataFileReferenceRef APS5_VABI _ZN7WebCore21BlobDataFileReference6createERKN3WTF6StringES4_(const NKWebKit::WtfAbi::String* path, const NKWebKit::WtfAbi::String* replacementPath);
const NKWebKit::WtfAbi::String* APS5_VABI _ZN7WebCore21BlobDataFileReference4pathEv(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
NKWebKit::WebCoreAbi::OptionalWallTime APS5_VABI _ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
void APS5_VABI _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
void APS5_VABI _ZN7WebCore21BlobDataFileReference20prepareForFileAccessEv(NKWebKit::WebCoreAbi::BlobDataFileReference* self);
void APS5_VABI _ZN7WebCore21BlobDataFileReference16revokeFileAccessEv(NKWebKit::WebCoreAbi::BlobDataFileReference* self);

}
