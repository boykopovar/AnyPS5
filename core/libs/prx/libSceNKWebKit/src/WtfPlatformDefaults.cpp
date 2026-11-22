#include "prx/libSceNKWebKit/include/WtfPlatformDefaultsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

constexpr int GuestLcMessages = 6;
constexpr int GuestProtNone = 0;
constexpr int GuestProtRead = 1;
constexpr int GuestProtWrite = 2;

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF11OSAllocator18releaseDecommittedEPvm(void* address, std::size_t bytes) {
    if (munmap_nid_postfix(address, bytes) != 0) WTFCrash();
}

void APS5_VABI _ZN3WTF11OSAllocator23hintMemoryNotNeededSoonEPvm(void*, std::size_t) {
}

void APS5_VABI _ZN3WTF11OSAllocator6commitEPvmbb(void* address, std::size_t bytes, bool writable, bool) {
    if (mprotect_nid_postfix(address, bytes, writable ? GuestProtRead | GuestProtWrite : GuestProtRead) != 0) WTFCrash();
}

void APS5_VABI _ZN3WTF11OSAllocator8decommitEPvm(void* address, std::size_t bytes) {
    if (mprotect_nid_postfix(address, bytes, GuestProtNone) != 0) WTFCrash();
}

void APS5_VABI _ZN3WTF14FileSystemImpl14setMetadataURLERKNS_6StringES3_S3_(const NKWebKit::WtfAbi::String&, const NKWebKit::WtfAbi::String&, const NKWebKit::WtfAbi::String&) {
}

bool APS5_VABI _ZN3WTF14FileSystemImpl29makeSafeToUseMemoryMapForPathERKNS_6StringE(const NKWebKit::WtfAbi::String&) {
    return true;
}

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14FileSystemImpl25createTemporaryZipArchiveERKNS_6StringE(const NKWebKit::WtfAbi::String&) {
    return NKWebKit::WtfAbi::String();
}

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF21bootSessionUUIDStringEv() {
    return NKWebKit::WtfAbi::String();
}

const char* APS5_VABI _ZN3WTF21currentSearchLocaleIDEv() {
    if (const char* localeDefault = setlocale_nid_postfix(GuestLcMessages, nullptr)) return localeDefault;
    return "";
}

const char* APS5_VABI _ZN3WTF24currentTextBreakLocaleIDEv() {
    if (const char* localeDefault = setlocale_nid_postfix(GuestLcMessages, nullptr)) return localeDefault;
    return "en_us";
}

void APS5_VABI _ZN3WTF6Thread31setCurrentThreadIsUserInitiatedEi(int) {
}

void APS5_VABI _ZN3WTF6Thread33setCurrentThreadIsUserInteractiveEi(int) {
}

}
