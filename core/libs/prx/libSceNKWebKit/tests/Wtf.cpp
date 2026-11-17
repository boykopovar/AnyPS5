#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceIcu/Version.hpp"

#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <thread>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/WtfStringAbi.hpp"
#include "Testing.hpp"
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <source_location>
#include <type_traits>

namespace {

template <typename Function, typename... Args>
void* TryResult(Function function, Args... args) {
    WTF::TryMallocReturnValue result{reinterpret_cast<void*>(std::uintptr_t{0xDEADBEEF})};
    WTF::TryMallocReturnValue* returned = function(&result, args...);
    Require(returned == &result);
    return result.data;
}

using WTF::ClockType;
using WTF::TimeType;
using WTF::Seconds;
using WTF::MonotonicTime;
using WTF::WallTime;
using WTF::TimeWithDynamicClockType;
using WTF::CPUTime;
using WTF::GregorianDateTime;
using WTF::LocalTimeOffset;
using WebCore::FloatPoint;
using WebCore::FloatSize;
using WebCore::FloatRect;
using WebCore::IntPoint;
using WebCore::IntSize;
using WebCore::IntRect;

}  // namespace

static void TestAllocationBoundaries() {
    constexpr std::size_t maxSize = std::numeric_limits<std::size_t>::max();
    Require(TryResult(_ZN3WTF13tryFastMallocEm, maxSize) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastMallocEm, maxSize - 8) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastMallocEm, maxSize - 64) == nullptr);
    Require(TryResult(_ZN3WTF19tryFastZeroedMallocEm, maxSize) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastCallocEmm, 1, maxSize) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastCallocEmm, maxSize, 2) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastCallocEmm, maxSize / 2 + 1, 2) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(64, maxSize) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(std::size_t{1} << 62, 64) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(maxSize, 64) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(48, 64) == nullptr);

    void* p = _ZN3WTF10fastMallocEm(32);
    Require(p != nullptr);
    std::memset(p, 0x5A, 32);
    Require(TryResult(_ZN3WTF14tryFastReallocEPvm, p, maxSize) == nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(p) == 32);
    Require(static_cast<unsigned char*>(p)[31] == 0x5A);
    _ZN3WTF8fastFreeEPv(p);

    void* z = TryResult(_ZN3WTF13tryFastCallocEmm, maxSize / 8, 0);
    if (z) {
        _ZN3WTF8fastFreeEPv(z);
    }
    void* zero = TryResult(_ZN3WTF13tryFastCallocEmm, 0, maxSize);
    if (zero) {
        _ZN3WTF8fastFreeEPv(zero);
    }

    void* m = TryResult(_ZN3WTF13tryFastMallocEm, std::size_t{48});
    Require(m != nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(m) == 48);
    std::memset(m, 0x3C, 48);
    void* r = TryResult(_ZN3WTF14tryFastReallocEPvm, m, std::size_t{96});
    Require(r != nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(r) == 96);
    for (int i = 0; i < 48; ++i) {
        Require(static_cast<unsigned char*>(r)[i] == 0x3C);
    }
    _ZN3WTF8fastFreeEPv(r);

    auto* zm = static_cast<unsigned char*>(TryResult(_ZN3WTF19tryFastZeroedMallocEm, std::size_t{40}));
    Require(zm != nullptr);
    for (int i = 0; i < 40; ++i) {
        Require(zm[i] == 0);
    }
    _ZN3WTF8fastFreeEPv(zm);

    auto* cm = static_cast<unsigned char*>(TryResult(_ZN3WTF13tryFastCallocEmm, std::size_t{6}, std::size_t{8}));
    Require(cm != nullptr);
    for (int i = 0; i < 48; ++i) {
        Require(cm[i] == 0);
    }
    _ZN3WTF8fastFreeEPv(cm);
}

static void TestMemoryAllocations() {
    void* p1 = _ZN3WTF10fastMallocEm(128);
    Require(p1 != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(p1) & 0x0F) == 0);
    std::memset(p1, 0xAB, 128);
    Require(_ZN3WTF14fastMallocSizeEPKv(p1) >= 128);
    Require(_ZN3WTF18fastMallocGoodSizeEm(128) >= 128);

    void* p2 = _ZN3WTF11fastReallocEPvm(p1, 256);
    Require(p2 != nullptr);
    auto* b2 = static_cast<unsigned char*>(p2);
    for (int i = 0; i < 128; ++i) {
        Require(b2[i] == 0xAB);
    }
    _ZN3WTF8fastFreeEPv(p2);

    void* pZero = _ZN3WTF16fastZeroedMallocEm(64);
    Require(pZero != nullptr);
    auto* bZero = static_cast<unsigned char*>(pZero);
    for (int i = 0; i < 64; ++i) {
        Require(bZero[i] == 0);
    }
    _ZN3WTF8fastFreeEPv(pZero);

    void* pCalloc = _ZN3WTF10fastCallocEmm(10, 8);
    Require(pCalloc != nullptr);
    _ZN3WTF8fastFreeEPv(pCalloc);

    void* pAligned = _ZN3WTF17fastAlignedMallocEmm(64, 128);
    Require(pAligned != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(pAligned) & 63) == 0);
    _ZN3WTF15fastAlignedFreeEPv(pAligned);

    char* dup = _ZN3WTF10fastStrDupEPKc("hello wtf");
    Require(dup != nullptr);
    Require(std::strcmp(dup, "hello wtf") == 0);
    _ZN3WTF8fastFreeEPv(dup);

    Require(_ZN3WTF19isFastMallocEnabledEv() == true);
    _ZN3WTF27releaseFastMallocFreeMemoryEv();
    _ZN3WTF40releaseFastMallocFreeMemoryForThisThreadEv();
    _ZN3WTF18fastEnableMiniModeEv();
    _ZN3WTF20fastDisableScavengerEv();
    FastMallocStatistics stats = _ZN3WTF20fastMallocStatisticsEv();
    static_cast<void>(stats);
    _ZN3WTF25fastMallocDumpMallocStatsEv();

    void* jsVal = _ZN3WTF13jsValueMallocEm(32);
    Require(jsVal != nullptr);
    _ZN3WTF11jsValueFreeEPv(jsVal);

    void* bm = _ZN7bmalloc3api15mallocOutOfLineEmNS_8HeapKindE(96, 0);
    Require(bm != nullptr);
    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(bm, 0);
    Require(_ZN7bmalloc3api9isEnabledENS_8HeapKindE(0) == true);
    Require(_ZN7bmalloc15availableMemoryEv() > 0);
    Require(_ZN7bmalloc16isActiveHeapKindENS_8HeapKindE(0) == true);
    Require(_ZN7bmalloc16isActiveHeapKindENS_8HeapKindE(1) == false);
    Require(_ZN7bmalloc19mapToActiveHeapKindENS_8HeapKindE(2) == 0);
    _ZN7bmalloc3api8scavengeEv();
    _ZN7bmalloc3api14enableMiniModeEv();
    _ZN7bmalloc3api16disableScavengerEv();

    void* gc = _ZN8Gigacage6mallocENS_4KindEm(0, 112);
    Require(gc != nullptr);
    _ZN8Gigacage4freeENS_4KindEPv(0, gc);

    void* gcArr = _ZN8Gigacage11mallocArrayENS_4KindEmm(0, 5, 20);
    Require(gcArr != nullptr);
    void* gcRealloc = _ZN8Gigacage10tryReallocENS_4KindEPvm(0, gcArr, 200);
    Require(gcRealloc != nullptr);
    _ZN8Gigacage4freeENS_4KindEPv(0, gcRealloc);

    void* gcAligned = _ZN8Gigacage16tryAlignedMallocENS_4KindEmm(0, 128, 64);
    Require(gcAligned != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(gcAligned) & 127) == 0);
    _ZN8Gigacage11alignedFreeENS_4KindEPv(0, gcAligned);

    void* cross = _ZN3WTF10fastMallocEm(50);
    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(cross, 0);

    void* cross2 = _ZN7bmalloc3api15mallocOutOfLineEmNS_8HeapKindE(60, 0);
    _ZN8Gigacage4freeENS_4KindEPv(0, cross2);
}

template <typename Function>
bool ExpectBmallocOperatorStub(Function function, const char* name) {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == std::string(name) + " not implemented" &&
            NotImplementedCalls_nid_no_patch() == before + 1) {
            std::fprintf(stderr, "EXPECTED_BMALLOC_THROW %s\n", name);
            return true;
        }
    }
    return false;
}

template <typename ScalarNew, typename ScalarDelete, typename ArrayNew>
bool TestBmallocOperatorClass(ScalarNew scalarNew, ScalarDelete scalarDelete, ArrayNew arrayNew,
                              const char* scalarNewName, const char* scalarDeleteName,
                              const char* arrayNewName) {
    if constexpr (std::is_invocable_r_v<int, ScalarNew> &&
                  std::is_invocable_r_v<int, ScalarDelete> &&
                  std::is_invocable_r_v<int, ArrayNew>) {
        ExpectBmallocOperatorStub(scalarNew, scalarNewName);
        ExpectBmallocOperatorStub(scalarDelete, scalarDeleteName);
        ExpectBmallocOperatorStub(arrayNew, arrayNewName);
        return false;
    } else {
        static_assert(std::is_invocable_r_v<void*, ScalarNew, std::size_t>);
        static_assert(std::is_invocable_r_v<void, ScalarDelete, void*>);
        static_assert(std::is_invocable_r_v<void*, ArrayNew, std::size_t>);

        void* scalarZero = scalarNew(0);
        Require(scalarZero != nullptr);
        Require((reinterpret_cast<std::uintptr_t>(scalarZero) % alignof(std::max_align_t)) == 0);
        scalarDelete(scalarZero);
        scalarDelete(nullptr);

        auto* scalar = static_cast<unsigned char*>(scalarNew(128));
        Require(scalar != nullptr);
        for (std::size_t i = 0; i < 128; ++i) {
            scalar[i] = static_cast<unsigned char>(i ^ 0xA5);
        }
        for (std::size_t i = 0; i < 128; ++i) {
            Require(scalar[i] == static_cast<unsigned char>(i ^ 0xA5));
        }
        scalarDelete(scalar);

        void* arrayZero = arrayNew(0);
        Require(arrayZero != nullptr);
        Require((reinterpret_cast<std::uintptr_t>(arrayZero) % alignof(std::max_align_t)) == 0);
        _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(arrayZero, 0);

        auto* array = static_cast<unsigned char*>(arrayNew(128));
        Require(array != nullptr);
        for (std::size_t i = 0; i < 128; ++i) {
            array[i] = static_cast<unsigned char>(i ^ 0x5A);
        }
        for (std::size_t i = 0; i < 128; ++i) {
            Require(array[i] == static_cast<unsigned char>(i ^ 0x5A));
        }
        _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(array, 0);
        return true;
    }
}

static bool TestBmallocHeapOperators() {
    const bool isoTls = TestBmallocOperatorClass(
        _ZN7bmalloc11IsoTLSEntrynwEm, _ZN7bmalloc11IsoTLSEntrydlEPv,
        _ZN7bmalloc11IsoTLSEntrynaEm, "_ZN7bmalloc11IsoTLSEntrynwEm",
        "_ZN7bmalloc11IsoTLSEntrydlEPv", "_ZN7bmalloc11IsoTLSEntrynaEm");
    const bool isoHeap = TestBmallocOperatorClass(
        _ZN7bmalloc15IsoHeapImplBasenwEm, _ZN7bmalloc15IsoHeapImplBasedlEPv,
        _ZN7bmalloc15IsoHeapImplBasenaEm, "_ZN7bmalloc15IsoHeapImplBasenwEm",
        "_ZN7bmalloc15IsoHeapImplBasedlEPv", "_ZN7bmalloc15IsoHeapImplBasenaEm");
    return isoTls && isoHeap;
}

static void TestTime() {
    MonotonicTime mNow = _ZN3WTF13MonotonicTime3nowEv();
    Require(mNow.m_value > 0.0);
    WallTime wNow = _ZN3WTF8WallTime3nowEv();
    Require(wNow.m_value > 1000000000.0);

    WallTime approxW = _ZNK3WTF13MonotonicTime19approximateWallTimeEv(&mNow);
    Require(std::abs(approxW.m_value - wNow.m_value) < 5.0);

    MonotonicTime approxM = _ZNK3WTF8WallTime24approximateMonotonicTimeEv(&wNow);
    Require(std::abs(approxM.m_value - mNow.m_value) < 5.0);

    TimeWithDynamicClockType dynWall = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(ClockType::Wall);
    Require(dynWall.m_type == ClockType::Wall);
    Require(dynWall.m_value > 0.0);

    TimeWithDynamicClockType dynMono = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(ClockType::Monotonic);
    Require(dynMono.m_type == ClockType::Monotonic);

    TimeWithDynamicClockType dynMono2 = dynMono;
    dynMono2.m_value += 1.0;
    Require(_ZNK3WTF24TimeWithDynamicClockTypegtERKS0_(&dynMono2, dynMono));
    Require(_ZNK3WTF24TimeWithDynamicClockTypegeERKS0_(&dynMono2, dynMono));
    Require(_ZNK3WTF24TimeWithDynamicClockTypeleERKS0_(&dynMono, dynMono2));

    Seconds diff = _ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(&dynMono2, dynMono);
    Require(std::abs(diff.m_value - 1.0) < 1e-6);

    Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(dynMono) == true);
    TimeWithDynamicClockType farFuture = dynMono;
    farFuture.m_value += 1000.0;
    Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(farFuture) == false);

    Seconds sec{10.0};
    MonotonicTime mSub = _ZNK3WTF7SecondsmiENS_13MonotonicTimeE(&sec, MonotonicTime{4.0});
    Require(std::abs(mSub.m_value - 6.0) < 1e-6);
    MonotonicTime mAdd = _ZNK3WTF7SecondsplENS_13MonotonicTimeE(&sec, MonotonicTime{4.0});
    Require(std::abs(mAdd.m_value - 14.0) < 1e-6);

    WallTime wSub = _ZNK3WTF7SecondsmiENS_8WallTimeE(&sec, WallTime{3.0});
    Require(std::abs(wSub.m_value - 7.0) < 1e-6);
    WallTime wAdd = _ZNK3WTF7SecondsplENS_8WallTimeE(&sec, WallTime{3.0});
    Require(std::abs(wAdd.m_value - 13.0) < 1e-6);

    GregorianDateTime gdt{};
    _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(&gdt);
    Require(gdt.m_year >= 2024);
    Require(gdt.m_month >= 0 && gdt.m_month <= 11);
    Require(gdt.m_monthDay >= 1 && gdt.m_monthDay <= 31);

    LocalTimeOffset lto = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(0.0, TimeType::UTCTime);
    Require(lto.offset >= -86400000 && lto.offset <= 86400000);

    bool isLocal = false;
    double parsed = _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb("2024-05-10T12:00:00Z", isLocal);
    Require(!std::isnan(parsed));
    Require(parsed > 1700000000000.0);
    Require(isLocal == false);

    bool isLocal2 = false;
    double parsedInvalid = _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb("not-a-date", isLocal2);
    Require(std::isnan(parsedInvalid));

    Seconds threadCpu = _ZN3WTF7CPUTime16forCurrentThreadEv();
    Require(threadCpu.m_value >= 0.0);
}

extern "C" void CurrentTimeClockForTest_nid_no_patch(int (*query)(int, KernelTimespec*));

static int g_clockQueries = 0;
static int g_lastClockId = -1;

static int ClockWithTimes(int clockId, KernelTimespec* time) {
    ++g_clockQueries;
    g_lastClockId = clockId;
    if (clockId == 0) {
        *time = KernelTimespec{1700000000, 250000000};
        return 0;
    }
    if (clockId == 4) {
        *time = KernelTimespec{12345, 500000000};
        return 0;
    }
    *time = KernelTimespec{99, 0};
    return 0;
}

static int ClockFailing(int, KernelTimespec*) {
    ++g_clockQueries;
    return -1;
}

static double GuestClockSeconds(int clockId) {
    KernelTimespec time{};
    Require(clock_gettime_nid_postfix(clockId, &time) == 0);
    return static_cast<double>(time.tv_sec) + time.tv_nsec / 1'000'000'000.0;
}

static void TestGuestClocks() {
    CurrentTimeClockForTest_nid_no_patch(ClockWithTimes);
    g_clockQueries = 0;
    Require(_ZN3WTF8WallTime3nowEv().m_value == 1700000000.25);
    Require(g_lastClockId == 0);
    Require(_ZN3WTF13MonotonicTime3nowEv().m_value == 12345.5);
    Require(g_lastClockId == 4);
    Require(g_clockQueries == 2);

    TimeWithDynamicClockType dynamicWall = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(ClockType::Wall);
    Require(dynamicWall.m_type == ClockType::Wall && dynamicWall.m_value == 1700000000.25);
    TimeWithDynamicClockType dynamicMonotonic = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(ClockType::Monotonic);
    Require(dynamicMonotonic.m_type == ClockType::Monotonic && dynamicMonotonic.m_value == 12345.5);

    MonotonicTime monotonic{12346.5};
    Require(_ZNK3WTF13MonotonicTime19approximateWallTimeEv(&monotonic).m_value == 1700000001.25);
    WallTime wall{1700000002.25};
    Require(_ZNK3WTF8WallTime24approximateMonotonicTimeEv(&wall).m_value == 12347.5);

    CurrentTimeClockForTest_nid_no_patch(ClockFailing);
    g_clockQueries = 0;
    Require(_ZN3WTF8WallTime3nowEv().m_value == 0.0);
    Require(_ZN3WTF13MonotonicTime3nowEv().m_value == 0.0);
    Require(g_clockQueries == 2);

    CurrentTimeClockForTest_nid_no_patch(nullptr);
    const double monotonicBefore = GuestClockSeconds(4);
    const double monotonicNow = _ZN3WTF13MonotonicTime3nowEv().m_value;
    Require(monotonicBefore <= monotonicNow && monotonicNow <= GuestClockSeconds(4));
    const double wallBefore = GuestClockSeconds(0);
    const double wallNow = _ZN3WTF8WallTime3nowEv().m_value;
    Require(wallBefore <= wallNow && wallNow <= GuestClockSeconds(0));
}

extern "C" void NumberOfProcessorCoresSysconfForTest_nid_no_patch(std::int64_t (*query)(int));

static int g_sysconfQueries = 0;

static std::int64_t SysconfWithProcessors(int name) {
    ++g_sysconfQueries;
    if (name == 58) return 37;
    if (name == 57) return 41;
    return -1;
}

static std::int64_t SysconfFailing(int) {
    ++g_sysconfQueries;
    return -1;
}

