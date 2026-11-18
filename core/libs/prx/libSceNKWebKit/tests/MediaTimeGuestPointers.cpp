#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "MediaTime guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static unsigned char* MapPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void ProtectReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

static bool Fields(const WTF::MediaTime& time, std::int64_t value, std::uint32_t scale, std::uint8_t flags) {
    return time.m_timeValue == value && time.m_timeScale == scale && time.m_timeFlags == flags;
}

int main() {
    auto* unreadable = reinterpret_cast<WTF::MediaTime*>(0x10);
    const auto rounding = WTF::MediaTime::RoundingFlags::HalfAwayFromZero;

    unsigned char* readOnlyPage = MapPage();
    auto* readOnly = reinterpret_cast<WTF::MediaTime*>(readOnlyPage);
    _ZN3WTF9MediaTimeC1Eljh(readOnly, 5, 10, WTF::MediaTime::Valid);
    unsigned char readOnlyBytes[sizeof(WTF::MediaTime)];
    std::memcpy(readOnlyBytes, readOnlyPage, sizeof(readOnlyBytes));
    ProtectReadOnly(readOnlyPage);
    auto unchanged = [&] { return std::memcmp(readOnlyPage, readOnlyBytes, sizeof(readOnlyBytes)) == 0; };

    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTimeC1Ev(unreadable); return 0; }, "_ZN3WTF9MediaTimeC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTimeC2Ev(readOnly); return 0; }, "_ZN3WTF9MediaTimeC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTimeC1Eljh(readOnly, 1, 2, WTF::MediaTime::Valid); return 0; }, "_ZN3WTF9MediaTimeC1Eljh"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTimeC2Eljh(unreadable, 1, 0, WTF::MediaTime::Valid); return 0; }, "_ZN3WTF9MediaTimeC1Eljh"));

    WTF::MediaTime host{7, 3, WTF::MediaTime::Valid};
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTimeC1ERKS0_(&host, *unreadable); return 0; }, "_ZN3WTF9MediaTimeC1ERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTimeC2ERKS0_(readOnly, host); return 0; }, "_ZN3WTF9MediaTimeC1ERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF9MediaTimeaSERKS0_(&host, *unreadable); }, "_ZN3WTF9MediaTimeaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF9MediaTimeaSERKS0_(readOnly, host); }, "_ZN3WTF9MediaTimeaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(readOnly, 20, rounding); return 0; },
        "_ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(unreadable, 20, rounding); return 0; },
        "_ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF9MediaTimepLERKS0_(readOnly, host); }, "_ZN3WTF9MediaTimepLERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF9MediaTimemIERKS0_(readOnly, host); }, "_ZN3WTF9MediaTimemIERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF9MediaTimepLERKS0_(&host, *unreadable); }, "_ZNK3WTF9MediaTimeplERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF9MediaTimemIERKS0_(unreadable, host); }, "_ZNK3WTF9MediaTimemiERKS0_"));
    Require(unchanged() && Fields(host, 7, 3, WTF::MediaTime::Valid));

    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(unreadable, 20, rounding); },
        "_ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime7compareERKS0_(unreadable, host); }, "_ZNK3WTF9MediaTime7compareERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime7compareERKS0_(&host, *unreadable); }, "_ZNK3WTF9MediaTime7compareERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimeplERKS0_(&host, *unreadable); }, "_ZNK3WTF9MediaTimeplERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimemiERKS0_(unreadable, host); }, "_ZNK3WTF9MediaTimemiERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimengEv(unreadable); }, "_ZNK3WTF9MediaTimengEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimemlEi(unreadable, 2); }, "_ZNK3WTF9MediaTimemlEi"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTimentEv(unreadable); }, "_ZNK3WTF9MediaTimentEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime12isIndefiniteEv(unreadable); }, "_ZNK3WTF9MediaTime12isIndefiniteEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime14hasBeenRoundedEv(unreadable); }, "_ZNK3WTF9MediaTime14hasBeenRoundedEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime14hasDoubleValueEv(unreadable); }, "_ZNK3WTF9MediaTime14hasDoubleValueEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime18isNegativeInfiniteEv(unreadable); }, "_ZNK3WTF9MediaTime18isNegativeInfiniteEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime18isPositiveInfiniteEv(unreadable); }, "_ZNK3WTF9MediaTime18isPositiveInfiniteEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime7isValidEv(unreadable); }, "_ZNK3WTF9MediaTime7isValidEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime9isInvalidEv(unreadable); }, "_ZNK3WTF9MediaTime9isInvalidEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime7toFloatEv(unreadable); }, "_ZNK3WTF9MediaTime7toFloatEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime8toDoubleEv(unreadable); }, "_ZNK3WTF9MediaTime8toDoubleEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF9MediaTime9timeFlagsEv(unreadable); }, "_ZNK3WTF9MediaTime9timeFlagsEv"));

    Require(Fields(_ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(readOnly, 20, rounding), 10, 20, WTF::MediaTime::Valid));
    Require(_ZNK3WTF9MediaTime7compareERKS0_(readOnly, *readOnly) == WTF::MediaTime::EqualTo);
    Require(Fields(_ZNK3WTF9MediaTimeplERKS0_(readOnly, *readOnly), 10, 10, WTF::MediaTime::Valid));
    Require(_ZNK3WTF9MediaTime8toDoubleEv(readOnly) == 0.5 && _ZNK3WTF9MediaTime7isValidEv(readOnly) && !_ZNK3WTF9MediaTimentEv(readOnly));
    _ZN3WTF9MediaTimeaSERKS0_(&host, *readOnly);
    Require(Fields(host, 5, 10, WTF::MediaTime::Valid) && unchanged());

    unsigned char* writablePage = MapPage();
    std::memset(writablePage, 0xcc, 4096);
    auto* writable = reinterpret_cast<WTF::MediaTime*>(writablePage);
    _ZN3WTF9MediaTimeC1Ev(writable);
    Require(Fields(*writable, 0, WTF::MediaTime::DefaultTimeScale, WTF::MediaTime::Valid));
    _ZN3WTF9MediaTimeC1Eljh(writable, -4, 0, WTF::MediaTime::Valid);
    Require(Fields(*writable, -1, 1, WTF::MediaTime::NegativeInfinite | WTF::MediaTime::Valid));
    _ZN3WTF9MediaTimeC1ERKS0_(writable, *readOnly);
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(writable, 3, rounding);
    Require(Fields(*writable, 2, 3, WTF::MediaTime::Valid | WTF::MediaTime::HasBeenRounded));
    _ZN3WTF9MediaTimepLERKS0_(writable, *readOnly);
    Require(Fields(*writable, 35, 30, WTF::MediaTime::Valid | WTF::MediaTime::HasBeenRounded));
    for (std::size_t i = 13; i < sizeof(WTF::MediaTime); ++i)
        Require(writablePage[i] == 0xcc);

    UnmapPage(writablePage);
    UnmapPage(readOnlyPage);
    return 0;
}
