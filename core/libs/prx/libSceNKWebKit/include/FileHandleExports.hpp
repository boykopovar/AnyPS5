#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/FileHandleAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore10FileHandleC1Ev(NKWebKit::WebCoreAbi::FileHandle* self);
void APS5_VABI _ZN7WebCore10FileHandleC2Ev(NKWebKit::WebCoreAbi::FileHandle* self);
void APS5_VABI _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(NKWebKit::WebCoreAbi::FileHandle* self, const NKWebKit::WtfAbi::String* path, int mode);
void APS5_VABI _ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(NKWebKit::WebCoreAbi::FileHandle* self, const NKWebKit::WtfAbi::String* path, int mode);
void APS5_VABI _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(NKWebKit::WebCoreAbi::FileHandle* self, const NKWebKit::WtfAbi::String* path, int mode, unsigned lockMode);
void APS5_VABI _ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(NKWebKit::WebCoreAbi::FileHandle* self, const NKWebKit::WtfAbi::String* path, int mode, unsigned lockMode);
void APS5_VABI _ZN7WebCore10FileHandleC1EOS0_(NKWebKit::WebCoreAbi::FileHandle* self, NKWebKit::WebCoreAbi::FileHandle* other);
void APS5_VABI _ZN7WebCore10FileHandleC2EOS0_(NKWebKit::WebCoreAbi::FileHandle* self, NKWebKit::WebCoreAbi::FileHandle* other);
void APS5_VABI _ZN7WebCore10FileHandleD1Ev(NKWebKit::WebCoreAbi::FileHandle* self);
void APS5_VABI _ZN7WebCore10FileHandleD2Ev(NKWebKit::WebCoreAbi::FileHandle* self);
NKWebKit::WebCoreAbi::FileHandle& APS5_VABI _ZN7WebCore10FileHandleaSEOS0_(NKWebKit::WebCoreAbi::FileHandle* self, NKWebKit::WebCoreAbi::FileHandle* other);
bool APS5_VABI _ZNK7WebCore10FileHandlecvbEv(const NKWebKit::WebCoreAbi::FileHandle* self);
bool APS5_VABI _ZN7WebCore10FileHandle4openERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(NKWebKit::WebCoreAbi::FileHandle* self, const NKWebKit::WtfAbi::String* path, int mode);
bool APS5_VABI _ZN7WebCore10FileHandle4openEv(NKWebKit::WebCoreAbi::FileHandle* self);
int APS5_VABI _ZN7WebCore10FileHandle4readEPvi(NKWebKit::WebCoreAbi::FileHandle* self, void* data, int length);
int APS5_VABI _ZN7WebCore10FileHandle5writeEPKvi(NKWebKit::WebCoreAbi::FileHandle* self, const void* data, int length);
bool APS5_VABI _ZN7WebCore10FileHandle6printfEPKcz(NKWebKit::WebCoreAbi::FileHandle* self, const char* format, ...);
void APS5_VABI _ZN7WebCore10FileHandle5closeEv(NKWebKit::WebCoreAbi::FileHandle* self);

}