static void TestNumberOfProcessorCores() {
    NumberOfProcessorCoresSysconfForTest_nid_no_patch(SysconfWithProcessors);
    g_sysconfQueries = 0;
    Require(_ZN3WTF22numberOfProcessorCoresEv() == 37);
    Require(_ZN3WTF22numberOfProcessorCoresEv() == 37);
    Require(g_sysconfQueries == 1);

    NumberOfProcessorCoresSysconfForTest_nid_no_patch(SysconfFailing);
    g_sysconfQueries = 0;
    Require(_ZN3WTF22numberOfProcessorCoresEv() == 1);
    Require(_ZN3WTF22numberOfProcessorCoresEv() == 1);
    Require(g_sysconfQueries == 1);

    NumberOfProcessorCoresSysconfForTest_nid_no_patch(nullptr);
    const int cores = _ZN3WTF22numberOfProcessorCoresEv();
    Require(cores >= 1);
    Require(cores == sysconf_nid_postfix(58));
    Require(_ZN3WTF22numberOfProcessorCoresEv() == cores);
}

extern "C" void RamSizeSysconfForTest_nid_no_patch(std::int64_t (*query)(int));

static std::int64_t SysconfWithMemory(int name) {
    ++g_sysconfQueries;
    if (name == 121) return 139888;
    if (name == 47) return 16384;
    return -1;
}

static std::int64_t SysconfWithoutPhysPages(int name) {
    ++g_sysconfQueries;
    return name == 47 ? 16384 : -1;
}

static std::int64_t SysconfWithoutPageSize(int name) {
    ++g_sysconfQueries;
    return name == 121 ? 139888 : -1;
}

static void TestRamSize() {
    RamSizeSysconfForTest_nid_no_patch(SysconfWithMemory);
    g_sysconfQueries = 0;
    Require(_ZN3WTF7ramSizeEv() == 139888ULL * 16384ULL);
    Require(_ZN3WTF7ramSizeEv() == 139888ULL * 16384ULL);
    Require(g_sysconfQueries == 2);

    RamSizeSysconfForTest_nid_no_patch(SysconfWithoutPhysPages);
    g_sysconfQueries = 0;
    Require(_ZN3WTF7ramSizeEv() == 512ULL * 1024ULL * 1024ULL);
    Require(_ZN3WTF7ramSizeEv() == 512ULL * 1024ULL * 1024ULL);
    Require(g_sysconfQueries == 2);

    RamSizeSysconfForTest_nid_no_patch(SysconfWithoutPageSize);
    Require(_ZN3WTF7ramSizeEv() == 512ULL * 1024ULL * 1024ULL);

    RamSizeSysconfForTest_nid_no_patch(SysconfFailing);
    Require(_ZN3WTF7ramSizeEv() == 512ULL * 1024ULL * 1024ULL);

    RamSizeSysconfForTest_nid_no_patch(nullptr);
    const std::uint64_t ram = _ZN3WTF7ramSizeEv();
    Require(ram == static_cast<std::uint64_t>(sysconf_nid_postfix(121)) * static_cast<std::uint64_t>(sysconf_nid_postfix(47)));
    Require(ram >= 1024ULL * 1024ULL * 1024ULL);
    Require(_ZN3WTF7ramSizeEv() == ram);
}

static void TestSystem() {
    TestNumberOfProcessorCores();
    TestRamSize();
    TestGuestClocks();

    std::size_t page = _ZN3WTF8pageSizeEv();
    Require(page == 16384);
    Require(_ZN3WTF8pageSizeEv() == page);

    _ZN3WTF6Config17permanentlyFreezeEv();

    std::uint8_t icuVersion[4] = {0};
    u_getVersion_nid_postfix(icuVersion);
    Require(_ZN3WTF3ICU12majorVersionEv() == icuVersion[0]);
    Require(_ZN3WTF3ICU12minorVersionEv() == icuVersion[1]);
    Require(_ZN3WTF3ICU12majorVersionEv() == 78);
    Require(_ZN3WTF3ICU12minorVersionEv() == 1);

    char b1[16] = {0};
    char b2[16] = {0};
    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(b1, b2, 16) == 0);
    b2[5] = 1;
    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(b1, b2, 16) != 0);

    std::uint8_t randBytes[32] = {0};
    _ZN3WTF29cryptographicallyRandomValuesEPvm(randBytes, sizeof(randBytes));
    bool hasNonZero = false;
    for (std::size_t i = 0; i < sizeof(randBytes); ++i) {
        if (randBytes[i] != 0) {
            hasNonZero = true;
            break;
        }
    }
    Require(hasNonZero);
}

static void TestString() {
    const char* ascii = "hello world";
    unsigned int dataLen = 0;
    unsigned int u16Len = 0;
    unsigned int hash = _ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(
        ascii, ascii + std::strlen(ascii), dataLen, u16Len);
    Require(hash != 0);
    Require(dataLen == std::strlen(ascii));
    Require(u16Len == std::strlen(ascii));
    Require((hash & 0xFF000000U) == 0);

    const char* utf8 = "caf\xc3\xa9";
    unsigned int hash2 = _ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(
        utf8, utf8 + std::strlen(utf8), dataLen, u16Len);
    Require(hash2 != 0);
    Require(dataLen == 5);
    Require(u16Len == 4);

    const char16_t numStr[] = u"123.456 extra";
    std::size_t parsedLen = 0;
    double d = _ZN3WTF8Internal25parseDoubleFromLongStringEPKDsmRm(numStr, sizeof(numStr) / sizeof(char16_t) - 1, parsedLen);
    Require(std::abs(d - 123.456) < 1e-6);
    Require(parsedLen == 7);
}

static void TestWebCorePure() {
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(200) == true);
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(404) == true);
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(403) == false);
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(500) == false);

    Require(_ZN7WebCore32isStatusCodePotentiallyCacheableEi(302) == true);
    Require(_ZN7WebCore32isStatusCodePotentiallyCacheableEi(403) == true);
    Require(_ZN7WebCore32isStatusCodePotentiallyCacheableEi(500) == false);

    Require(_ZN7WebCore43initializeMaximumHTTPConnectionCountPerHostEv() == 4);
    _ZN7WebCore27setPresentingApplicationPIDEi(1234);

    FloatRect container{FloatPoint{0.0f, 0.0f}, FloatSize{200.0f, 100.0f}};
    FloatRect inside = _ZN7WebCore36largestRectWithAspectRatioInsideRectEfRKNS_9FloatRectE(1.0f, container);
    Require(std::abs(inside.m_size.m_width - 100.0f) < 1e-3f);
    Require(std::abs(inside.m_size.m_height - 100.0f) < 1e-3f);
    Require(std::abs(inside.m_location.m_x - 50.0f) < 1e-3f);

    FloatRect around = _ZN7WebCore37smallestRectWithAspectRatioAroundRectEfRKNS_9FloatRectE(1.0f, container);
    Require(std::abs(around.m_size.m_width - 200.0f) < 1e-3f);
    Require(std::abs(around.m_size.m_height - 200.0f) < 1e-3f);

    FloatRect fr{FloatPoint{10.2f, 20.8f}, FloatSize{100.4f, 50.6f}};
    IntRect ir = _ZN7WebCore14roundedIntRectERKNS_9FloatRectE(fr);
    Require(ir.m_location.m_x == 10);
    Require(ir.m_location.m_y == 21);
    Require(ir.m_size.m_width == 100);
    Require(ir.m_size.m_height == 51);
}

static void TestJscAndPal() {
    double arr[4] = {1.0, 2.0, 3.0, 4.0};
    _ZN3JSC16clearArrayMemsetEPdj(arr, 4);
    for (int i = 0; i < 4; ++i) {
        Require(std::isnan(arr[i]));
    }

    _ZN3JSC18enableSuperSamplerEv();
    _ZN3JSC19disableSuperSamplerEv();
    _ZN3JSC22printSuperSamplerStateEv();

    Require(_ZN3JSC25numberOfOSRExitFuzzChecksEv() == 0);
    Require(_ZN3JSC31numberOfStaticOSRExitFuzzChecksEv() == 0);
    Require(_ZN3JSC27numberOfExceptionFuzzChecksEv() == 0);
    Require(_ZN3JSC38numberOfExecutableAllocationFuzzChecksEv() == 0);

    _ZN3JSC27finalizeStatsAtEndOfTestingEv();
    _ZN3JSC20waitForVMDestructionEv();
    _ZN3JSC30waitForAsynchronousDisassemblyEv();
    _ZN3JSC18logTotalPhaseTimesEv();

    _ZN3PAL10systemBeepEv();

    void* stack[8] = {nullptr};
    int frames = 0;
    WTFGetBacktrace(stack, &frames);
    Require(frames >= 0);
    Require(WTFIsDebuggerAttached() == false);
}

void TestAdditionalWtf();

static void TestMappedFiles() {
    const auto open = &_ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb;
    const auto close = &_ZN3WTF14FileSystemImpl9closeFileERi;
    const auto construct = &_ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb;
    const auto readOnly = &_ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_14MappedFileModeERb;
    const auto fromPath = &_ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb;
    const auto fromPathBase = &_ZN3WTF14FileSystemImpl14MappedFileDataC2ERKNS_6StringENS0_14MappedFileModeERb;
    const auto destroy = &_ZN3WTF14FileSystemImpl14MappedFileDataD1Ev;
    const auto destroyBase = &_ZN3WTF14FileSystemImpl14MappedFileDataD2Ev;
    const auto map = &_ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE;
    WTF::MappedFileData mapped{};
    bool success = true;
    readOnly(&mapped, -1, 0, success);
    Require(!success && !mapped.data && !mapped.size);
    destroy(&mapped);
    const auto root = std::filesystem::current_path() / ("nkwebkit-mapped-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-mapped", reinterpret_cast<const char*>(hostPath.c_str()));
    auto path = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>("/nkwebkit-mapped/file.bin"));
    fromPath(&mapped, path, 0, success);
    Require(!success && !mapped.data && !mapped.size);
    int handle = open(path, 2, true, true);
    Require(handle >= 0);
    readOnly(&mapped, handle, 0, success);
    Require(success && !mapped.data && !mapped.size);
    destroyBase(&mapped);
    Require(sceKernelWrite(handle, "abcdef", 6) == 6);
    readOnly(&mapped, handle, 0, success);
    Require(success && mapped.size == 6 && std::memcmp(mapped.data, "abcdef", 6) == 0);
    VirtualQueryInfo info{};
    void* address = mapped.data;
    Require(sceKernelVirtualQuery(address, 0, &info, sizeof(info)) == 0);
    close(handle);
    Require(std::memcmp(mapped.data, "abcdef", 6) == 0);
    destroy(&mapped);
    Require(sceKernelVirtualQuery(address, 0, &info, sizeof(info)) != 0);
    handle = open(path, 2, true, false);
    construct(&mapped, handle, 2, 0, success);
    Require(success && mapped.size == 6);
    static_cast<char*>(mapped.data)[0] = 'S';
    char contents[6]{};
    Require(sceKernelRead(handle, contents, sizeof(contents)) == 6 && contents[0] == 'S');
    destroy(&mapped);
    construct(&mapped, handle, 2, 1, success);
    Require(success && mapped.size == 6);
    static_cast<char*>(mapped.data)[0] = 'P';
    Require(sceKernelLseek(handle, 0, 0) == 0);
    Require(sceKernelRead(handle, contents, sizeof(contents)) == 6 && contents[0] == 'S');
    destroyBase(&mapped);
    construct(&mapped, handle, 1, 0, success);
    Require(success);
    static_cast<char*>(mapped.data)[1] = 'W';
    destroy(&mapped);
    close(handle);
    fromPathBase(&mapped, path, 1, success);
    Require(success && mapped.size == 6 && std::memcmp(mapped.data, "SWcdef", 6) == 0);
    destroyBase(&mapped);
    handle = open(path, 0, true, false);
    construct(&mapped, handle, 2, 0, success);
    Require(!success && !mapped.data && !mapped.size);
    Require(map(&mapped, handle, 0, 1));
    Require(mapped.size == 6 && std::memcmp(mapped.data, "SWcdef", 6) == 0);
    destroy(&mapped);
    close(handle);
#ifndef _WIN32
    std::filesystem::resize_file(root / "file.bin", std::uint64_t{1} << 32);
    handle = open(path, 0, true, false);
    readOnly(&mapped, handle, 0, success);
    Require(!success && !mapped.data && !mapped.size);
    close(handle);
#endif
    RemovePathAlias_nid_no_patch("/nkwebkit-mapped");
    std::filesystem::remove_all(root);
}

static void TestFileHandles() {
    const auto open = &_ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb;
    const auto close = &_ZN3WTF14FileSystemImpl9closeFileERi;
    const auto seek = &_ZN3WTF14FileSystemImpl8seekFileEixNS0_14FileSeekOriginE;
    const auto truncate = &_ZN3WTF14FileSystemImpl12truncateFileEix;
    const auto openAndLock = &_ZN3WTF14FileSystemImpl15openAndLockFileERKNS_6StringENS0_12FileOpenModeENS_9OptionSetINS0_12FileLockModeEEE;
    const auto unlockAndClose = &_ZN3WTF14FileSystemImpl18unlockAndCloseFileEi;
    const auto root = std::filesystem::current_path() / ("nkwebkit-handles-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-handles", reinterpret_cast<const char*>(hostPath.c_str()));
    const char* guestPath = "/nkwebkit-handles/\xC3\xA9.bin";
    auto path = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(guestPath));
    Require(open(path, 0, true, false) == -1);
    Require(openAndLock(path, 0, 1) == -1);
    NKWebKit::WtfAbi::String nullPath;
    Require(open(nullPath, 0, true, false) == -1);
#ifndef _WIN32
    const auto oldMask = ::umask(0);
#endif
    int handle = open(path, 1, false, false);
#ifndef _WIN32
    ::umask(oldMask);
#endif
    Require(handle >= 0);
    Require(sceKernelWrite(handle, "abcdef", 6) == 6);
    Require(seek(handle, 2, 0) == 2);
    Require(seek(handle, 1, 1) == 3);
    Require(seek(handle, -1, 2) == 5);
    Require(truncate(handle, 4));
    Require(seek(handle, 0, 2) == 4);
    Require(!truncate(handle, -1));
    close(handle);
    Require(handle == -1);
    close(handle);
    Require(handle == -1);
#ifndef _WIN32
    Require((std::filesystem::status(root / std::filesystem::path(u8"é.bin")).permissions() & std::filesystem::perms::mask) == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
    handle = open(path, 0, true, false);
    Require(handle >= 0);
    char bytes[8]{};
    Require(sceKernelRead(handle, bytes, sizeof(bytes)) == 4);
    Require(std::memcmp(bytes, "abcd", 4) == 0);
    Require(!truncate(handle, 1));
    close(handle);
    for (int mode = 0; mode != 3; ++mode) Require(open(path, mode, true, true) == -1);
    Require(std::filesystem::file_size(root / std::filesystem::path(u8"é.bin")) == 4);
    handle = open(path, 2, true, false);
    Require(handle >= 0 && seek(handle, 0, 2) == 4);
    Require(sceKernelWrite(handle, "ef", 2) == 2);
    Require(seek(handle, 0, 0) == 0);
    Require(sceKernelRead(handle, bytes, sizeof(bytes)) == 6);
    Require(std::memcmp(bytes, "abcdef", 6) == 0);
    close(handle);
    handle = open(path, 1, true, false);
    Require(handle >= 0 && seek(handle, 0, 2) == 0);
    Require(sceKernelClose(handle) == 0);
    handle = sceKernelOpen(guestPath, SCE_KERNEL_O_RDONLY, 0);
    Require(handle >= 0);
    close(handle);
    Require(handle == -1);
    handle = openAndLock(path, 2, 2);
    Require(handle >= 0 && sceKernelWrite(handle, "x", 1) == 1);
    unlockAndClose(handle);
    Require(seek(handle, 0, 0) == -1);
    Require(!truncate(-1, 0));
    Require(seek(-1, 0, 0) == -1);
    handle = -2;
    close(handle);
    Require(handle == -1);
    auto allPath = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>("/nkwebkit-handles/all.bin"));
#ifndef _WIN32
    const auto allMask = ::umask(0);
#endif
    handle = open(allPath, 2, true, true);
#ifndef _WIN32
    ::umask(allMask);
#endif
    Require(handle >= 0);
    close(handle);
#ifndef _WIN32
    Require((std::filesystem::status(root / "all.bin").permissions() & std::filesystem::perms::mask) == static_cast<std::filesystem::perms>(0666));
#endif
    auto readPath = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>("/nkwebkit-handles/read.bin"));
    handle = open(readPath, 0, false, true);
    Require(handle >= 0 && sceKernelRead(handle, bytes, sizeof(bytes)) == 0);
    close(handle);
    RemovePathAlias_nid_no_patch("/nkwebkit-handles");
    std::filesystem::remove_all(root);
}

static void RequireCurrentStackBounds() {
    static_assert(sizeof(WTF::StackBounds) == 16);
    static_assert(offsetof(WTF::StackBounds, m_origin) == 0);
    static_assert(offsetof(WTF::StackBounds, m_bound) == 8);
    const WTF::StackBounds bounds = _ZN3WTF11StackBounds32currentThreadStackBoundsInternalEv();
    int local = 0;
    const auto* here = reinterpret_cast<const char*>(&local);
    const auto* origin = static_cast<const char*>(bounds.m_origin);
    const auto* bound = static_cast<const char*>(bounds.m_bound);
    Require(origin > bound);
    Require(here < origin && here > bound);
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(pthread_attr_get_np_nid_postfix(pthread_self_nid_postfix(), &attr) == 0);
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    Require(pthread_attr_getstack_nid_postfix(&attr, &stackAddress, &stackSize) == 0);
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(bounds.m_bound == stackAddress);
    Require(origin == static_cast<const char*>(stackAddress) + stackSize);
}

static void TestStackBounds() {
    RequireCurrentStackBounds();
    std::thread worker(RequireCurrentStackBounds);
    worker.join();
}

