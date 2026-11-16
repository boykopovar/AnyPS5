#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

extern "C" {

void APS5_VABI _ZN3WTF11OSAllocator23hintMemoryNotNeededSoonEPvm(void* address, std::size_t bytes);
void APS5_VABI _ZN3WTF14FileSystemImpl14setMetadataURLERKNS_6StringES3_S3_(const NKWebKit::WtfAbi::String& path, const NKWebKit::WtfAbi::String& urlString, const NKWebKit::WtfAbi::String& referrer);
bool APS5_VABI _ZN3WTF14FileSystemImpl29makeSafeToUseMemoryMapForPathERKNS_6StringE(const NKWebKit::WtfAbi::String& path);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14FileSystemImpl25createTemporaryZipArchiveERKNS_6StringE(const NKWebKit::WtfAbi::String& directory);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF21bootSessionUUIDStringEv();
const char* APS5_VABI _ZN3WTF21currentSearchLocaleIDEv();
const char* APS5_VABI _ZN3WTF24currentTextBreakLocaleIDEv();
void APS5_VABI _ZN3WTF6Thread31setCurrentThreadIsUserInitiatedEi(int relativePriority);
void APS5_VABI _ZN3WTF6Thread33setCurrentThreadIsUserInteractiveEi(int relativePriority);

}
