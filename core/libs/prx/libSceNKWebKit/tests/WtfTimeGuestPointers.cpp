#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF time guest pointer check failed at line %d\n", line);
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

int main() {
    const double infinity = std::numeric_limits<double>::infinity();
    const WTF::MonotonicTime monotonic{infinity};
    const WTF::WallTime wall{-infinity};
    const WTF::TimeWithDynamicClockType wallClock{5.0, WTF::ClockType::Wall};
    const WTF::TimeWithDynamicClockType monotonicClock{infinity, WTF::ClockType::Monotonic};
    const WTF::TimeWithDynamicClockType approximateClock{7.0, WTF::ClockType::Approximate};
    const WTF::TimeWithDynamicClockType later{9.0, WTF::ClockType::Wall};
    const WTF::Seconds seconds{3.0};
    const WTF::CPUTime first{{2.0}, {0.5}, {0.25}};
    const WTF::CPUTime second{{4.0}, {1.0}, {0.75}};

    Require(_ZNK3WTF13MonotonicTime19approximateWallTimeEv(&monotonic).m_value == infinity);
    Require(_ZNK3WTF8WallTime24approximateMonotonicTimeEv(&wall).m_value == -infinity);
    Require(_ZNK3WTF24TimeWithDynamicClockType13monotonicTimeEv(&wallClock).m_value == 5.0);
    Require(_ZNK3WTF24TimeWithDynamicClockType8wallTimeEv(&approximateClock).m_value == 7.0);
    Require(_ZNK3WTF24TimeWithDynamicClockType19approximateWallTimeEv(&wallClock).m_value == 5.0);
    Require(_ZNK3WTF24TimeWithDynamicClockType19approximateWallTimeEv(&monotonicClock).m_value == infinity);
    Require(_ZNK3WTF24TimeWithDynamicClockType24approximateMonotonicTimeEv(&approximateClock).m_value == 7.0);
    Require(_ZNK3WTF24TimeWithDynamicClockType24approximateMonotonicTimeEv(&monotonicClock).m_value == infinity);
    Require(_ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(&approximateClock).m_type == WTF::ClockType::Approximate);
    Require(_ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(&later, wallClock).m_value == 4.0);
    Require(_ZNK3WTF24TimeWithDynamicClockTypegtERKS0_(&later, wallClock));
    Require(_ZNK3WTF24TimeWithDynamicClockTypegeERKS0_(&later, later));
    Require(!_ZNK3WTF24TimeWithDynamicClockTypeleERKS0_(&later, wallClock));
    Require(!_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(monotonicClock));
    Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(wallClock));
    _ZN3WTF5sleepERKNS_24TimeWithDynamicClockTypeE(wallClock);
    Require(_ZNK3WTF7SecondsmiENS_13MonotonicTimeE(&seconds, WTF::MonotonicTime{1.0}).m_value == 2.0);
    Require(_ZNK3WTF7SecondsmiENS_8WallTimeE(&seconds, WTF::WallTime{2.0}).m_value == 1.0);
    const auto difference = _ZNK3WTF7SecondsmiERKNS_24TimeWithDynamicClockTypeE(&seconds, approximateClock);
    Require(difference.m_value == -4.0 && difference.m_type == WTF::ClockType::Approximate);
    Require(_ZNK3WTF7SecondsplENS_13MonotonicTimeE(&seconds, WTF::MonotonicTime{1.0}).m_value == 4.0);
    Require(_ZNK3WTF7SecondsplENS_8WallTimeE(&seconds, WTF::WallTime{2.0}).m_value == 5.0);
    Require(_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(&second, first) == 50.0);
    Require(_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(&first, second) == 50.0);
    Require(std::isnan(_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(&first, first)));
    const WTF::CPUTime sameClockMoreWork{{2.0}, {1.5}, {0.25}};
    Require(_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(&sameClockMoreWork, first) == infinity);
    Require(_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(&first, sameClockMoreWork) == -infinity);
    for (const auto clock : {WTF::ClockType::Wall, WTF::ClockType::Monotonic, WTF::ClockType::Approximate}) {
        Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(WTF::TimeWithDynamicClockType{-infinity, clock}));
        Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(WTF::TimeWithDynamicClockType{std::numeric_limits<double>::quiet_NaN(), clock}));
        Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(WTF::TimeWithDynamicClockType{0.0, clock}));
        Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(WTF::TimeWithDynamicClockType{-0.0, clock}));
        Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(WTF::TimeWithDynamicClockType{-5.0, clock}));
        Require(!_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(WTF::TimeWithDynamicClockType{infinity, clock}));
    }

    WTF::GregorianDateTime date{};
    _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(&date);
    Require(date.m_year >= 2024 && date.m_month >= 0 && date.m_month <= 11 && date.m_monthDay >= 1 && date.m_monthDay <= 31);
    Require(date.m_yearDay >= 0 && date.m_yearDay <= 365 && date.m_weekDay >= 0 && date.m_weekDay <= 6 && date.m_hour >= 0 && date.m_hour <= 23);

    const auto* unreadableMonotonic = reinterpret_cast<const WTF::MonotonicTime*>(std::uintptr_t{0x10});
    const auto* unreadableWall = reinterpret_cast<const WTF::WallTime*>(std::uintptr_t{0x10});
    const auto* unreadableClock = reinterpret_cast<const WTF::TimeWithDynamicClockType*>(std::uintptr_t{0x10});
    const auto* unreadableSeconds = reinterpret_cast<const WTF::Seconds*>(std::uintptr_t{0x10});
    const auto* unreadableCpu = reinterpret_cast<const WTF::CPUTime*>(std::uintptr_t{0x10});
    auto* unreadableScope = reinterpret_cast<WTF::TimingScope*>(std::uintptr_t{0x10});
    auto* unwritableDate = reinterpret_cast<WTF::GregorianDateTime*>(std::uintptr_t{0x10});

    Require(FaultsAsNotImplemented([&] { _ZN3WTF11TimingScope11scopeDidEndEv(unreadableScope); return 0; }, "_ZN3WTF11TimingScope11scopeDidEndEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF13MonotonicTime19approximateWallTimeEv(unreadableMonotonic); }, "_ZNK3WTF13MonotonicTime19approximateWallTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF8WallTime24approximateMonotonicTimeEv(unreadableWall); }, "_ZNK3WTF8WallTime24approximateMonotonicTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockType13monotonicTimeEv(unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockType13monotonicTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockType8wallTimeEv(unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockType8wallTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockType19approximateWallTimeEv(unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockType19approximateWallTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockType24approximateMonotonicTimeEv(unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockType24approximateMonotonicTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(unreadableClock, wallClock); }, "_ZNK3WTF24TimeWithDynamicClockTypemiERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(&wallClock, *unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockTypemiERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypegtERKS0_(unreadableClock, wallClock); }, "_ZNK3WTF24TimeWithDynamicClockTypegtERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypegeERKS0_(&wallClock, *unreadableClock); }, "_ZNK3WTF24TimeWithDynamicClockTypegeERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF24TimeWithDynamicClockTypeleERKS0_(unreadableClock, wallClock); }, "_ZNK3WTF24TimeWithDynamicClockTypeleERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(*unreadableClock); }, "_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF5sleepERKNS_24TimeWithDynamicClockTypeE(*unreadableClock); return 0; }, "_ZN3WTF5sleepERKNS_24TimeWithDynamicClockTypeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsmiENS_13MonotonicTimeE(unreadableSeconds, WTF::MonotonicTime{1.0}); }, "_ZNK3WTF7SecondsmiENS_13MonotonicTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsmiENS_8WallTimeE(unreadableSeconds, WTF::WallTime{1.0}); }, "_ZNK3WTF7SecondsmiENS_8WallTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsmiERKNS_24TimeWithDynamicClockTypeE(&seconds, *unreadableClock); }, "_ZNK3WTF7SecondsmiERKNS_24TimeWithDynamicClockTypeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsplENS_13MonotonicTimeE(unreadableSeconds, WTF::MonotonicTime{1.0}); }, "_ZNK3WTF7SecondsplENS_13MonotonicTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7SecondsplENS_8WallTimeE(unreadableSeconds, WTF::WallTime{1.0}); }, "_ZNK3WTF7SecondsplENS_8WallTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(unreadableCpu, first); }, "_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(&first, *unreadableCpu); }, "_ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(unwritableDate); return 0; }, "_ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv"));

    unsigned char* readOnlyPage = MapPage();
    std::memset(readOnlyPage, 0x5a, 4096);
    ProtectReadOnly(readOnlyPage);
    auto* readOnlyDate = reinterpret_cast<WTF::GregorianDateTime*>(readOnlyPage);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(readOnlyDate); return 0; }, "_ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv"));
    for (std::size_t index = 0; index < sizeof(WTF::GregorianDateTime); ++index) Require(readOnlyPage[index] == 0x5a);
    UnmapPage(readOnlyPage);
    return 0;
}