static std::string CaptureLog(void (*body)()) {
    const auto path = (std::filesystem::temp_directory_path() / ("nkwebkit_timing_scope_" + std::to_string(_ZN3WTF13MonotonicTime3nowEv().m_value) + ".log")).string();
    std::fflush(stdout);
    std::fflush(stderr);
#ifdef _WIN32
    const int file = _open(path.c_str(), _O_CREAT | _O_TRUNC | _O_WRONLY, 0600);
    const int savedOut = _dup(1);
    const int savedErr = _dup(2);
    Require(file >= 0 && savedOut >= 0 && savedErr >= 0 && _dup2(file, 1) == 0 && _dup2(file, 2) == 0);
#else
    const int file = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
    const int savedOut = dup(1);
    const int savedErr = dup(2);
    Require(file >= 0 && savedOut >= 0 && savedErr >= 0 && dup2(file, 1) == 1 && dup2(file, 2) == 2);
#endif
    body();
    std::fflush(stdout);
    std::fflush(stderr);
#ifdef _WIN32
    Require(_dup2(savedOut, 1) == 0 && _dup2(savedErr, 2) == 0);
    _close(savedOut);
    _close(savedErr);
    _close(file);
#else
    Require(dup2(savedOut, 1) == 1 && dup2(savedErr, 2) == 2);
    close(savedOut);
    close(savedErr);
    close(file);
#endif
    std::ifstream input(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    std::filesystem::remove(path);
    return text;
}

static std::size_t CountOccurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) ++count;
    return count;
}

static const char g_firstTimingName[] = "nkwebkit-timing-first";
static const char g_secondTimingName[] = "nkwebkit-timing-second";

static void EndScope(const char* name, unsigned interval) {
    WTF::TimingScope scope{_ZN3WTF13MonotonicTime3nowEv(), name, interval};
    _ZN3WTF11TimingScope11scopeDidEndEv(&scope);
}

static void TestTimingScope() {
    static_assert(sizeof(WTF::TimingScope) == 24);
    static_assert(offsetof(WTF::TimingScope, m_startTime) == 0);
    static_assert(offsetof(WTF::TimingScope, m_name) == 8);
    static_assert(offsetof(WTF::TimingScope, m_logIterationInterval) == 16);
    const std::string first = CaptureLog([] {
        EndScope(g_firstTimingName, 2);
        EndScope(g_firstTimingName, 2);
        EndScope(g_secondTimingName, 2);
    });
    Require(CountOccurrences(first, std::string(g_firstTimingName) + ": 2 calls, mean duration: ") == 1);
    Require(CountOccurrences(first, std::string(g_firstTimingName) + ": 1 calls") == 0);
    Require(CountOccurrences(first, g_secondTimingName) == 0);
    Require(CountOccurrences(first, "ms, total duration: ") == 1);
    Require(CountOccurrences(first, "ms, max duration ") == 1);
    const std::string second = CaptureLog([] {
        EndScope(g_secondTimingName, 2);
        EndScope(g_firstTimingName, 1);
    });
    Require(CountOccurrences(second, std::string(g_secondTimingName) + ": 2 calls") == 1);
    Require(CountOccurrences(second, std::string(g_firstTimingName) + ": 3 calls") == 1);
    bool rejected = false;
    try {
        EndScope(g_firstTimingName, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
}

static void TestUniStdExtras() {
    const auto root = std::filesystem::current_path() / ("nkwebkit-unistd-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-unistd", reinterpret_cast<const char*>(hostPath.c_str()));
    const int handle = sceKernelOpen("/nkwebkit-unistd/file.bin", SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT, 0666);
    Require(handle >= 0);
    Require((fcntl_nid_postfix(handle, 3) & SCE_KERNEL_O_NONBLOCK) == 0);
    Require(_ZN3WTF11setNonBlockEi(handle));
    Require((fcntl_nid_postfix(handle, 3) & SCE_KERNEL_O_NONBLOCK) != 0);
    Require((fcntl_nid_postfix(handle, 3) & SCE_KERNEL_O_ACCMODE) == SCE_KERNEL_O_RDWR);
    Require(_ZN3WTF11setNonBlockEi(handle));
    Require(fcntl_nid_postfix(handle, 1) == 0);
    const int duplicate = _ZN3WTF14dupCloseOnExecEi(handle);
    Require(duplicate >= 0 && duplicate != handle);
    Require(fcntl_nid_postfix(duplicate, 1) == 1);
    Require(fcntl_nid_postfix(handle, 1) == 0);
    Require(sceKernelWrite(duplicate, "abc", 3) == 3);
    Require(sceKernelLseek(handle, 0, 1) == 3);
    Require(close_nid_postfix(duplicate) == 0);
    Require(sceKernelClose(handle) == 0);
    *__error_nid_postfix() = 0;
    Require(!_ZN3WTF11setNonBlockEi(handle));
    Require(*__error_nid_postfix() == 9);
    *__error_nid_postfix() = 0;
    Require(_ZN3WTF14dupCloseOnExecEi(-1) == -1);
    Require(*__error_nid_postfix() == 9);
    RemovePathAlias_nid_no_patch("/nkwebkit-unistd");
    std::filesystem::remove_all(root);
}

static void TestSleepSeconds() {
    double start = _ZN3WTF13MonotonicTime3nowEv().m_value;
    _ZN3WTF5sleepENS_7SecondsE(Seconds{0.05});
    Require(_ZN3WTF13MonotonicTime3nowEv().m_value - start >= 0.05);
    start = _ZN3WTF13MonotonicTime3nowEv().m_value;
    _ZN3WTF5sleepENS_7SecondsE(Seconds{0.0});
    _ZN3WTF5sleepENS_7SecondsE(Seconds{-5.0});
    _ZN3WTF5sleepENS_7SecondsE(Seconds{-std::numeric_limits<double>::infinity()});
    _ZN3WTF5sleepENS_7SecondsE(Seconds{std::numeric_limits<double>::quiet_NaN()});
    Require(_ZN3WTF13MonotonicTime3nowEv().m_value - start < 1.0);
}

static void TestMakeAllDirectories() {
    const auto make = &_ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE;
    const auto root = std::filesystem::current_path() / ("nkwebkit-dirs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-dirs", reinterpret_cast<const char*>(hostPath.c_str()));
    const auto guest = [](const char* text) {
        return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text));
    };
#ifndef _WIN32
    const auto oldMask = ::umask(0);
#endif
    Require(make(guest("/nkwebkit-dirs/a/b/c")));
#ifndef _WIN32
    ::umask(oldMask);
#endif
    Require(std::filesystem::is_directory(root / "a" / "b" / "c"));
#ifndef _WIN32
    Require((std::filesystem::status(root / "a" / "b").permissions() & std::filesystem::perms::mask) == std::filesystem::perms::owner_all);
    Require((std::filesystem::status(root / "a" / "b" / "c").permissions() & std::filesystem::perms::mask) == std::filesystem::perms::owner_all);
#endif
    Require(make(guest("/nkwebkit-dirs/a/b/c")));
    Require(make(guest("/nkwebkit-dirs")));
    Require(make(guest("/nkwebkit-dirs/d/e/")));
    Require(std::filesystem::is_directory(root / "d" / "e"));
    Require(make(guest("/nkwebkit-dirs/\xC3\xA9/z")));
    Require(std::filesystem::is_directory(root / std::filesystem::path(u8"é") / "z"));
    {
        std::ofstream file(root / "f.bin", std::ios::binary);
        Require(static_cast<bool>(file));
    }
    Require(make(guest("/nkwebkit-dirs/f.bin")));
    Require(std::filesystem::is_regular_file(root / "f.bin"));
    Require(!make(guest("/nkwebkit-dirs/f.bin/x")));
    Require(!std::filesystem::exists(root / "f.bin" / "x"));
    Require(!make(guest("/nkwebkit-dirs/f.bin/x/y")));
    NKWebKit::WtfAbi::String nullPath;
    Require(!make(nullPath));
    Require(!make(guest("")));
    RemovePathAlias_nid_no_patch("/nkwebkit-dirs");
    std::filesystem::remove_all(root);
}

int main() {
    if (!TestBmallocHeapOperators()) {
        return 1;
    }
    TestAdditionalWtf();
    TestStackBounds();
    TestTimingScope();
    TestMemoryAllocations();
    TestAllocationBoundaries();
    TestTime();
    TestSystem();
    TestString();
    TestWebCorePure();
    TestJscAndPal();
    TestMappedFiles();
    TestFileHandles();
    TestUniStdExtras();
    TestSleepSeconds();
    TestMakeAllDirectories();
    return 0;
}

