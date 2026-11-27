#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/AsyncFileStreamAbi.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore15AsyncFileStreamC1ERNS_16FileStreamClientE(NKWebKit::WebCoreAbi::AsyncFileStream* self, NKWebKit::WebCoreAbi::FileStreamClient* client);
void APS5_VABI _ZN7WebCore15AsyncFileStreamC2ERNS_16FileStreamClientE(NKWebKit::WebCoreAbi::AsyncFileStream* self, NKWebKit::WebCoreAbi::FileStreamClient* client);
void APS5_VABI _ZN7WebCore15AsyncFileStreamD1Ev(NKWebKit::WebCoreAbi::AsyncFileStream* self);
void APS5_VABI _ZN7WebCore15AsyncFileStreamD2Ev(NKWebKit::WebCoreAbi::AsyncFileStream* self);
void APS5_VABI _ZN7WebCore15AsyncFileStream11openForReadERKN3WTF6StringExx(
    NKWebKit::WebCoreAbi::AsyncFileStream* self, const NKWebKit::WtfAbi::String* path, long long offset, long long length);
void APS5_VABI _ZN7WebCore15AsyncFileStream5closeEv(NKWebKit::WebCoreAbi::AsyncFileStream* self);
void APS5_VABI _ZN7WebCore15AsyncFileStream7performEON3WTF8FunctionIFNS2_IFvRNS_16FileStreamClientEEEERNS_10FileStreamEEEE(
    NKWebKit::WebCoreAbi::AsyncFileStream* self, NKWebKit::WebCoreAbi::FileStreamOperationFunction* operation);
void APS5_VABI _ZN7WebCore15AsyncFileStream4readEPci(NKWebKit::WebCoreAbi::AsyncFileStream* self, char* buffer, int length);
void APS5_VABI _ZN7WebCore15AsyncFileStream7getSizeERKN3WTF6StringENS1_8OptionalINS1_8WallTimeEEE(
    NKWebKit::WebCoreAbi::AsyncFileStream* self, const NKWebKit::WtfAbi::String* path, const NKWebKit::WebCoreAbi::OptionalWallTime* expectedModificationTime);

}
