#include "prx/libSceNKWebKit/include/WtfPlatformDefaultsExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(sizeof(String) == 8);
static_assert(alignof(String) == 8);
static_assert(!std::is_trivially_copyable_v<String>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF platform defaults check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

StringImpl MakeImpl(const char* text) {
    StringImpl impl{};
    impl.refCount = 3;
    impl.length = static_cast<unsigned>(std::strlen(text));
    impl.data = text;
    impl.hashAndFlags = 4;
    return impl;
}

}  // namespace

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();

    StringImpl pathImpl = MakeImpl("/data/file.bin");
    StringImpl emptyImpl = MakeImpl("");
    String nullString;
    String emptyString(&emptyImpl);
    String path(&pathImpl);

    char buffer[64]{};
    _ZN3WTF11OSAllocator23hintMemoryNotNeededSoonEPvm(nullptr, 0);
    _ZN3WTF11OSAllocator23hintMemoryNotNeededSoonEPvm(buffer, sizeof(buffer));
    _ZN3WTF11OSAllocator23hintMemoryNotNeededSoonEPvm(buffer, SIZE_MAX);

    {
        constexpr std::size_t bytes = 0x8000;
        void* reserved = mmap_nid_postfix(nullptr, bytes, 0, 0x1002, -1, 0);
        Require(reserved != reinterpret_cast<void*>(-1));
        auto* bytesView = static_cast<volatile unsigned char*>(reserved);
        _ZN3WTF11OSAllocator6commitEPvmbb(reserved, bytes, true, false);
        bytesView[0] = 0x5a;
        bytesView[bytes - 1] = 0xa5;
        _ZN3WTF11OSAllocator6commitEPvmbb(reserved, bytes, false, false);
        Require(bytesView[0] == 0x5a && bytesView[bytes - 1] == 0xa5);
        _ZN3WTF11OSAllocator8decommitEPvm(reserved, bytes);
        _ZN3WTF11OSAllocator6commitEPvmbb(reserved, bytes / 2, true, false);
        bytesView[1] = 1;
        _ZN3WTF11OSAllocator8decommitEPvm(reserved, bytes / 2);
        _ZN3WTF11OSAllocator8decommitEPvm(reserved, 0);
        char residency[2]{};
        Require(mincore_nid_postfix(reserved, bytes, residency) == 0);
        _ZN3WTF11OSAllocator18releaseDecommittedEPvm(reserved, bytes);
        Require(mincore_nid_postfix(reserved, bytes, residency) == -1);
    }

    _ZN3WTF14FileSystemImpl14setMetadataURLERKNS_6StringES3_S3_(nullString, nullString, nullString);
    _ZN3WTF14FileSystemImpl14setMetadataURLERKNS_6StringES3_S3_(path, emptyString, path);
    Require(pathImpl.refCount == 3);

    Require(_ZN3WTF14FileSystemImpl29makeSafeToUseMemoryMapForPathERKNS_6StringE(nullString));
    Require(_ZN3WTF14FileSystemImpl29makeSafeToUseMemoryMapForPathERKNS_6StringE(emptyString));
    Require(_ZN3WTF14FileSystemImpl29makeSafeToUseMemoryMapForPathERKNS_6StringE(path));

    {
        const String archive = _ZN3WTF14FileSystemImpl25createTemporaryZipArchiveERKNS_6StringE(path);
        Require(archive.impl == nullptr);
        const String fromNull = _ZN3WTF14FileSystemImpl25createTemporaryZipArchiveERKNS_6StringE(nullString);
        Require(fromNull.impl == nullptr);
        const String uuid = _ZN3WTF21bootSessionUUIDStringEv();
        Require(uuid.impl == nullptr);
    }
    Require(pathImpl.refCount == 3);

    const char* search = _ZN3WTF21currentSearchLocaleIDEv();
    const char* textBreak = _ZN3WTF24currentTextBreakLocaleIDEv();
    Require(search != nullptr && std::strcmp(search, "C") == 0);
    Require(textBreak != nullptr && std::strcmp(textBreak, "C") == 0);

    for (const int priority : {0, -1, -15}) {
        _ZN3WTF6Thread31setCurrentThreadIsUserInitiatedEi(priority);
        _ZN3WTF6Thread33setCurrentThreadIsUserInteractiveEi(priority);
    }

    Require(NotImplementedCalls_nid_no_patch() == before);
    path.impl = nullptr;
    emptyString.impl = nullptr;
    return 0;
}