namespace Wtf2Tests {

using namespace NKWebKit::WtfAbi;

extern "C" {
}

const auto Create8 = &_ZN3WTF10StringImpl6createEPKhj;
const auto Create16 = &_ZN3WTF10StringImpl6createEPKDsj;
const auto Narrow = &_ZN3WTF10StringImpl20create8BitIfPossibleEPKDsj;
const auto Uninit8 = &_ZN3WTF10StringImpl19createUninitializedEjRPh;
const auto Uninit16 = &_ZN3WTF10StringImpl19createUninitializedEjRPDs;
const auto ImplLower = &_ZN3WTF10StringImpl23convertToASCIILowercaseEv;
const auto ImplUpper = &_ZN3WTF10StringImpl23convertToASCIIUppercaseEv;
const auto Lower = &_ZNK3WTF6String23convertToASCIILowercaseEv;
const auto Upper = &_ZNK3WTF6String23convertToASCIIUppercaseEv;
const auto ViewLower = &_ZNK3WTF10StringView23convertToASCIILowercaseEv;
const auto ViewUpper = &_ZNK3WTF10StringView23convertToASCIIUppercaseEv;
const auto ImplUTF8 = &_ZNK3WTF10StringImpl4utf8ENS_14ConversionModeE;
const auto UTF8 = &_ZNK3WTF6String4utf8ENS_14ConversionModeE;
const auto ViewUTF8 = &_ZNK3WTF10StringView4utf8ENS_14ConversionModeE;
const auto Utf8For16 = &_ZN3WTF10StringImpl17utf8ForCharactersEPKDsjNS_14ConversionModeE;
const auto Utf8For8 = &_ZN3WTF10StringImpl17utf8ForCharactersEPKhj;
const auto FromUTF8 = &_ZN3WTF6String8fromUTF8EPKhm;
const auto FromUTF8Z = &_ZN3WTF6String8fromUTF8EPKh;
const auto FromCString = &_ZN3WTF6String8fromUTF8ERKNS_7CStringE;
const auto LatinFallback = &_ZN3WTF6String26fromUTF8WithLatin1FallbackEPKhm;
const auto FromCodePoint = &_ZN3WTF6String13fromCodePointEi;
const auto Character = &_ZNK3WTF6String19characterStartingAtEj;
const auto ImplCharacter = &_ZN3WTF10StringImpl19characterStartingAtEj;
const auto Hash = &_ZNK3WTF10StringImpl12hashSlowCaseEv;
const auto ConcurrentHash = &_ZNK3WTF10StringImpl14concurrentHashEv;
const auto BytesSize = &_ZNK3WTF10StringImpl11sizeInBytesEv;
const auto Starts = &_ZNK3WTF10StringImpl10startsWithEDs;
const auto Ends = &_ZNK3WTF10StringImpl8endsWithEDs;
const auto Prefix = &_ZNK3WTF10StringImpl10startsWithEPKcj;
const auto Suffix = &_ZNK3WTF10StringImpl8endsWithEPKcj;
const auto ViewStarts = &_ZNK3WTF10StringView10startsWithEDs;
const auto ViewEnds = &_ZNK3WTF10StringView8endsWithEDs;
const auto ReverseFind = &_ZN3WTF10StringImpl11reverseFindEDsj;
const auto Substring = &_ZN3WTF10StringImpl9substringEjj;
const auto SharedSubstring = &_ZNK3WTF6String20substringSharingImplEjj;
const auto Safe = &_ZNK3WTF6String27isSafeToSendToAnotherThreadEv;
const auto Ascii = &_ZNK3WTF6String5asciiEv;
const auto Latin1 = &_ZNK3WTF6String6latin1Ev;
const auto Find = &_ZNK3WTF10StringView4findES0_j;
const auto Case8 = &_ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPh;
const auto Case16 = &_ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPDs;
const auto EqualViews = &_ZN3WTF22equalRespectingNullityENS_10StringViewES0_;
const auto Equal = &_ZN3WTF5equalEPKNS_10StringImplES2_;
const auto EqualRefs = &_ZN3WTF5equalERKNS_10StringImplES2_;
const auto Equal8 = &_ZN3WTF5equalEPKNS_10StringImplEPKhj;
const auto Equal16 = &_ZN3WTF5equalEPKNS_10StringImplEPKDsj;
const auto EqualZ = &_ZN3WTF5equalEPKNS_10StringImplEPKh;
const auto EqualNullity = &_ZN3WTF20equalIgnoringNullityEPNS_10StringImplES1_;
const auto EqualRawNullity = &_ZN3WTF20equalIgnoringNullityEPKDsmPNS_10StringImplE;
const auto EqualCase = &_ZN3WTF22equalIgnoringASCIICaseEPKNS_10StringImplES2_;
const auto EqualCaseNonNull = &_ZN3WTF29equalIgnoringASCIICaseNonNullEPKNS_10StringImplES2_;
const auto CStringNew = &_ZN3WTF7CString16newUninitializedEmRPc;
const auto CStringMutable = &_ZN3WTF7CString11mutableDataEv;
const auto CStringGrow = &_ZN3WTF7CString4growEm;
const auto CStringHash = &_ZNK3WTF7CString4hashEv;
const auto CStringEqual = &_ZN3WTFeqERKNS_7CStringES2_;
const auto CStringEqualZ = &_ZN3WTFeqERKNS_7CStringEPKc;
const auto CStringLess = &_ZN3WTFltERKNS_7CStringES2_;
const auto CStringHashEqual = &_ZN3WTF11CStringHash5equalERKNS_7CStringES3_;
const auto AllPrivileges = &_ZN3WTF13allPrivilegesEv;
const auto SetPrivileges = &_ZN3WTF20setProcessPrivilegesENS_9OptionSetINS_16ProcessPrivilegeEEE;
const auto AddPrivilege = &_ZN3WTF19addProcessPrivilegeENS_16ProcessPrivilegeE;
const auto RemovePrivilege = &_ZN3WTF22removeProcessPrivilegeENS_16ProcessPrivilegeE;
const auto HasPrivilege = &_ZN3WTF19hasProcessPrivilegeENS_16ProcessPrivilegeE;
const auto SixHash = &_ZN3WTF31sixCharacterHashStringToIntegerEPKc;
const auto IsReload = &_ZN7WebCore8isReloadENS_13FrameLoadTypeE;
const auto IsBackForward = &_ZN7WebCore21isBackForwardLoadTypeENS_13FrameLoadTypeE;

const unsigned char* U8(const char* data) { return reinterpret_cast<const unsigned char*>(data); }
std::string Bytes(const CString& value) {
    return value.buffer ? std::string(reinterpret_cast<const char*>(value.buffer + 1), value.buffer->length) : std::string();
}
void Text(const String& value, const char* expected) {
    Require(value.impl != nullptr);
    Require(Bytes(UTF8(&value, 1)) == expected);
}

void TestStorageAndQueries() {
    String null;
    auto empty = Create8(U8(""), 0);
    Require(null.impl == nullptr && empty.impl != nullptr && empty.impl->length == 0);
    auto fromNull = Create16(nullptr, 99);
    Require(fromNull.impl == empty.impl);
    unsigned char* data8 = reinterpret_cast<unsigned char*>(1);
    auto empty8 = Uninit8(0, data8);
    Require(!data8 && empty8.impl == empty.impl);
    char16_t* data16 = reinterpret_cast<char16_t*>(1);
    auto empty16 = Uninit16(0, data16);
    Require(!data16 && empty16.impl == empty.impl);
    auto made8 = Uninit8(3, data8);
    std::memcpy(data8, "aBa", 3);
    Require(made8.impl->refCount == 2 && made8.impl->hashAndFlags == 4);
    Require(made8.impl->data == reinterpret_cast<unsigned char*>(made8.impl) + 20);
    auto made16 = Uninit16(3, data16);
    data16[0] = 'a'; data16[1] = 'B'; data16[2] = 'a';
    Require(made16.impl->hashAndFlags == 0);
    Require(Equal(made8.impl, made16.impl) && EqualRefs(*made8.impl, *made16.impl));
    Require(Equal8(made16.impl, U8("aBa"), 3) && Equal16(made8.impl, u"aBa", 3));
    Require(EqualZ(made16.impl, U8("aBa")) && !EqualZ(made16.impl, U8("aB")));
    Require(Equal(nullptr, nullptr) && !Equal(nullptr, empty.impl));
    Require(Equal8(nullptr, nullptr, 4) && !Equal8(empty.impl, nullptr, 0));
    Require(EqualNullity(nullptr, empty.impl) && EqualRawNullity(nullptr, 0, empty.impl));
    Require(!EqualNullity(nullptr, made8.impl) && !EqualRawNullity(u"aB", 2, made8.impl));
    auto lower = Create8(U8("aba"), 3);
    Require(EqualCase(made16.impl, lower.impl) && EqualCaseNonNull(made8.impl, lower.impl));
    Require(EqualCase(nullptr, nullptr) && !EqualCase(nullptr, empty.impl));
    ExpectErr([&] { EqualCaseNonNull(nullptr, lower.impl); });
    Require(BytesSize(made8.impl) == 27 && BytesSize(made16.impl) == 30);
    Require(ConcurrentHash(made8.impl) == ConcurrentHash(made16.impl));
    Require(made8.impl->hashAndFlags == 4);
    auto hash = Hash(made8.impl);
    Require(hash != 0 && made8.impl->hashAndFlags == ((hash << 8) | 4));
    Require(Hash(made16.impl) == hash);
    auto hello = Create8(U8("hello"), 5);
    Require(Hash(hello.impl) == 0x00D17551U);
    unsigned bytesRead = 99;
    unsigned utf16Read = 99;
    const char* valid = "hello";
    Require(_ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(valid, valid + 5, bytesRead, utf16Read) == 0x00D17551U);
    Require(bytesRead == 5 && utf16Read == 5);
    for (const char* invalid : {"\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
        bytesRead = 99;
        Require(!_ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(invalid, invalid + std::strlen(invalid), bytesRead, utf16Read));
        Require(bytesRead == 99 && utf16Read == 0);
    }
    auto released = Create8(U8("fastMalloc"), 10);
    auto* storage = std::exchange(released.impl, nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(storage) >= 30);
    _ZN3WTF8fastFreeEPv(storage);
    Require(Starts(made8.impl, 'a') && Ends(made16.impl, 'a'));
    Require(!Starts(empty.impl, 0) && !Ends(empty.impl, 0));
    Require(Prefix(made16.impl, "aB", 2) && Suffix(made8.impl, "Ba", 2));
    Require(Prefix(empty.impl, nullptr, 0) && !Prefix(made8.impl, "aBab", 4));
    Require(!Suffix(made16.impl, "bb", 2));
    ExpectErr([&] { Prefix(made8.impl, nullptr, 1); });
    Require(ReverseFind(made8.impl, 'a', 99) == 2 && ReverseFind(made16.impl, 'a', 1) == 0);
    Require(ReverseFind(empty.impl, 'a', 0) == std::size_t(-1));
    Require(ReverseFind(made8.impl, 0x100, 99) == std::size_t(-1));
    auto narrow = Narrow(u"caf\u00e9", 4);
    Require((narrow.impl->hashAndFlags & 4) != 0);
    auto wide = Narrow(u"x\u0100", 2);
    Require((wide.impl->hashAndFlags & 4) == 0);
    Require(Character(&null, 0) == 0 && Character(&made8, 99) == 0);
    Require(ImplCharacter(made8.impl, 1) == 'B');
    ExpectErr([&] { ImplCharacter(made8.impl, 3); });
    ExpectErr([&] { BytesSize(nullptr); });
    ExpectErr([&] { Uninit16(0xFFFFFFFF, data16); });
    Require(Safe(&null) && Safe(&made8) && Safe(&empty));
    { auto copy = made8; Require(!Safe(&made8)); }
    Require(Safe(&made8));
}

void TestASCIIAndViews() {
    auto original = Create16(u"Ab\u00c9z", 4);
    auto lower = ImplLower(original.impl);
    auto upper = ImplUpper(original.impl);
    Require(Equal16(lower.impl, u"ab\u00c9z", 4));
    Require(Equal16(upper.impl, u"AB\u00c9Z", 4));
    { auto same = ImplLower(lower.impl); Require(same.impl == lower.impl && lower.impl->refCount == 4); }
    Require(lower.impl->refCount == 2);
    Require(Equal16(Lower(&original).impl, u"ab\u00c9z", 4));
    Require(Equal16(Upper(&original).impl, u"AB\u00c9Z", 4));
    String null;
    Require(!Lower(&null).impl && !Upper(&null).impl);
    StringView view{U8("aBcD"), 4, true};
    StringView wide{u"aBcD", 4, false};
    StringView empty{"", 0, true};
    StringView absent{nullptr, 0, true};
    Text(ViewLower(&view), "abcd");
    Text(ViewUpper(&wide), "ABCD");
    Require(!ViewLower(&absent).impl && ViewUpper(&empty).impl);
    Require(ViewStarts(&view, 'a') && ViewEnds(&wide, 'D'));
    Require(!ViewStarts(&absent, 0) && !ViewEnds(&empty, 0));
    Require(EqualViews(view, wide) && !EqualViews(absent, empty));
    Require(EqualViews(absent, absent) && EqualViews(empty, empty));
    StringView match{u"Bc", 2, false};
    Require(Find(&view, match, 0) == 1 && Find(&wide, {"D", 1, true}, 2) == 3);
    Require(Find(&view, match, 2) == std::size_t(-1));
    Require(Find(&view, absent, 4) == 4 && Find(&view, absent, 5) == std::size_t(-1));
    unsigned char output8[5] = {0, 0, 0, 0, 77};
    char16_t output16[5] = {0, 0, 0, 0, 77};
    Case8(&view, 0, output8);
    Case16(&view, 1, output16);
    Require(!std::memcmp(output8, "ABCD", 4) && output8[4] == 77);
    Require(!std::memcmp(output16, u"abcd", 8) && output16[4] == 77);
    Case16(&wide, 0, output16);
    Require(!std::memcmp(output16, u"ABCD", 8));
    Case8(&absent, 0, nullptr);
    ExpectErr([&] { Case8(&wide, 1, output8); });
    ExpectErr([&] { Case16(&wide, 1, nullptr); });
    auto special = Create16(u"\0\n\x7f\u00e9\u0100", 5);
    Require(Bytes(Ascii(&special)) == std::string("\0?\x7f??", 5));
    Require(Bytes(Latin1(&special)) == std::string("\0\n\x7f\xe9?", 5));
    Require(Ascii(&null).buffer != nullptr && Latin1(&null).buffer != nullptr);
}

void TestUTFConversions() {
    String null;
    Require(Bytes(UTF8(&null, 1)).empty());
    Require(!FromUTF8(nullptr, 9).impl && !FromUTF8Z(nullptr).impl);
    auto ascii = FromUTF8(U8("abc"), 3);
    Require((ascii.impl->hashAndFlags & 4) != 0);
    auto unicode = FromUTF8(U8("caf\xc3\xa9\xf0\x9f\x98\x80"), 9);
    Require(unicode.impl && unicode.impl->length == 6 && !(unicode.impl->hashAndFlags & 4));
    Require(Character(&unicode, 4) == 0x1F600 && Character(&unicode, 5) == 0);
    Require(Bytes(ImplUTF8(unicode.impl, 1)) == "caf\xc3\xa9\xf0\x9f\x98\x80");
    StringView view{unicode.impl->data, unicode.impl->length, false};
    Require(Bytes(ViewUTF8(&view, 1)) == Bytes(UTF8(&unicode, 1)));
    for (const char* invalid : {"\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xe2\x82"}) {
        Require(!FromUTF8Z(U8(invalid)).impl);
    }
    auto fallback = LatinFallback(U8("\xff"), 1);
    Require(Equal16(fallback.impl, u"\xff", 1) && (fallback.impl->hashAndFlags & 4));
    auto zero = FromUTF8(U8("a\0b"), 3);
    Require(zero.impl->length == 3 && Character(&zero, 1) == 0);
    char16_t bad[] = {0xD800, 'X', 0xDC00, 0xDFFF};
    auto malformed = Create16(bad, 4);
    Require(ImplCharacter(malformed.impl, 0) == 0 && ImplCharacter(malformed.impl, 2) == 0);
    Require(Bytes(UTF8(&malformed, 0)) == "\xed\xa0\x80X\xed\xb0\x80\xed\xbf\xbf");
    Require(Bytes(UTF8(&malformed, 2)) == "\xef\xbf\xbdX\xef\xbf\xbd\xef\xbf\xbd");
    ExpectErr([&] { UTF8(&malformed, 1); });
    Require(Bytes(Utf8For16(bad, 4, 0)) == "\xed\xa0\x80X\xed\xb0\x80\xed\xbf\xbf");
    Require(Bytes(Utf8For16(bad, 4, 2)) == "\xef\xbf\xbdX\xef\xbf\xbd\xef\xbf\xbd");
    ExpectErr([&] { Utf8For16(bad, 4, 1); });
    char16_t paired[] = {'c', 'a', 'f', 0xE9, 0xD83D, 0xDE00};
    Require(Bytes(Utf8For16(paired, 6, 1)) == "caf\xc3\xa9\xf0\x9f\x98\x80");
    Require(Bytes(Utf8For16(paired, 0, 1)).empty() && Bytes(Utf8For16(nullptr, 0, 1)).empty());
    ExpectErr([&] { Utf8For16(nullptr, 1, 1); });
    Require(Bytes(Utf8For8(U8("caf\xe9\x80"), 5)) == "caf\xc3\xa9\xc2\x80");
    Require(Bytes(Utf8For8(U8("a\0b"), 3)) == std::string("a\0b", 3));
    Require(Bytes(Utf8For8(U8("x"), 0)).empty() && Bytes(Utf8For8(nullptr, 0)).empty());
    ExpectErr([&] { Utf8For8(nullptr, 1); });
    Text(FromCodePoint(0x1F600), "\xf0\x9f\x98\x80");
    Require(!FromCodePoint(-1).impl && !FromCodePoint(0x110000).impl);
    auto surrogate = FromCodePoint(0xD800);
    Require(surrogate.impl && surrogate.impl->length == 1 && Character(&surrogate, 0) == 0);
    ExpectErr([&] { FromUTF8(U8(""), std::size_t(0x80000000)); });
}

void TestSubstrings() {
    auto original = Create8(U8("abcdefghijklmnopqrstuvwx"), 24);
    { auto same = Substring(original.impl, 0, 99); Require(same.impl == original.impl); }
    Text(Substring(original.impl, 23, 99), "x");
    Require(Substring(original.impl, 99, 9).impl->length == 0);
    auto shared = SharedSubstring(&original, 1, 20);
    Require((shared.impl->hashAndFlags & 3) == 2);
    Require(shared.impl->data == static_cast<const unsigned char*>(original.impl->data) + 1);
    Require(original.impl->refCount == 4);
    {
        auto nested = SharedSubstring(&shared, 1, 18);
        Text(nested, "cdefghijklmnopqrst");
        Require(original.impl->refCount == 6 && shared.impl->refCount == 2);
    }
    Require(original.impl->refCount == 4);
    auto small = SharedSubstring(&original, 1, 3);
    Require((small.impl->hashAndFlags & 3) == 0);
    Text(small, "bcd");
    String null;
    Require(!SharedSubstring(&null, 5, 9).impl);
    auto wide = Create16(u"abcdefghijklmnopqrstuvwx", 24);
    auto wideShared = SharedSubstring(&wide, 2, 20);
    Require((wideShared.impl->hashAndFlags & 3) == 2 && !(wideShared.impl->hashAndFlags & 4));
    Text(wideShared, "cdefghijklmnopqrstuv");
}

void TestCStrings() {
    char* data;
    auto value = CStringNew(3, data);
    Require(data[3] == 0 && value.buffer->refCount == 1);
    std::memcpy(data, "abc", 3);
    CString shared(value);
    Require(value.buffer->refCount == 2);
    char* writable = CStringMutable(&shared);
    writable[0] = 'z';
    Require(Bytes(value) == "abc" && Bytes(shared) == "zbc");
    Require(value.buffer->refCount == 1 && shared.buffer->refCount == 1);
    Require(CStringLess(value, shared) && !CStringEqual(value, shared));
    Require(CStringEqualZ(value, "abc") && !CStringEqualZ(value, nullptr));
    auto embedded = CStringNew(3, data);
    std::memcpy(data, "a\0b", 3);
    auto different = CStringNew(3, data);
    std::memcpy(data, "a\0c", 3);
    Require(!CStringEqual(embedded, different) && CStringEqualZ(embedded, "a"));
    Require(CStringHash(&embedded) == CStringHash(&different));
    Require(!CStringHashEqual(embedded, different));
    CString absent;
    auto empty = CStringNew(0, data);
    Require(data[0] == 0 && !CStringEqual(absent, empty));
    Require(CStringHash(&absent) == 0 && CStringHash(&empty) != 0);
    Require(CStringEqualZ(absent, nullptr) && CStringLess(absent, empty));
    Require(!CStringMutable(&absent));
    CStringGrow(&value, 8);
    Require(value.buffer->length == 8 && !std::memcmp(Bytes(value).data(), "abc\0", 4));
    writable = CStringMutable(&value);
    std::memcpy(writable + 3, "defgh", 5);
    writable[8] = 0;
    Require(Bytes(value) == "abcdefgh");
    ExpectErr([&] { CStringGrow(&value, 8); });
    ExpectErr([&] { CStringGrow(&absent, 1); });
    ExpectErr([&] { CStringNew(std::size_t(-1), data); });
    Text(FromCString(embedded), "a");
    Require(!FromCString(absent).impl);
    auto deleted = reinterpret_cast<CStringBuffer*>(std::uintptr_t(-1));
    CString a(deleted), b(deleted);
    Require(CStringHashEqual(a, b) && !CStringHashEqual(a, value));
    a.buffer = nullptr; b.buffer = nullptr;
}

void TestPrimitiveState() {
    Require(AllPrivileges().storage == 7);
    SetPrivileges({0});
    Require(!HasPrivilege(1));
    AddPrivilege(1); AddPrivilege(4);
    Require(HasPrivilege(1) && HasPrivilege(4) && !HasPrivilege(2));
    RemovePrivilege(1);
    Require(!HasPrivilege(1) && HasPrivilege(4));
    SetPrivileges({2});
    Require(HasPrivilege(2) && !HasPrivilege(4));
    SetPrivileges({0});
    Require(SixHash("AAAAAA") == 0 && SixHash("AAAAAB") == 1);
    Require(SixHash("AAAAAa") == 26 && SixHash("AAAAA0") == 52);
    Require(SixHash("999999") == 965660735U);
    ExpectErr([&] { SixHash(nullptr); });
    ExpectErr([&] { SixHash("short"); });
    ExpectErr([&] { SixHash("AAAAAAA"); });
    ExpectErr([&] { SixHash("AAAAA!"); });
    for (std::uint8_t type = 0; type < 10; ++type) {
        Require(IsReload(type) == (type == 4 || type == 8 || type == 9));
        Require(IsBackForward(type) == (type >= 1 && type <= 3));
    }
    ExpectErr([&] { IsReload(255); });
    ExpectErr([&] { IsBackForward(255); });
}

void TestIntegersAndConstructors() {
    Text(_ZN3WTF6String6numberEi(std::numeric_limits<int>::min()), "-2147483648");
    Text(_ZN3WTF6String6numberEi(0), "0");
    Text(_ZN3WTF6String6numberEj(std::numeric_limits<unsigned>::max()), "4294967295");
    Text(_ZN3WTF6String6numberEj(0), "0");
    Text(_ZN3WTF6String6numberEl(std::numeric_limits<std::int64_t>::min()), "-9223372036854775808");
    Text(_ZN3WTF6String6numberEl(0), "0");
    Text(_ZN3WTF6String6numberEm(std::numeric_limits<std::uint64_t>::max()), "18446744073709551615");
    Text(_ZN3WTF6String6numberEm(0), "0");
    Text(_ZN3WTF6String6numberEx(std::numeric_limits<long long>::min()), "-9223372036854775808");
    Text(_ZN3WTF6String6numberEx(0), "0");
    Text(_ZN3WTF6String6numberEy(std::numeric_limits<unsigned long long>::max()), "18446744073709551615");
    Text(_ZN3WTF6String6numberEy(0), "0");
    { String value; _ZN3WTF6StringC1EPKDsj(&value, u"test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKDsj(&value, nullptr, 0); Require(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKDsj(&value, u"test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKDsj(&value, nullptr, 0); Require(!value.impl); }
    { String value; _ZN3WTF6StringC1EPKc(&value, "test"); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKc(&value, nullptr); Require(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKc(&value, "test"); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKc(&value, nullptr); Require(!value.impl); }
    { String value; _ZN3WTF6StringC1EPKcj(&value, "test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKcj(&value, nullptr, 0); Require(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKcj(&value, "test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKcj(&value, nullptr, 0); Require(!value.impl); }
    { String value; _ZN3WTF6StringC1EPKhj(&value, U8("test"), 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKhj(&value, nullptr, 0); Require(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKhj(&value, U8("test"), 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKhj(&value, nullptr, 0); Require(!value.impl); }
    { CString value; _ZN3WTF7CStringC1EPKc(&value, "test"); Require(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC1EPKc(&value, nullptr); Require(!value.buffer); }
    { CString value; _ZN3WTF7CStringC2EPKc(&value, "test"); Require(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC2EPKc(&value, nullptr); Require(!value.buffer); }
    { CString value; _ZN3WTF7CStringC1EPKcm(&value, "test", 4); Require(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC1EPKcm(&value, nullptr, 0); Require(!value.buffer); }
    { CString value; _ZN3WTF7CStringC2EPKcm(&value, "test", 4); Require(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC2EPKcm(&value, nullptr, 0); Require(!value.buffer); }
}

std::u16string Units(const String& value) {
    std::u16string result;
    if (!value.impl) return result;
    for (unsigned i = 0; i < value.impl->length; ++i) {
        if (value.impl->hashAndFlags & 4) result.push_back(static_cast<const unsigned char*>(value.impl->data)[i]);
        else result.push_back(static_cast<const char16_t*>(value.impl->data)[i]);
    }
    return result;
}

void TestFoldQuoteMarks() {
    const auto fold = &_ZN7WebCore14foldQuoteMarksERKN3WTF6StringE;
    struct Mark { char16_t code; char16_t folded; };
    const Mark marks[] = {
        {0x00AB, u'"'}, {0x00BB, u'"'}, {0x05F4, u'"'}, {0x201C, u'"'}, {0x201D, u'"'}, {0x201E, u'"'},
        {0x201F, u'"'}, {0x2E42, u'"'}, {0x301D, u'"'}, {0x301E, u'"'}, {0x301F, u'"'}, {0xFF02, u'"'},
        {0x05F3, u'\''}, {0x2018, u'\''}, {0x2019, u'\''}, {0x201A, u'\''}, {0x201B, u'\''}, {0x2039, u'\''},
        {0x203A, u'\''}, {0x300C, u'\''}, {0x300D, u'\''}, {0x300E, u'\''}, {0x300F, u'\''}, {0xFE41, u'\''},
        {0xFE42, u'\''}, {0xFE43, u'\''}, {0xFE44, u'\''}, {0xFF07, u'\''}, {0xFF62, u'\''}, {0xFF63, u'\''},
    };
    Require(sizeof(marks) / sizeof(marks[0]) == 30);
    for (const auto& mark : marks) {
        const char16_t input[] = {u'a', mark.code, u'b'};
        auto wide = Create16(input, 3);
        auto folded = fold(&wide);
        Require(folded.impl != wide.impl);
        Require(!(folded.impl->hashAndFlags & 4));
        Require(Units(folded) == std::u16string({u'a', mark.folded, u'b'}));
        Require(Units(wide) == std::u16string(input, 3));
        const char16_t single[] = {mark.code};
        auto lone = Create16(single, 1);
        Require(Units(fold(&lone)) == std::u16string(1, mark.folded));
    }
    const char16_t unchanged[] = {u'"', u'\'', u'a', 0x2017, 0x2020, 0x2038, 0x203B, 0x300B, 0x3010, 0xFE40, 0xFE45, 0xFF01, 0xFF08, 0xFF61, 0xFF64, 0x05F2, 0x05F5, 0x00AA, 0x00BC, 0x4E2D};
    auto plain = Create16(unchanged, sizeof(unchanged) / sizeof(unchanged[0]));
    auto plainResult = fold(&plain);
    Require(plainResult.impl == plain.impl);
    Require(plain.impl->refCount == 4);
    const char16_t mixed[] = {0x201C, u'h', u'i', 0x201D, u' ', 0x2018, u'x', 0x2019, 0x4E2D, 0x300C, 0x300D, 0xFF07, 0xFF02};
    auto mixedString = Create16(mixed, 13);
    Require(Units(fold(&mixedString)) == std::u16string({u'"', u'h', u'i', u'"', u' ', u'\'', u'x', u'\'', 0x4E2D, u'\'', u'\'', u'\'', u'"'}));
    const char16_t asciiAfter[] = {0x201C, u'a', 0x201D};
    auto wideAscii = Create16(asciiAfter, 3);
    auto wideAsciiResult = fold(&wideAscii);
    Require(Units(wideAsciiResult) == u"\"a\"");
    Require(!(wideAsciiResult.impl->hashAndFlags & 4));

    const unsigned char latin[] = {0xAB, 'a', 0xBB, 0xE9, 0xA0};
    auto narrow = Create8(latin, 5);
    Require(narrow.impl->hashAndFlags & 4);
    auto narrowResult = fold(&narrow);
    Require(narrowResult.impl != narrow.impl);
    Require(narrowResult.impl->hashAndFlags & 4);
    Require(Units(narrowResult) == std::u16string({u'"', u'a', u'"', 0xE9, 0xA0}));
    Require(Units(narrow) == std::u16string({0xAB, u'a', 0xBB, 0xE9, 0xA0}));
    auto narrowPlain = Create8(U8("it's"), 4);
    auto narrowPlainResult = fold(&narrowPlain);
    Require(narrowPlainResult.impl == narrowPlain.impl);
    Require(narrowPlain.impl->refCount == 4);

    auto empty = Create16(u"", 0);
    auto emptyResult = fold(&empty);
    Require(emptyResult.impl == empty.impl);
    Require(!emptyResult.impl->length);
    String absent;
    Require(!fold(&absent).impl);
    ExpectErr([&] { fold(nullptr); });
}

void TestDefaultMIMEType() {
    const auto defaultType = &_ZN7WebCore15defaultMIMETypeEv;
    const String& first = defaultType();
    const String& second = defaultType();
    Require(&first == &second);
    Require(first.impl);
    Require(first.impl->length == 24);
    Require(first.impl->hashAndFlags & 4);
    Require(!(first.impl->hashAndFlags & 48));
    Require(Units(first) == u"application/octet-stream");
    Require(first.impl->refCount & 1);
    const unsigned refCount = first.impl->refCount;
    {
        String copy(first);
        Require(copy.impl == first.impl);
        Require(first.impl->refCount == refCount + 2);
    }
    Require(first.impl->refCount == refCount);
    Require(ConcurrentHash(first.impl) == (first.impl->hashAndFlags >> 8));
    auto fresh = Create8(U8("application/octet-stream"), 24);
    Require(Equal(first.impl, fresh.impl));
    Require(EqualZ(first.impl, U8("application/octet-stream")));
}

void TestRedirectChainAllowsReuse() {
    const auto allows = &_ZN7WebCore24redirectChainAllowsReuseENS_24RedirectChainCacheStatusENS_28ReuseExpiredRedirectionOrNotE;
    using Status = WebCore::RedirectChainCacheStatus::Status;
    const double now = _ZN3WTF8WallTime3nowEv().m_value;
    const double infinity = std::numeric_limits<double>::infinity();
    for (int reuse = 0; reuse < 2; ++reuse) {
        Require(allows({WallTime{infinity}, Status::NoRedirection}, reuse));
        Require(allows({WallTime{0.0}, Status::NoRedirection}, reuse));
        Require(allows({WallTime{std::nan("")}, Status::NoRedirection}, reuse));
        Require(!allows({WallTime{infinity}, Status::NotCachedRedirection}, reuse));
        Require(!allows({WallTime{0.0}, Status::NotCachedRedirection}, reuse));
    }
    Require(allows({WallTime{infinity}, Status::CachedRedirection}, 0));
    Require(allows({WallTime{now + 1.0e9}, Status::CachedRedirection}, 0));
    Require(!allows({WallTime{now - 1.0e9}, Status::CachedRedirection}, 0));
    Require(!allows({WallTime{0.0}, Status::CachedRedirection}, 0));
    Require(!allows({WallTime{-infinity}, Status::CachedRedirection}, 0));
    Require(!allows({WallTime{std::nan("")}, Status::CachedRedirection}, 0));
    Require(allows({WallTime{now - 1.0e9}, Status::CachedRedirection}, 1));
    Require(allows({WallTime{0.0}, Status::CachedRedirection}, 1));
    Require(allows({WallTime{-infinity}, Status::CachedRedirection}, 1));
    Require(allows({WallTime{std::nan("")}, Status::CachedRedirection}, 1));
    Require(allows({WallTime{now - 1.0e9}, Status::CachedRedirection}, 2));
    Require(allows({WallTime{now + 1.0e9}, Status::CachedRedirection}, 1));
    Require(!allows({WallTime{infinity}, static_cast<Status>(3)}, 0));
    Require(!allows({WallTime{infinity}, static_cast<Status>(255)}, 1));
}

void TestSharedStringHash() {
    const auto hash16 = &_ZN7WebCore23computeSharedStringHashEPKDsj;
    const auto hashString = &_ZN7WebCore23computeSharedStringHashERKN3WTF6StringE;
    struct Vector { const char* latin1; const char16_t* utf16; unsigned length; unsigned expected; };
    const Vector vectors[] = {
        {"", u"", 0, 0x04ec889eu},
        {"a", u"a", 1, 0xf095343bu},
        {"ab", u"ab", 2, 0x1b7603e0u},
        {"abc", u"abc", 3, 0xcc946882u},
        {"http://example.com/", u"http://example.com/", 19, 0x836f1d81u},
    };
    for (const auto& vector : vectors) {
        Require(hash16(vector.utf16, vector.length) == vector.expected);
        String narrow;
        _ZN3WTF6StringC1EPKcj(&narrow, vector.latin1, vector.length);
        Require(!vector.length || (narrow.impl->hashAndFlags & 4));
        Require(hashString(&narrow) == vector.expected);
        String wide;
        _ZN3WTF6StringC1EPKDsj(&wide, vector.utf16, vector.length);
        Require(!vector.length || !(wide.impl->hashAndFlags & 4));
        Require(hashString(&wide) == vector.expected);
    }
    const char16_t nonLatin1[] = {0x00e9, 0x4e2d};
    Require(hash16(nonLatin1, 2) == 0x77f0e865u);
    String nonLatin1String;
    _ZN3WTF6StringC1EPKDsj(&nonLatin1String, nonLatin1, 2);
    Require(hashString(&nonLatin1String) == 0x77f0e865u);
    const char16_t deletedValue[] = {3163, 53528};
    Require(hash16(deletedValue, 2) == 0xffffffffu);
    String deletedString;
    _ZN3WTF6StringC1EPKDsj(&deletedString, deletedValue, 2);
    Require(hashString(&deletedString) == 0xffffffffu);
    String absent;
    Require(hashString(&absent) == 0x04ec889eu);
    Require(hash16(nullptr, 0) == 0x04ec889eu);
    ExpectErr([&] { hash16(nullptr, 1); });
    ExpectErr([&] { hashString(nullptr); });
}

void Run() {
    TestStorageAndQueries();
    TestASCIIAndViews();
    TestUTFConversions();
    TestSubstrings();
    TestCStrings();
    TestPrimitiveState();
    TestIntegersAndConstructors();
    TestFoldQuoteMarks();
    TestDefaultMIMEType();
    TestRedirectChainAllowsReuse();
    TestSharedStringHash();
}

}  // namespace Wtf2Tests

namespace Wtf2Tests { void TestURLsAndReallocation(); void TestURLLastPathComponentAndReferrer(); void TestFileNameHelpers(); void TestFileSystemRepresentations(); }

void TestAdditionalWtf() { Wtf2Tests::Run(); Wtf2Tests::TestURLsAndReallocation(); Wtf2Tests::TestURLLastPathComponentAndReferrer(); Wtf2Tests::TestFileNameHelpers(); Wtf2Tests::TestFileSystemRepresentations(); }

namespace Wtf2Tests {

extern "C" {
}

const auto URLProtocol = &_ZNK3WTF3URL8protocolEv;
const auto URLHost = &_ZNK3WTF3URL4hostEv;
const auto URLPath = &_ZNK3WTF3URL4pathEv;
const auto URLUser = &_ZNK3WTF3URL11encodedUserEv;
const auto URLPassword = &_ZNK3WTF3URL15encodedPasswordEv;
const auto URLFragment = &_ZNK3WTF3URL18fragmentIdentifierEv;
const auto URLFragmentSign = &_ZNK3WTF3URL39fragmentIdentifierWithLeadingNumberSignEv;
const auto URLQuery = &_ZNK3WTF3URL28queryWithLeadingQuestionMarkEv;
const auto URLIsProtocol = &_ZNK3WTF3URL10protocolIsENS_10StringViewE;
const auto URLHierarchical = &_ZNK3WTF3URL14isHierarchicalEv;
const auto URLBlank = &_ZNK3WTF3URL12isAboutBlankEv;
const auto URLSrcdoc = &_ZNK3WTF3URL13isAboutSrcDocEv;
const auto URLJavaScript = &_ZNK3WTF3URL20protocolIsJavaScriptEv;
const auto URLInvalidate = &_ZN3WTF3URL10invalidateEv;
const auto URLEqualIgnoringFragment = &_ZN3WTF31equalIgnoringFragmentIdentifierERKNS_3URLES2_;
const auto URLPortAllowed = &_ZN3WTF11portAllowedERKNS_3URLE;
const auto URLProtocolHostAndPortAreEqual = &_ZN3WTF27protocolHostAndPortAreEqualERKNS_3URLES2_;
const auto IsJavaScript = &_ZN3WTF20protocolIsJavaScriptENS_10StringViewE;
const auto IsHTTP = &_ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE;
const auto JSRealloc = &_ZN3WTF14jsValueReallocEPvm;
const auto URLLastPathComponent = &_ZNK3WTF3URL17lastPathComponentEv;
const auto URLStrippedForReferrer = &_ZNK3WTF3URL24strippedForUseAsReferrerEv;

std::string ViewText(StringView view) { return Bytes(ViewUTF8(&view, 1)); }

const auto URLEllipsize = &_ZNK3WTF3URL30stringCenterEllipsizedToLengthEj;
const auto DataURLMime = &_ZN3WTF19mimeTypeFromDataURLENS_10StringViewE;

bool Is8Bit(const String& value) { return value.impl && (value.impl->hashAndFlags & 4); }

StringView ViewOf(const String& value) {
    return {value.impl->data, value.impl->length, !!(value.impl->hashAndFlags & 4)};
}

StringView Literal(const char* text) { return {text, static_cast<unsigned>(std::strlen(text)), true}; }

void TestURLStringHelpers() {
    auto narrow = FromUTF8Z(U8("http://example.com/abcdefghij"));
    URL narrowURL{narrow.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    for (unsigned length : {29U, 30U, 40U, 0xFFFFFFFFU}) {
        auto same = URLEllipsize(&narrowURL, length);
        Require(same.impl == narrow.impl && narrow.impl->refCount == 4);
    }
    Require(narrow.impl->refCount == 2);
    Text(URLEllipsize(&narrowURL, 28), "http://exampl...m/abcdefghij");
    Text(URLEllipsize(&narrowURL, 10), "http...hij");
    Text(URLEllipsize(&narrowURL, 11), "http...hij");
    Text(URLEllipsize(&narrowURL, 6), "ht...j");
    Text(URLEllipsize(&narrowURL, 5), "h...");
    Text(URLEllipsize(&narrowURL, 4), "h...");
    Text(URLEllipsize(&narrowURL, 3), "...");
    Text(URLEllipsize(&narrowURL, 2), "...");
    Text(URLEllipsize(&narrowURL, 1), "http://example.com/abcdefghij...");
    Text(URLEllipsize(&narrowURL, 0), "http://example.com/abcdefghij...");
    Require(Is8Bit(URLEllipsize(&narrowURL, 10)) && Is8Bit(URLEllipsize(&narrowURL, 3)));
    Require(URLEllipsize(&narrowURL, 10).impl != narrow.impl);

    auto wide = Create16(u"http://wide.test/path", 21);
    URL wideURL{wide.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    Require(URLEllipsize(&wideURL, 21).impl == wide.impl);
    auto wideShort = URLEllipsize(&wideURL, 10);
    Text(wideShort, "http...ath");
    Require(!Is8Bit(wideShort));
    auto wideDots = URLEllipsize(&wideURL, 3);
    Text(wideDots, "...");
    Require(!Is8Bit(wideDots));
    auto wideLong = URLEllipsize(&wideURL, 1);
    Text(wideLong, "http://wide.test/path...");
    Require(!Is8Bit(wideLong));

    URL absent{};
    Require(URLEllipsize(&absent, 0).impl == nullptr && URLEllipsize(&absent, 10).impl == nullptr);
    auto empty = Create8(U8(""), 0);
    URL emptyURL{empty.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    Require(URLEllipsize(&emptyURL, 0).impl == empty.impl && URLEllipsize(&emptyURL, 5).impl == empty.impl);
    ExpectErr([&] { URLEllipsize(nullptr, 10); });

    Text(DataURLMime(Literal("data:text/HTML;base64,x")), "text/html");
    Text(DataURLMime(Literal("data:Text/X,y")), "text/x");
    Text(DataURLMime(Literal("data:,x")), "text/plain");
    Text(DataURLMime(Literal("data:;base64,x")), "text/plain");
    Require(Is8Bit(DataURLMime(Literal("data:,x"))));
    Text(DataURLMime(Literal("data:a,b;c")), "a,b");
    Text(DataURLMime(Literal("data:text/plain")), "");
    Text(DataURLMime(Literal("data:")), "");
    Text(DataURLMime(Literal("data")), "");
    Text(DataURLMime(Literal("")), "");
    Text(DataURLMime(StringView{nullptr, 0, true}), "");
    Text(DataURLMime(Literal("http://a;b")), "//a");
    auto wideData = Create16(u"data:TEXT/Plain;x", 17);
    auto wideMime = DataURLMime(ViewOf(wideData));
    Text(wideMime, "text/plain");
    Require(!Is8Bit(wideMime));
    auto accented = Create16(u"data:É/X,", 9);
    Text(DataURLMime(ViewOf(accented)), "\xC3\x89/x");
    auto widePlain = Create16(u"data:,x", 7);
    Text(DataURLMime(ViewOf(widePlain)), "text/plain");
}

const auto EncodeName = &_ZN3WTF14FileSystemImpl17encodeForFileNameERKNS_6StringE;
const auto DecodeName = &_ZN3WTF14FileSystemImpl18decodeFromFilenameERKNS_6StringE;
const auto LastComponent = &_ZN3WTF14FileSystemImpl40lastComponentOfPathIgnoringTrailingSlashERKNS_6StringE;

bool NameIs8Bit(const String& value) { return value.impl && (value.impl->hashAndFlags & 4); }

std::u16string NameUnits(const String& value) {
    Require(value.impl != nullptr);
    std::u16string units;
    for (unsigned i = 0; i < value.impl->length; ++i) {
        units.push_back(NameIs8Bit(value) ? static_cast<const unsigned char*>(value.impl->data)[i] : static_cast<const char16_t*>(value.impl->data)[i]);
    }
    return units;
}

String Wide(const std::u16string& units) { return Create16(units.data(), static_cast<unsigned>(units.size())); }

String NameString(const char* text) { return Create8(U8(text), static_cast<unsigned>(std::strlen(text))); }

void TestFileNameHelpers() {
    for (unsigned c = 0; c < 128; ++c) {
        char16_t unit = static_cast<char16_t>(c);
        auto encoded = EncodeName(&(const String&)Wide(std::u16string(1, unit)));
        bool escaped = c < 0x20 || c == 0x7F || std::strchr(" \"%*/:<>?\\|", static_cast<int>(c)) != nullptr;
        if (escaped) {
            static const char Hex[] = "0123456789ABCDEF";
            std::u16string expected{u'%', static_cast<char16_t>(Hex[c >> 4]), static_cast<char16_t>(Hex[c & 15])};
            Require(NameUnits(encoded) == expected);
        } else {
            Require(NameUnits(encoded) == std::u16string(1, unit));
        }
        Require(NameIs8Bit(encoded));
    }

    Text(EncodeName(&(const String&)NameString("a b/c%")), "a%20b%2Fc%25");
    Text(EncodeName(&(const String&)NameString("#+.~ -_")), "#+.~%20-_");
    Text(EncodeName(&(const String&)NameString("a\\b|c:d*e?f<g>h\"i")), "a%5Cb%7Cc%3Ad%2Ae%3Ff%3Cg%3Eh%22i");

    auto accented = Wide(u"café");
    Require(!NameIs8Bit(accented));
    auto accentedEncoded = EncodeName(&accented);
    Require(NameUnits(accentedEncoded) == u"café" && NameIs8Bit(accentedEncoded));
    Require(accentedEncoded.impl != accented.impl);

    const unsigned char latin1Source[] = {'c', 'a', 'f', 0xE9, '/'};
    auto latin1Bytes = Create8(latin1Source, 5);
    auto latin1Encoded = EncodeName(&latin1Bytes);
    Require(NameUnits(latin1Encoded) == u"café%2F" && NameIs8Bit(latin1Encoded));
    auto latin1Decoded = DecodeName(&latin1Encoded);
    Require(NameUnits(latin1Decoded) == u"café/" && NameIs8Bit(latin1Decoded));

    auto wide = Wide(u"a中 b");
    auto wideEncoded = EncodeName(&wide);
    Require(NameUnits(wideEncoded) == u"a中%20b" && !NameIs8Bit(wideEncoded));

    auto paired = Wide(u"\xD83D\xDE00");
    auto pairedEncoded = EncodeName(&paired);
    Require(NameUnits(pairedEncoded) == u"\xD83D\xDE00" && !NameIs8Bit(pairedEncoded));

    auto loneLeadEnd = Wide(u"a\xD83D");
    Require(NameUnits(EncodeName(&loneLeadEnd)) == u"a%+D83D");
    auto loneLeadMiddle = Wide(u"\xD83D" u"a");
    Require(NameUnits(EncodeName(&loneLeadMiddle)) == u"%+D83Da");
    auto loneTrailStart = Wide(u"\xDE00" u"a");
    Require(NameUnits(EncodeName(&loneTrailStart)) == u"%+DE00a");
    auto loneTrailMiddle = Wide(u"a\xDE00");
    Require(NameUnits(EncodeName(&loneTrailMiddle)) == u"a%+DE00");
    auto doubleLead = Wide(u"\xD83D\xD83D\xDE00");
    Require(NameUnits(EncodeName(&doubleLead)) == u"%+D83D\xD83D\xDE00");
    auto doubleTrail = Wide(u"\xD83D\xDE00\xDE00");
    Require(NameUnits(EncodeName(&doubleTrail)) == u"\xD83D\xDE00%+DE00");
    auto reversed = Wide(u"\xDE00\xD83D");
    Require(NameUnits(EncodeName(&reversed)) == u"%+DE00%+D83D");

    auto latinWide = Wide(u"a/é");
    Require(!NameIs8Bit(latinWide) && !NameIs8Bit(wide) && !NameIs8Bit(paired));
    auto latinWideEncoded = EncodeName(&latinWide);
    Require(NameUnits(latinWideEncoded) == u"a%2Fé" && NameIs8Bit(latinWideEncoded));
    auto highByte = Wide(u"\x00ff\x0100");
    Require(NameUnits(EncodeName(&highByte)) == u"\x00ff\x0100");

    String null;
    Require(EncodeName(&null).impl == nullptr);
    auto empty = NameString("");
    auto emptyEncoded = EncodeName(&empty);
    Require(emptyEncoded.impl == empty.impl && empty.impl->length == 0);
    ExpectErr([&] { EncodeName(nullptr); });

    StringImpl privateEmpty{4, 0, nullptr, 0};
    String privateHolder(&privateEmpty);
    Require(EncodeName(&privateHolder).impl == &privateEmpty);
    Require(DecodeName(&privateHolder).impl == &privateEmpty);
    Require(LastComponent(&privateHolder).impl == &privateEmpty);
    Require(privateEmpty.refCount == 4);

    auto plain = NameString("name.txt");
    auto plainEncoded = EncodeName(&plain);
    Text(plainEncoded, "name.txt");
    Require(plainEncoded.impl != plain.impl);

    auto roundTrip = Wide(u"a b/\x01\x7Fé中\xD83D\xDE00\xD83D%\xDE00");
    auto roundTripEncoded = EncodeName(&roundTrip);
    Require(NameUnits(DecodeName(&roundTripEncoded)) == NameUnits(roundTrip));

    Text(DecodeName(&(const String&)NameString("a%20b%2fc%2Fd")), "a b/c/d");
    Text(DecodeName(&(const String&)NameString("%41")), "A");
    Text(DecodeName(&(const String&)NameString("%+0041")), "A");
    Text(DecodeName(&(const String&)NameString("plain+text")), "plain+text");
    Text(DecodeName(&(const String&)NameString("%41%42")), "AB");
    auto e9 = DecodeName(&(const String&)NameString("%+00e9"));
    Require(NameUnits(e9) == u"é" && NameIs8Bit(e9));
    auto latin1 = DecodeName(&(const String&)NameString("%E9"));
    Require(NameUnits(latin1) == u"é" && NameIs8Bit(latin1));
    auto cjk = DecodeName(&(const String&)NameString("x%+4E2Dy"));
    Require(NameUnits(cjk) == u"x中y" && !NameIs8Bit(cjk));
    auto lone = DecodeName(&(const String&)NameString("%+d83D"));
    Require(NameUnits(lone) == u"\xD83D");
    auto nul = DecodeName(&(const String&)NameString("a%00b"));
    Require(NameUnits(nul) == std::u16string(u"a\0b", 3));
    auto widePlain = Wide(u"中%41");
    Require(NameUnits(DecodeName(&widePlain)) == u"中A");

    for (const char* invalid : {"%", "%4", "a%4", "%G1", "%1G", "%+", "%+4", "%+41", "%+004", "%+00G1", "%+G041", "%+0G41", "%+00", "x%", "x%4"}) {
        auto result = DecodeName(&(const String&)NameString(invalid));
        Require(result.impl == nullptr);
    }
    auto truncatedInSharedParent = [&](const char* parentText, unsigned length) {
        auto parent = NameString(parentText);
        auto part = SharedSubstring(&parent, 0, length);
        Require((part.impl->hashAndFlags & 3) == 2 && part.impl->length == length);
        Require(DecodeName(&part).impl == nullptr);
    };
    truncatedInSharedParent("aaaaaaaaaaaaaaaa%41", 18);
    truncatedInSharedParent("aaaaaaaaaaaaaaaa%+0041", 17);
    truncatedInSharedParent("aaaaaaaaaaaaaaaa%+0041", 19);
    truncatedInSharedParent("aaaaaaaaaaaaaaaa%+4141", 20);

    auto invalidWide = Wide(u"中%4");
    Require(DecodeName(&invalidWide).impl == nullptr);
    auto nonHexWide = Wide(u"%١٢");
    Require(DecodeName(&nonHexWide).impl == nullptr);

    Require(DecodeName(&null).impl == nullptr);
    auto emptyDecoded = DecodeName(&empty);
    Require(emptyDecoded.impl == empty.impl);
    ExpectErr([&] { DecodeName(nullptr); });
    auto unchanged = NameString("abc");
    Require(DecodeName(&unchanged).impl != unchanged.impl);

    auto sameImpl = [&](const char* path) {
        auto input = NameString(path);
        auto result = LastComponent(&input);
        Require(result.impl == input.impl);
    };
    sameImpl("abc");
    sameImpl("");
    Require(LastComponent(&null).impl == nullptr);

    Text(LastComponent(&(const String&)NameString("/abc")), "abc");
    Text(LastComponent(&(const String&)NameString("a/b/c")), "c");
    Text(LastComponent(&(const String&)NameString("a/b/")), "b");
    Text(LastComponent(&(const String&)NameString("abc/")), "abc");
    Text(LastComponent(&(const String&)NameString("a/")), "a");
    Text(LastComponent(&(const String&)NameString("x/abc/")), "abc");
    Text(LastComponent(&(const String&)NameString("/a/")), "a");
    Text(LastComponent(&(const String&)NameString("/")), "");
    Text(LastComponent(&(const String&)NameString("//")), "");
    Text(LastComponent(&(const String&)NameString("a//")), "");
    Text(LastComponent(&(const String&)NameString("///")), "");
    Text(LastComponent(&(const String&)NameString("a/b//")), "");
    Text(LastComponent(&(const String&)NameString("dir/file.txt")), "file.txt");
    sameImpl("a\\b\\c");
    sameImpl("a\\b\\c\\");

    auto widePath = Wide(u"中/文/");
    auto wideLast = LastComponent(&widePath);
    Require(NameUnits(wideLast) == u"文" && !NameIs8Bit(wideLast));
    auto wideNoSlash = Wide(u"中文");
    Require(LastComponent(&wideNoSlash).impl == wideNoSlash.impl);
    auto wideTail = Wide(u"a/文");
    Require(NameUnits(LastComponent(&wideTail)) == u"文");
    ExpectErr([&] { LastComponent(nullptr); });
}

void TestFileSystemRepresentations() {
    const auto Representation = &_ZN3WTF14FileSystemImpl24fileSystemRepresentationERKNS_6StringE;
    const auto FromRepresentation = &_ZN3WTF14FileSystemImpl34stringFromFileSystemRepresentationEPKc;
    const auto Append = &_ZN3WTF14FileSystemImpl25pathByAppendingComponentsENS_10StringViewERKNS_6VectorIS1_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE;

    Require(Bytes(Representation(&(const String&)NameString("/data/a b"))) == "/data/a b");
    const unsigned char latin1Path[] = {'/', 'c', 'a', 'f', 0xE9};
    auto latin1 = Create8(latin1Path, 5);
    Require(Bytes(Representation(&latin1)) == "/caf\xC3\xA9");
    Require(Bytes(Representation(&(const String&)Wide(u"/a/caf\u00E9/\u4E2D"))) == "/a/caf\xC3\xA9/\xE4\xB8\xAD");
    Require(Bytes(Representation(&(const String&)Wide(u"x\xD800y"))) == "x\xED\xA0\x80y");
    String nullString;
    CString nullRepresentation = Representation(&nullString);
    Require(nullRepresentation.buffer && Bytes(nullRepresentation).empty());
    ExpectErr([&] { Representation(nullptr); });

    Require(!FromRepresentation(nullptr).impl);
    auto fromUTF8 = FromRepresentation("/a/caf\xC3\xA9/\xE4\xB8\xAD");
    Require(NameUnits(fromUTF8) == u"/a/caf\u00E9/\u4E2D");
    Require(!FromRepresentation("bad\xFF").impl);
    auto empty = FromRepresentation("");
    Require(empty.impl && !empty.impl->length);

    NKWebKit::WtfAbi::Vector<StringView> components(3);
    components.buffer[0] = Literal("b");
    components.buffer[1] = Literal("");
    components.buffer[2] = Literal("c.txt");
    components.size = 3;
    auto joined = Append(Literal("/a"), &components);
    Require(NameUnits(joined) == u"/a/b//c.txt" && NameIs8Bit(joined));
    auto trailing = Append(Literal("/a/"), &components);
    Require(NameUnits(trailing) == u"/a//b//c.txt");
    auto relative = Append(Literal(""), &components);
    Require(NameUnits(relative) == u"/b//c.txt");

    NKWebKit::WtfAbi::Vector<StringView> none;
    auto alone = Append(Literal("/only"), &none);
    Require(NameUnits(alone) == u"/only" && NameIs8Bit(alone));
    auto nothing = Append(Literal(""), &none);
    Require(nothing.impl && !nothing.impl->length && NameIs8Bit(nothing));
    auto nullPath = Append(StringView{nullptr, 0, true}, &none);
    Require(nullPath.impl && !nullPath.impl->length);

    const std::u16string widePath = u"/\u4E2D";
    auto wideJoined = Append(StringView{widePath.data(), 2, false}, &components);
    Require(NameUnits(wideJoined) == u"/\u4E2D/b//c.txt" && !NameIs8Bit(wideJoined));
    const std::u16string latinUnit = u"\u00E9";
    auto latinAlone = Append(StringView{latinUnit.data(), 1, false}, &none);
    Require(NameUnits(latinAlone) == u"\u00E9" && NameIs8Bit(latinAlone));
    const std::u16string emptyWide;
    auto emptyWideAlone = Append(StringView{emptyWide.data(), 0, false}, &none);
    Require(NameIs8Bit(emptyWideAlone));
    NKWebKit::WtfAbi::Vector<StringView> wideComponent(1);
    wideComponent.buffer[0] = StringView{latinUnit.data(), 1, false};
    wideComponent.size = 1;
    auto upconverted = Append(Literal("/a"), &wideComponent);
    Require(NameUnits(upconverted) == u"/a/\u00E9" && !NameIs8Bit(upconverted));
    wideComponent.buffer[0] = StringView{emptyWide.data(), 0, false};
    auto emptyUpconverted = Append(Literal("/a"), &wideComponent);
    Require(NameUnits(emptyUpconverted) == u"/a/" && !NameIs8Bit(emptyUpconverted));

    wideComponent.buffer[0] = StringView{nullptr, 1, true};
    ExpectErr([&] { Append(Literal("/a"), &wideComponent); });
    ExpectErr([&] { Append(Literal("/a"), nullptr); });
}

const auto ParserEqual = &_ZN3WTF9URLParser14allValuesEqualERKNS_3URLES3_;
const auto ParserConsistent = &_ZN3WTF9URLParser24internalValuesConsistentERKNS_3URLE;

void TestURLParserChecks() {
    constexpr unsigned valid = 1U;
    constexpr unsigned http = 2U;
    constexpr unsigned opaque = 4U;
    auto text = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#frag"));
    const std::string raw = Bytes(UTF8(&text, 1));
    const unsigned hostEnd = static_cast<unsigned>(raw.find(":8443"));
    URL url{text.impl, valid | (5U << 3) | (5U << 6), 8, 12, 17, hostEnd,
        static_cast<unsigned>(raw.rfind('/') + 1), static_cast<unsigned>(raw.find('?')), static_cast<unsigned>(raw.find('#'))};
    Require(ParserEqual(&url, &url) && ParserConsistent(&url));

    auto copy = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#frag"));
    Require(copy.impl != text.impl);
    URL same = url;
    same.impl = copy.impl;
    Require(ParserEqual(&url, &same) && ParserEqual(&same, &url));
    auto wide = Create16(u"https://u%20:p%40@example.test:8443/a/b?q=1#frag", 48);
    Require(!(wide.impl->hashAndFlags & 4));
    same.impl = wide.impl;
    Require(ParserEqual(&url, &same) && ParserEqual(&same, &url));
    auto other = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#frog"));
    same.impl = other.impl;
    Require(!ParserEqual(&url, &same) && ParserConsistent(&same));
    auto shorter = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#fra"));
    same.impl = shorter.impl;
    Require(!ParserEqual(&url, &same));

    for (unsigned flip : {valid, http, opaque, 1U << 3, 4U << 3, 1U << 6, 1U << 31}) {
        URL changed = url;
        changed.bits ^= flip;
        Require(!ParserEqual(&url, &changed) && !ParserEqual(&changed, &url));
    }
    unsigned URL::* const offsets[] = {&URL::userStart, &URL::userEnd, &URL::passwordEnd, &URL::hostEnd,
        &URL::pathAfterLastSlash, &URL::pathEnd, &URL::queryEnd};
    for (auto offset : offsets) {
        URL changed = url;
        changed.*offset += 1;
        Require(!ParserEqual(&url, &changed) && !ParserEqual(&changed, &url));
    }

    URL absent{};
    URL absentCopy{};
    Require(ParserEqual(&absent, &absentCopy) && ParserConsistent(&absent));
    auto empty = FromUTF8Z(U8(""));
    Require(empty.impl && empty.impl->length == 0);
    URL emptyURL{empty.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    Require(!ParserEqual(&absent, &emptyURL) && !ParserEqual(&emptyURL, &absent));
    Require(ParserConsistent(&emptyURL));
    absent.queryEnd = 1;
    Require(!ParserConsistent(&absent));

    {
        URL changed = url;
        changed.bits = (changed.bits & 63U) | ((changed.userStart + 1) << 6);
        Require(!ParserConsistent(&changed));
        changed.bits = (changed.bits & 63U) | (changed.userStart << 6);
        Require(ParserConsistent(&changed));
    }
    const std::pair<unsigned URL::*, unsigned URL::*> orders[] = {{&URL::userStart, &URL::userEnd},
        {&URL::userEnd, &URL::passwordEnd}, {&URL::passwordEnd, &URL::hostEnd}, {&URL::pathAfterLastSlash, &URL::pathEnd},
        {&URL::pathEnd, &URL::queryEnd}};
    for (const auto& [lower, upper] : orders) {
        URL changed = url;
        changed.*lower = changed.*upper + 1;
        Require(!ParserConsistent(&changed));
        changed.*lower = changed.*upper;
        Require(ParserConsistent(&changed));
    }
    {
        URL changed = url;
        changed.pathAfterLastSlash = changed.hostEnd + 4;
        Require(!ParserConsistent(&changed));
        changed.pathAfterLastSlash = changed.hostEnd + 5;
        Require(ParserConsistent(&changed));
        changed.bits &= ~(7U << 3);
        changed.pathAfterLastSlash = changed.hostEnd;
        Require(ParserConsistent(&changed));
    }
    {
        URL changed = url;
        changed.queryEnd = text.impl->length;
        Require(ParserConsistent(&changed));
        changed.queryEnd = text.impl->length + 1;
        Require(!ParserConsistent(&changed));
        changed.queryEnd = url.queryEnd;
        changed.bits &= ~(valid | http | opaque);
        Require(ParserConsistent(&changed));
        changed.bits |= http | opaque;
        Require(ParserConsistent(&changed));
    }
    {
        auto abc = FromUTF8Z(U8("abc"));
        URL wrapped{abc.impl, valid | (1U << 3), 0, 0, 0, 0xFFFFFFFFU, 0, 0, 0};
        Require(ParserConsistent(&wrapped));
        wrapped.bits = valid | (2U << 3);
        Require(!ParserConsistent(&wrapped));
        wrapped.pathAfterLastSlash = 1;
        wrapped.pathEnd = 1;
        wrapped.queryEnd = 1;
        Require(ParserConsistent(&wrapped));
    }

    StringImpl corrupt{2, 3, nullptr, 4};
    StringImpl corruptCopy{2, 3, nullptr, 4};
    URL broken{&corrupt, 0, 0, 0, 0, 0, 0, 0, 0};
    URL brokenCopy{&corruptCopy, 0, 0, 0, 0, 0, 0, 0, 0};
    ExpectErr([&] { ParserEqual(&broken, &brokenCopy); });
    Require(ParserEqual(&broken, &broken));
    ExpectErr([&] { ParserEqual(nullptr, &url); });
    ExpectErr([&] { ParserEqual(&url, nullptr); });
    ExpectErr([&] { ParserConsistent(nullptr); });
}

void TestURLsAndReallocation() {
    struct PinnedBits {
        unsigned valid : 1;
        unsigned http : 1;
        unsigned cannotBeBase : 1;
        unsigned portLength : 3;
        unsigned schemeEnd : 26;
    };
    static_assert(sizeof(PinnedBits) == 4);
    PinnedBits fields{1, 1, 0, 5, 5};
    unsigned bits;
    std::memcpy(&bits, &fields, sizeof(bits));
    Require(bits == (1U | 2U | (5U << 3) | (5U << 6)));
    auto string = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#frag"));
    URL url{string.impl, bits, 8, 12, 17, 29, 37, 38, 42};
    std::string raw = Bytes(UTF8(&string, 1));
    url.hostEnd = static_cast<unsigned>(raw.find(":8443"));
    url.pathAfterLastSlash = static_cast<unsigned>(raw.rfind('/') + 1);
    url.pathEnd = static_cast<unsigned>(raw.find('?'));
    url.queryEnd = static_cast<unsigned>(raw.find('#'));
    Require(ViewText(URLProtocol(&url)) == "https");
    Require(ViewText(URLHost(&url)) == "example.test");
    Require(ViewText(URLUser(&url)) == "u%20" && ViewText(URLPassword(&url)) == "p%40");
    Require(ViewText(URLPath(&url)) == "/a/b");
    Require(ViewText(URLQuery(&url)) == "?q=1");
    Require(ViewText(URLFragment(&url)) == "frag" && ViewText(URLFragmentSign(&url)) == "#frag");
    Require(URLIsProtocol(&url, {"https", 5, true}) && !URLIsProtocol(&url, {"http", 4, true}));
    Require(URLHierarchical(&url) && !URLBlank(&url) && !URLSrcdoc(&url));
    auto wideString = Create16(u"http://wide.test/path", 21);
    URL wide{wideString.impl, 1U | 2U | (4U << 6), 7, 7, 7, 16, 17, 21, 21};
    Require(ViewText(URLProtocol(&wide)) == "http" && !URLHost(&wide).is8Bit);
    Require(ViewText(URLHost(&wide)) == "wide.test" && ViewText(URLPath(&wide)) == "/path");
    auto about = FromUTF8Z(U8("about:blank#"));
    URL blank{about.impl, 1U | 4U | (5U << 6), 6, 6, 6, 6, 6, 11, 11};
    Require(URLBlank(&blank) && !URLSrcdoc(&blank) && !URLHierarchical(&blank));
    Require(URLFragment(&blank).data && URLFragment(&blank).length == 0);
    auto src = FromUTF8Z(U8("about:srcdoc"));
    blank.impl = src.impl; blank.pathEnd = 12; blank.queryEnd = 12;
    Require(URLSrcdoc(&blank) && !URLBlank(&blank));
    URL absent{};
    Require(!URLProtocol(&absent).data && !URLHost(&absent).data && !URLPath(&absent).data);
    Require(!URLPassword(&absent).data && !URLQuery(&absent).data && !URLFragment(&absent).data);
    Require(URLUser(&absent).data && URLUser(&absent).length == 0);
    Require(!URLHierarchical(&absent) && !URLIsProtocol(&absent, {"", 0, true}));
    auto js = FromUTF8Z(U8(" \tJaVa\nScRiPt:alert(1)"));
    URL javascript{js.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    Require(URLJavaScript(&javascript));
    Require(IsJavaScript({js.impl->data, js.impl->length, true}));
    Require(!IsJavaScript({"javascriptx:", 12, true}) && !IsJavaScript({nullptr, 0, true}));
    Require(IsHTTP({"HTTP:", 5, true}) && IsHTTP({u"HtTpS:", 6, false}));
    Require(!IsHTTP({" https:", 7, true}) && !IsHTTP({"httpx:", 6, true}));
    Require(URLEqualIgnoringFragment(&url, &url));
    auto urlDiffFragStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#other"));
    URL urlDiffFrag = url;
    urlDiffFrag.impl = urlDiffFragStr.impl;
    std::string rawDiffFrag = Bytes(UTF8(&urlDiffFragStr, 1));
    urlDiffFrag.queryEnd = static_cast<unsigned>(rawDiffFrag.find('#'));
    Require(URLEqualIgnoringFragment(&url, &urlDiffFrag));
    auto urlNoFragStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1"));
    URL urlNoFrag = url;
    urlNoFrag.impl = urlNoFragStr.impl;
    std::string rawNoFrag = Bytes(UTF8(&urlNoFragStr, 1));
    urlNoFrag.queryEnd = static_cast<unsigned>(rawNoFrag.length());
    Require(URLEqualIgnoringFragment(&url, &urlNoFrag));
    auto urlEmptyFragStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#"));
    URL urlEmptyFrag = url;
    urlEmptyFrag.impl = urlEmptyFragStr.impl;
    std::string rawEmptyFrag = Bytes(UTF8(&urlEmptyFragStr, 1));
    urlEmptyFrag.queryEnd = static_cast<unsigned>(rawEmptyFrag.find('#'));
    Require(URLEqualIgnoringFragment(&url, &urlEmptyFrag));
    Require(URLEqualIgnoringFragment(&urlNoFrag, &urlEmptyFrag));
    auto aboutNoFrag = FromUTF8Z(U8("about:blank"));
    URL blankAbout{about.impl, 1U | 4U | (5U << 6), 6, 6, 6, 6, 6, 11, 11};
    blankAbout.queryEnd = static_cast<unsigned>(Bytes(UTF8(&about, 1)).find('#'));
    URL blankNoFrag = blankAbout;
    blankNoFrag.impl = aboutNoFrag.impl;
    blankNoFrag.queryEnd = static_cast<unsigned>(Bytes(UTF8(&aboutNoFrag, 1)).length());
    Require(URLEqualIgnoringFragment(&blankAbout, &blankNoFrag));
    auto urlDiffQueryStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=2#frag"));
    URL urlDiffQuery = url;
    urlDiffQuery.impl = urlDiffQueryStr.impl;
    std::string rawDiffQuery = Bytes(UTF8(&urlDiffQueryStr, 1));
    urlDiffQuery.queryEnd = static_cast<unsigned>(rawDiffQuery.find('#'));
    Require(!URLEqualIgnoringFragment(&url, &urlDiffQuery));
    auto urlDiffPathStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/c?q=1#frag"));
    URL urlDiffPath = url;
    urlDiffPath.impl = urlDiffPathStr.impl;
    std::string rawDiffPath = Bytes(UTF8(&urlDiffPathStr, 1));
    urlDiffPath.queryEnd = static_cast<unsigned>(rawDiffPath.find('#'));
    Require(!URLEqualIgnoringFragment(&url, &urlDiffPath));
    auto urlDiffHostStr = FromUTF8Z(U8("https://u%20:p%40@other.test:8443/a/b?q=1#frag"));
    URL urlDiffHost = url;
    urlDiffHost.impl = urlDiffHostStr.impl;
    std::string rawDiffHost = Bytes(UTF8(&urlDiffHostStr, 1));
    urlDiffHost.queryEnd = static_cast<unsigned>(rawDiffHost.find('#'));
    Require(!URLEqualIgnoringFragment(&url, &urlDiffHost));
    auto wideHash = [](const StringImpl* impl) {
        if (!impl || !impl->data) return 0U;
        const auto* chars = static_cast<const char16_t*>(impl->data);
        for (unsigned i = 0; i < impl->length; ++i) {
            if (chars[i] == u'#') return i;
        }
        return impl->length;
    };
    auto wide1 = Create16(u"http://wide.test/path#sec1", 26);
    auto wide2 = Create16(u"http://wide.test/path#sec2", 26);
    URL urlWide1 = wide;
    urlWide1.impl = wide1.impl;
    urlWide1.queryEnd = wideHash(urlWide1.impl);
    URL urlWide2 = wide;
    urlWide2.impl = wide2.impl;
    urlWide2.queryEnd = wideHash(urlWide2.impl);
    Require(URLEqualIgnoringFragment(&urlWide1, &urlWide2));
    auto wideNoFrag = Create16(u"http://wide.test/path", 21);
    URL urlWideNoFrag = wide;
    urlWideNoFrag.impl = wideNoFrag.impl;
    urlWideNoFrag.queryEnd = wideHash(urlWideNoFrag.impl);
    auto narrow8Str = FromUTF8Z(U8("http://wide.test/path#other"));
    URL urlNarrow8 = wide;
    urlNarrow8.impl = narrow8Str.impl;
    std::string rawNarrow8 = Bytes(UTF8(&narrow8Str, 1));
    urlNarrow8.queryEnd = static_cast<unsigned>(rawNarrow8.find('#'));
    Require(URLEqualIgnoringFragment(&urlWide1, &urlNarrow8));
    Require(URLEqualIgnoringFragment(&urlNarrow8, &urlWideNoFrag));
    auto narrow8DiffStr = FromUTF8Z(U8("http://wide.test/other#sec1"));
    URL urlNarrow8Diff = wide;
    urlNarrow8Diff.impl = narrow8DiffStr.impl;
    std::string rawNarrow8Diff = Bytes(UTF8(&narrow8DiffStr, 1));
    urlNarrow8Diff.queryEnd = static_cast<unsigned>(rawNarrow8Diff.find('#'));
    Require(!URLEqualIgnoringFragment(&urlWide1, &urlNarrow8Diff));
    Require(URLEqualIgnoringFragment(&absent, &absent));
    Require(URLEqualIgnoringFragment(&javascript, &javascript));
    Require(!URLEqualIgnoringFragment(&url, &absent));
    Require(!URLEqualIgnoringFragment(&absent, &url));
    auto jsFrag1 = FromUTF8Z(U8(" \tJaVa\nScRiPt:alert(1)#x"));
    auto jsFrag2 = FromUTF8Z(U8(" \tJaVa\nScRiPt:alert(1)#y"));
    URL urlJsFrag1{jsFrag1.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    std::string rawJsFrag1 = Bytes(UTF8(&jsFrag1, 1));
    urlJsFrag1.queryEnd = static_cast<unsigned>(rawJsFrag1.find('#'));
    URL urlJsFrag2{jsFrag2.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    std::string rawJsFrag2 = Bytes(UTF8(&jsFrag2, 1));
    urlJsFrag2.queryEnd = static_cast<unsigned>(rawJsFrag2.find('#'));
    Require(!URLEqualIgnoringFragment(&urlJsFrag1, &urlJsFrag2));
    Require(!URLEqualIgnoringFragment(&absent, &javascript));
    ExpectErr([&] { URLEqualIgnoringFragment(nullptr, &url); });
    ExpectErr([&] { URLEqualIgnoringFragment(&url, nullptr); });
    auto urlNoPortStr = FromUTF8Z(U8("http://example.test/path"));
    URL urlNoPort{urlNoPortStr.impl, 1U | 2U | (0U << 3) | (4U << 6), 7, 7, 7, 19, 20, 24, 24};
    Require(URLPortAllowed(&urlNoPort));
    auto urlPort80Str = FromUTF8Z(U8("http://example.test:80/path"));
    URL urlPort80{urlPort80Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(URLPortAllowed(&urlPort80));
    auto urlPort443Str = FromUTF8Z(U8("http://example.test:443/path"));
    URL urlPort443{urlPort443Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(URLPortAllowed(&urlPort443));
    auto urlPort8080Str = FromUTF8Z(U8("http://example.test:8080/path"));
    URL urlPort8080{urlPort8080Str.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 19, 25, 29, 29};
    Require(URLPortAllowed(&urlPort8080));
    auto urlPort0Str = FromUTF8Z(U8("http://example.test:0/path"));
    URL urlPort0{urlPort0Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Require(URLPortAllowed(&urlPort0));
    auto urlPort10081Str = FromUTF8Z(U8("http://example.test:10081/path"));
    URL urlPort10081{urlPort10081Str.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Require(URLPortAllowed(&urlPort10081));
    auto urlPort1Str = FromUTF8Z(U8("http://example.test:1/path"));
    URL urlPort1{urlPort1Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Require(!URLPortAllowed(&urlPort1));
    auto urlPort7Str = FromUTF8Z(U8("http://example.test:7/path"));
    URL urlPort7{urlPort7Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Require(!URLPortAllowed(&urlPort7));
    auto urlPort21Str = FromUTF8Z(U8("http://example.test:21/path"));
    URL urlPort21{urlPort21Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(!URLPortAllowed(&urlPort21));
    auto urlPort22Str = FromUTF8Z(U8("http://example.test:22/path"));
    URL urlPort22{urlPort22Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(!URLPortAllowed(&urlPort22));
    auto urlPort25Str = FromUTF8Z(U8("http://example.test:25/path"));
    URL urlPort25{urlPort25Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(!URLPortAllowed(&urlPort25));
    auto urlPort87Str = FromUTF8Z(U8("http://example.test:87/path"));
    URL urlPort87{urlPort87Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(!URLPortAllowed(&urlPort87));
    auto urlPort10080Str = FromUTF8Z(U8("http://example.test:10080/path"));
    URL urlPort10080{urlPort10080Str.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Require(!URLPortAllowed(&urlPort10080));
    auto ftpPort20Str = FromUTF8Z(U8("ftp://example.test:20/path"));
    URL ftpPort20{ftpPort20Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(!URLPortAllowed(&ftpPort20));
    auto ftpPort21Str = FromUTF8Z(U8("ftp://example.test:21/path"));
    URL ftpPort21{ftpPort21Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(URLPortAllowed(&ftpPort21));
    auto ftpPort22Str = FromUTF8Z(U8("ftp://example.test:22/path"));
    URL ftpPort22{ftpPort22Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(URLPortAllowed(&ftpPort22));
    auto ftpPort25Str = FromUTF8Z(U8("ftp://example.test:25/path"));
    URL ftpPort25{ftpPort25Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(!URLPortAllowed(&ftpPort25));
    auto ftpUpper21Str = FromUTF8Z(U8("FTP://example.test:21/path"));
    URL ftpUpper21{ftpUpper21Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(URLPortAllowed(&ftpUpper21));
    auto ftpUpper22Str = FromUTF8Z(U8("FTP://example.test:22/path"));
    URL ftpUpper22{ftpUpper22Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(URLPortAllowed(&ftpUpper22));
    auto ftpUpper25Str = FromUTF8Z(U8("FTP://example.test:25/path"));
    URL ftpUpper25{ftpUpper25Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Require(!URLPortAllowed(&ftpUpper25));
    auto filePort1Str = FromUTF8Z(U8("file://example.test:1/path"));
    URL filePort1{filePort1Str.impl, 1U | 0U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Require(URLPortAllowed(&filePort1));
    auto filePort21Str = FromUTF8Z(U8("file://example.test:21/path"));
    URL filePort21{filePort21Str.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(URLPortAllowed(&filePort21));
    auto filePort25Str = FromUTF8Z(U8("file://example.test:25/path"));
    URL filePort25{filePort25Str.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(URLPortAllowed(&filePort25));
    auto filePort10080Str = FromUTF8Z(U8("file://example.test:10080/path"));
    URL filePort10080{filePort10080Str.impl, 1U | 0U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Require(URLPortAllowed(&filePort10080));
    auto fileUpper25Str = FromUTF8Z(U8("FILE://example.test:25/path"));
    URL fileUpper25{fileUpper25Str.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Require(URLPortAllowed(&fileUpper25));
    auto urlMinusPortStr = FromUTF8Z(U8("http://example.test:-80/path"));
    URL urlMinusPort{urlMinusPortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(URLPortAllowed(&urlMinusPort));
    auto urlOverflowPortStr = FromUTF8Z(U8("http://example.test:65536/path"));
    URL urlOverflowPort{urlOverflowPortStr.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Require(URLPortAllowed(&urlOverflowPort));
    auto urlBarePlusPortStr = FromUTF8Z(U8("http://example.test:+/path"));
    URL urlBarePlusPort{urlBarePlusPortStr.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Require(URLPortAllowed(&urlBarePlusPort));
    auto urlPlus25PortStr = FromUTF8Z(U8("http://example.test:+25/path"));
    URL urlPlus25Port{urlPlus25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlPlus25Port));
    auto urlSpace25PortStr = FromUTF8Z(U8("http://example.test: 25 /path"));
    URL urlSpace25Port{urlSpace25PortStr.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 19, 25, 29, 29};
    Require(!URLPortAllowed(&urlSpace25Port));
    auto urlTab25PortStr = FromUTF8Z(U8("http://example.test:\t25/path"));
    URL urlTab25Port{urlTab25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlTab25Port));
    auto urlLf25PortStr = FromUTF8Z(U8("http://example.test:\n25/path"));
    URL urlLf25Port{urlLf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlLf25Port));
    auto urlCr25PortStr = FromUTF8Z(U8("http://example.test:\r25/path"));
    URL urlCr25Port{urlCr25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlCr25Port));
    auto urlFf25PortStr = FromUTF8Z(U8("http://example.test:\f25/path"));
    URL urlFf25Port{urlFf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlFf25Port));
    auto urlTrailingTab25PortStr = FromUTF8Z(U8("http://example.test:25\t/path"));
    URL urlTrailingTab25Port{urlTrailingTab25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlTrailingTab25Port));
    auto urlTrailingLf25PortStr = FromUTF8Z(U8("http://example.test:25\n/path"));
    URL urlTrailingLf25Port{urlTrailingLf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlTrailingLf25Port));
    auto urlTrailingCr25PortStr = FromUTF8Z(U8("http://example.test:25\r/path"));
    URL urlTrailingCr25Port{urlTrailingCr25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlTrailingCr25Port));
    auto urlTrailingFf25PortStr = FromUTF8Z(U8("http://example.test:25\f/path"));
    URL urlTrailingFf25Port{urlTrailingFf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(!URLPortAllowed(&urlTrailingFf25Port));
    auto urlJunk25PortStr = FromUTF8Z(U8("http://example.test:25x/path"));
    URL urlJunk25Port{urlJunk25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Require(URLPortAllowed(&urlJunk25Port));
    auto urlJunk80PortStr = FromUTF8Z(U8("http://example.test:80junk/path"));
    URL urlJunk80Port{urlJunk80PortStr.impl, 1U | 2U | (7U << 3) | (4U << 6), 7, 7, 7, 19, 27, 31, 31};
    Require(URLPortAllowed(&urlJunk80Port));
    auto widePort8080 = Create16(u"http://wide.test:8080/path", 26);
    URL urlWidePort8080{widePort8080.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 16, 22, 26, 26};
    Require(URLPortAllowed(&urlWidePort8080));
    auto widePort25 = Create16(u"http://wide.test:25/path", 24);
    URL urlWidePort25{widePort25.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 16, 20, 24, 24};
    Require(!URLPortAllowed(&urlWidePort25));
    auto wideFtpPort21 = Create16(u"ftp://wide.test:21/path", 23);
    URL urlWideFtpPort21{wideFtpPort21.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 15, 19, 23, 23};
    Require(URLPortAllowed(&urlWideFtpPort21));
    auto wideFilePort25 = Create16(u"file://wide.test:25/path", 24);
    URL urlWideFilePort25{wideFilePort25.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 16, 20, 24, 24};
    Require(URLPortAllowed(&urlWideFilePort25));
    auto wideFtpUpper21 = Create16(u"FTP://wide.test:21/path", 23);
    URL urlWideFtpUpper21{wideFtpUpper21.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 15, 19, 23, 23};
    Require(URLPortAllowed(&urlWideFtpUpper21));
    auto wideFileUpper25 = Create16(u"FILE://wide.test:25/path", 24);
    URL urlWideFileUpper25{wideFileUpper25.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 16, 20, 24, 24};
    Require(URLPortAllowed(&urlWideFileUpper25));
    auto wideTab25 = Create16(u"http://wide.test:\t25/path", 25);
    URL urlWideTab25{wideTab25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideTab25));
    auto wideLf25 = Create16(u"http://wide.test:\n25/path", 25);
    URL urlWideLf25{wideLf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideLf25));
    auto wideCr25 = Create16(u"http://wide.test:\r25/path", 25);
    URL urlWideCr25{wideCr25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideCr25));
    auto wideFf25 = Create16(u"http://wide.test:\f25/path", 25);
    URL urlWideFf25{wideFf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideFf25));
    auto wideTrailingTab25 = Create16(u"http://wide.test:25\t/path", 25);
    URL urlWideTrailingTab25{wideTrailingTab25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideTrailingTab25));
    auto wideTrailingLf25 = Create16(u"http://wide.test:25\n/path", 25);
    URL urlWideTrailingLf25{wideTrailingLf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideTrailingLf25));
    auto wideTrailingCr25 = Create16(u"http://wide.test:25\r/path", 25);
    URL urlWideTrailingCr25{wideTrailingCr25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideTrailingCr25));
    auto wideTrailingFf25 = Create16(u"http://wide.test:25\f/path", 25);
    URL urlWideTrailingFf25{wideTrailingFf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(!URLPortAllowed(&urlWideTrailingFf25));
    auto wideJunk25 = Create16(u"http://wide.test:25x/path", 25);
    URL urlWideJunk25{wideJunk25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Require(URLPortAllowed(&urlWideJunk25));
    auto urlWraparoundPortStr = FromUTF8Z(U8("25://example.test/path"));
    URL urlWraparoundPort{urlWraparoundPortStr.impl, 1U | 2U | (3U << 3) | (2U << 6), 5, 5, 5, ~0U, 23, 22, 22};
    Require(URLPortAllowed(&urlWraparoundPort));
    Require(URLPortAllowed(&absent));
    ExpectErr([&] { URLPortAllowed(nullptr); });
    {
        Require(URLProtocolHostAndPortAreEqual(&url, &url));
        Require(URLProtocolHostAndPortAreEqual(&url, &urlNoFrag));
        Require(URLProtocolHostAndPortAreEqual(&url, &urlDiffQuery));
        Require(URLProtocolHostAndPortAreEqual(&url, &urlDiffPath));
        auto urlUpperSchemeStr = FromUTF8Z(U8("HTTPS://u%20:p%40@example.test:8443/a/b?q=1#frag"));
        URL urlUpperScheme = url;
        urlUpperScheme.impl = urlUpperSchemeStr.impl;
        Require(URLProtocolHostAndPortAreEqual(&url, &urlUpperScheme));
        auto urlUpperHostStr = FromUTF8Z(U8("https://u%20:p%40@EXAMPLE.TEST:8443/a/b?q=1#frag"));
        URL urlUpperHost = url;
        urlUpperHost.impl = urlUpperHostStr.impl;
        Require(URLProtocolHostAndPortAreEqual(&url, &urlUpperHost));
        auto urlHttpStr = FromUTF8Z(U8("http://u%20:p%40@example.test:8443/a/b?q=1#frag"));
        URL urlHttp = url;
        urlHttp.impl = urlHttpStr.impl;
        urlHttp.bits = 1U | 2U | (5U << 3) | (4U << 6);
        Require(!URLProtocolHostAndPortAreEqual(&url, &urlHttp));
        Require(!URLProtocolHostAndPortAreEqual(&url, &urlDiffHost));
        auto urlShortHostStr = FromUTF8Z(U8("https://u%20:p%40@ex.test:8443/a/b?q=1#frag"));
        URL urlShortHost = url;
        urlShortHost.impl = urlShortHostStr.impl;
        urlShortHost.hostEnd = 25;
        Require(!URLProtocolHostAndPortAreEqual(&url, &urlShortHost));
        auto urlDiffPortStr = FromUTF8Z(U8("https://u%20:p%40@example.test:9443/a/b?q=1#frag"));
        URL urlDiffPort = url;
        urlDiffPort.impl = urlDiffPortStr.impl;
        Require(!URLProtocolHostAndPortAreEqual(&url, &urlDiffPort));
        auto urlPort80Str = FromUTF8Z(U8("http://example.test:80/path"));
        URL urlPort80{urlPort80Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
        auto urlNoPortStr = FromUTF8Z(U8("http://example.test/path"));
        URL urlNoPort{urlNoPortStr.impl, 1U | 2U | (0U << 3) | (4U << 6), 7, 7, 7, 19, 20, 24, 24};
        Require(!URLProtocolHostAndPortAreEqual(&urlPort80, &urlNoPort));
        auto urlNoPort2Str = FromUTF8Z(U8("http://example.test/other"));
        URL urlNoPort2{urlNoPort2Str.impl, 1U | 2U | (0U << 3) | (4U << 6), 7, 7, 7, 19, 20, 25, 25};
        Require(URLProtocolHostAndPortAreEqual(&urlNoPort, &urlNoPort2));
        auto widePort = Create16(u"http://wide.test:8080/path", 26);
        URL urlWidePort{widePort.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 16, 22, 26, 26};
        auto widePortMatch = Create16(u"HTTP://WIDE.TEST:8080/other", 27);
        URL urlWideMatch{widePortMatch.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 16, 22, 27, 27};
        Require(URLProtocolHostAndPortAreEqual(&urlWidePort, &urlWideMatch));
        auto narrowPortStr = FromUTF8Z(U8("http://wide.test:8080/narrow"));
        URL urlNarrowPort{narrowPortStr.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 16, 22, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlWidePort, &urlNarrowPort));
        auto urlPort0Str = FromUTF8Z(U8("http://example.test:0/path"));
        URL urlPort0{urlPort0Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
        Require(URLProtocolHostAndPortAreEqual(&urlPort0, &urlPort0));
        Require(!URLProtocolHostAndPortAreEqual(&urlPort0, &urlNoPort));
        Require(!URLProtocolHostAndPortAreEqual(&urlPort0, &urlPort80));
        auto urlPortMaxStr = FromUTF8Z(U8("http://example.test:65535/path"));
        URL urlPortMax{urlPortMaxStr.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
        Require(URLProtocolHostAndPortAreEqual(&urlPortMax, &urlPortMax));
        Require(!URLProtocolHostAndPortAreEqual(&urlPortMax, &urlPort80));
        auto urlEmptyPortStr = FromUTF8Z(U8("http://example.test:/path"));
        URL urlEmptyPort{urlEmptyPortStr.impl, 1U | 2U | (1U << 3) | (4U << 6), 7, 7, 7, 19, 21, 25, 25};
        Require(!URLProtocolHostAndPortAreEqual(&urlEmptyPort, &urlPort80));
        Require(URLProtocolHostAndPortAreEqual(&urlEmptyPort, &urlNoPort));
        auto urlMalformedPortStr = FromUTF8Z(U8("http://example.test:8x/path"));
        URL urlMalformedPort{urlMalformedPortStr.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
        Require(!URLProtocolHostAndPortAreEqual(&urlMalformedPort, &urlPort80));
        Require(URLProtocolHostAndPortAreEqual(&urlMalformedPort, &urlNoPort));
        auto urlOutOfRangePortStr = FromUTF8Z(U8("http://example.test:70000/path"));
        URL urlOutOfRangePort{urlOutOfRangePortStr.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
        Require(!URLProtocolHostAndPortAreEqual(&urlOutOfRangePort, &urlPort80));
        Require(URLProtocolHostAndPortAreEqual(&urlOutOfRangePort, &urlNoPort));
        auto urlPlusPort80Str = FromUTF8Z(U8("http://example.test:+80/path"));
        URL urlPlusPort80{urlPlusPort80Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlPlusPort80, &urlPort80));
        Require(!URLProtocolHostAndPortAreEqual(&urlPlusPort80, &urlNoPort));
        auto urlSpacePort80Str = FromUTF8Z(U8("http://example.test: 80 /path"));
        URL urlSpacePort80{urlSpacePort80Str.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 19, 25, 29, 29};
        Require(URLProtocolHostAndPortAreEqual(&urlSpacePort80, &urlPort80));
        Require(!URLProtocolHostAndPortAreEqual(&urlSpacePort80, &urlNoPort));
        auto urlTabPort80Str = FromUTF8Z(U8("http://example.test:\t80/path"));
        URL urlTabPort80{urlTabPort80Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlTabPort80, &urlPort80));
        auto urlNewlinePort80Str = FromUTF8Z(U8("http://example.test:\n80/path"));
        URL urlNewlinePort80{urlNewlinePort80Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlNewlinePort80, &urlPort80));
        auto urlReturnPort80Str = FromUTF8Z(U8("http://example.test:\r80/path"));
        URL urlReturnPort80{urlReturnPort80Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlReturnPort80, &urlPort80));
        auto urlFormFeedPort80Str = FromUTF8Z(U8("http://example.test:\f80/path"));
        URL urlFormFeedPort80{urlFormFeedPort80Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlFormFeedPort80, &urlPort80));
        auto widePlusPort = Create16(u"http://wide.test:+8080/path", 27);
        URL urlWidePlusPort{widePlusPort.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 16, 23, 27, 27};
        Require(URLProtocolHostAndPortAreEqual(&urlWidePlusPort, &urlWidePort));
        auto wideSpacePort = Create16(u"http://wide.test: 8080 /path", 28);
        URL urlWideSpacePort{wideSpacePort.impl, 1U | 2U | (7U << 3) | (4U << 6), 7, 7, 7, 16, 24, 28, 28};
        Require(URLProtocolHostAndPortAreEqual(&urlWideSpacePort, &urlWidePort));
        auto wideTabPort = Create16(u"http://wide.test:\t8080/path", 27);
        URL urlWideTabPort{wideTabPort.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 16, 23, 27, 27};
        Require(URLProtocolHostAndPortAreEqual(&urlWideTabPort, &urlWidePort));
        auto wideNewlinePort = Create16(u"http://wide.test:\n8080/path", 27);
        URL urlWideNewlinePort{wideNewlinePort.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 16, 23, 27, 27};
        Require(URLProtocolHostAndPortAreEqual(&urlWideNewlinePort, &urlWidePort));
        auto wideReturnPort = Create16(u"http://wide.test:\r8080/path", 27);
        URL urlWideReturnPort{wideReturnPort.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 16, 23, 27, 27};
        Require(URLProtocolHostAndPortAreEqual(&urlWideReturnPort, &urlWidePort));
        auto wideFormFeedPort = Create16(u"http://wide.test:\f8080/path", 27);
        URL urlWideFormFeedPort{wideFormFeedPort.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 16, 23, 27, 27};
        Require(URLProtocolHostAndPortAreEqual(&urlWideFormFeedPort, &urlWidePort));
        auto urlMinusPortStr = FromUTF8Z(U8("http://example.test:-80/path"));
        URL urlMinusPort{urlMinusPortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
        Require(!URLProtocolHostAndPortAreEqual(&urlMinusPort, &urlPort80));
        Require(URLProtocolHostAndPortAreEqual(&urlMinusPort, &urlNoPort));
        auto urlBarePlusPortStr = FromUTF8Z(U8("http://example.test:+/path"));
        URL urlBarePlusPort{urlBarePlusPortStr.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
        Require(!URLProtocolHostAndPortAreEqual(&urlBarePlusPort, &urlPort80));
        Require(URLProtocolHostAndPortAreEqual(&urlBarePlusPort, &urlNoPort));
        auto urlPortOutOfBoundsStr = FromUTF8Z(U8("http://example.test:80"));
        URL urlPortOutOfBounds{urlPortOutOfBoundsStr.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 19, 24, 22, 22};
        Require(URLProtocolHostAndPortAreEqual(&urlPortOutOfBounds, &urlNoPort));
        Require(!URLProtocolHostAndPortAreEqual(&urlPortOutOfBounds, &urlPort80));
        Require(URLProtocolHostAndPortAreEqual(&absent, &absent));
        Require(!URLProtocolHostAndPortAreEqual(&url, &absent));
        Require(!URLProtocolHostAndPortAreEqual(&absent, &url));
        ExpectErr([&] { URLProtocolHostAndPortAreEqual(nullptr, &url); });
        ExpectErr([&] { URLProtocolHostAndPortAreEqual(&url, nullptr); });
    }
    URLInvalidate(&url);
    Require(url.impl == string.impl && url.bits == 0 && url.userStart == 0 && url.userEnd == 0);
    Require(url.passwordEnd == 0 && url.hostEnd == 0 && url.pathEnd == 0 && url.pathAfterLastSlash == 0 && url.queryEnd == 0);
    ExpectErr([&] { URLInvalidate(nullptr); });
    ExpectErr([&] { URLProtocol(nullptr); });
    auto* memory = static_cast<unsigned char*>(JSRealloc(nullptr, 5));
    std::memcpy(memory, "hello", 5);
    memory = static_cast<unsigned char*>(JSRealloc(memory, 50));
    Require(!std::memcmp(memory, "hello", 5));
    memory = static_cast<unsigned char*>(JSRealloc(memory, 2));
    Require(memory[0] == 'h' && memory[1] == 'e');
    Require(!JSRealloc(memory, 0));
    TestURLStringHelpers();
    TestURLParserChecks();
}

void TestURLLastPathComponentAndReferrer() {
    auto httpBits = [](unsigned schemeEnd, unsigned portLength) { return 1U | 2U | (portLength << 3) | (schemeEnd << 6); };
    auto simple = FromUTF8Z(U8("http://h/a/b"));
    URL url{simple.impl, httpBits(4, 0), 7, 7, 7, 8, 11, 12, 12};
    Require(ViewText(URLLastPathComponent(&url)) == "b");
    auto trailing = FromUTF8Z(U8("http://h/a/b/"));
    URL slashed{trailing.impl, httpBits(4, 0), 7, 7, 7, 8, 13, 13, 13};
    Require(ViewText(URLLastPathComponent(&slashed)) == "b");
    auto root = FromUTF8Z(U8("http://h/"));
    URL rootUrl{root.impl, httpBits(4, 0), 7, 7, 7, 8, 9, 9, 9};
    Require(!URLLastPathComponent(&rootUrl).data);
    auto hostOnly = FromUTF8Z(U8("http://h"));
    URL noPath{hostOnly.impl, httpBits(4, 0), 7, 7, 7, 8, 8, 8, 8};
    Require(!URLLastPathComponent(&noPath).data);
    auto opaque = FromUTF8Z(U8("mailto:abc"));
    URL mailto{opaque.impl, 1U | 4U | (6U << 6), 7, 7, 7, 7, 7, 10, 10};
    Require(ViewText(URLLastPathComponent(&mailto)) == "mailto:abc");
    auto wide = Create16(u"http://h/a/b", 12);
    URL wideUrl{wide.impl, httpBits(4, 0), 7, 7, 7, 8, 11, 12, 12};
    auto wideResult = URLLastPathComponent(&wideUrl);
    Require(!wideResult.is8Bit && wideResult.length == 1 && ViewText(wideResult) == "b");
    auto port = FromUTF8Z(U8("http://h:80/dir/file.txt?q"));
    URL withPort{port.impl, httpBits(4, 3), 7, 7, 7, 8, 16, 24, 26};
    Require(ViewText(URLLastPathComponent(&withPort)) == "file.txt");
    auto slashDot = FromUTF8Z(U8("foo:/.//a"));
    URL dotted{slashDot.impl, 1U | (3U << 6), 4, 4, 4, 4, 8, 9, 9};
    Require(ViewText(URLLastPathComponent(&dotted)) == "a");
    auto beforePath = FromUTF8Z(U8("foo:/.a"));
    URL guarded{beforePath.impl, 1U | (3U << 6), 4, 4, 4, 4, 5, 7, 7};
    Require(!URLLastPathComponent(&guarded).data);
    auto emptyComponent = FromUTF8Z(U8("http://h//"));
    URL doubled{emptyComponent.impl, httpBits(4, 0), 7, 7, 7, 8, 10, 10, 10};
    auto emptyResult = URLLastPathComponent(&doubled);
    Require(emptyResult.data && emptyResult.length == 0);
    URL absent{};
    Require(!URLLastPathComponent(&absent).data);
    URL pastEnd = url;
    pastEnd.pathEnd = 50;
    ExpectErr([&] { URLLastPathComponent(&pastEnd); });
    ExpectErr([&] { URLLastPathComponent(nullptr); });

    auto full = FromUTF8Z(U8("https://u:p@h:80/a?q#f"));
    URL credentialed{full.impl, httpBits(5, 3), 8, 9, 11, 13, 17, 18, 20};
    auto stripped = URLStrippedForReferrer(&credentialed);
    Require(Bytes(UTF8(&stripped, 1)) == "https://h:80/a?q" && stripped.impl != full.impl && stripped.impl->length == 16);
    auto plain = FromUTF8Z(U8("http://h/a?q"));
    URL untouched{plain.impl, httpBits(4, 0), 7, 7, 7, 8, 9, 10, 12};
    auto same = URLStrippedForReferrer(&untouched);
    Require(same.impl == plain.impl && plain.impl->refCount == 4);
    auto fragment = FromUTF8Z(U8("http://h/a#f"));
    URL fragmentOnly{fragment.impl, httpBits(4, 0), 7, 7, 7, 8, 9, 10, 10};
    auto noFragment = URLStrippedForReferrer(&fragmentOnly);
    Require(Bytes(UTF8(&noFragment, 1)) == "http://h/a");
    auto userOnly = FromUTF8Z(U8("http://u@h/a"));
    URL userUrl{userOnly.impl, httpBits(4, 0), 7, 8, 8, 10, 11, 12, 12};
    auto noUser = URLStrippedForReferrer(&userUrl);
    Require(Bytes(UTF8(&noUser, 1)) == "http://h/a");
    auto wideFull = Create16(u"http://u@h/a#f", 14);
    URL wideCredentialed{wideFull.impl, httpBits(4, 0), 7, 8, 8, 10, 11, 12, 12};
    auto wideStripped = URLStrippedForReferrer(&wideCredentialed);
    Require(!(wideStripped.impl->hashAndFlags & 4) && wideStripped.impl->length == 10 && Bytes(UTF8(&wideStripped, 1)) == "http://h/a");
    URL invalid{plain.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    {
        const unsigned before = plain.impl->refCount;
        auto kept = URLStrippedForReferrer(&invalid);
        Require(kept.impl == plain.impl && plain.impl->refCount == before + 2);
    }
    Require(plain.impl->refCount == 4);
    URL badBounds = untouched;
    badBounds.queryEnd = 99;
    ExpectErr([&] { URLStrippedForReferrer(&badBounds); });
    auto passwordOnly = FromUTF8Z(U8("http://:p@h/"));
    URL passwordUrl{passwordOnly.impl, httpBits(4, 0), 7, 7, 9, 11, 12, 12, 12};
    auto noPassword = URLStrippedForReferrer(&passwordUrl);
    Require(Bytes(UTF8(&noPassword, 1)) == "http://h/");
    URL reversedCredentials = userUrl;
    reversedCredentials.userStart = 200;
    ExpectErr([&] { URLStrippedForReferrer(&reversedCredentials); });
    auto hostless = FromUTF8Z(U8("mailto:@x"));
    URL atSign{hostless.impl, 1U | 4U | (6U << 6), 7, 7, 7, 7, 7, 9, 9};
    Require(URLStrippedForReferrer(&atSign).impl == hostless.impl);
    auto allCredentials = FromUTF8Z(U8("ab"));
    URL consumed{allCredentials.impl, 1U, 0, 0, 0, 0, 0, 0, 0};
    auto vanished = URLStrippedForReferrer(&consumed);
    Require(vanished.impl && vanished.impl->length == 0 && vanished.impl != allCredentials.impl);
    URL queryBeforeCredentials = userUrl;
    queryBeforeCredentials.queryEnd = 8;
    ExpectErr([&] { URLStrippedForReferrer(&queryBeforeCredentials); });
    ExpectErr([&] { URLStrippedForReferrer(nullptr); });
}

}  // namespace Wtf2Tests
