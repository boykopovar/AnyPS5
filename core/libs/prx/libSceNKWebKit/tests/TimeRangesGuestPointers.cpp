#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

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
        std::fprintf(stderr, "TimeRanges guest pointer check failed at line %d\n", line);
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

static WTF::MediaTime Seconds(std::int64_t value) {
    return WTF::MediaTime{value, 1, WTF::MediaTime::Valid};
}

static bool Same(const WTF::MediaTime& time, std::int64_t value) {
    return time.m_timeValue == value && time.m_timeScale == 1 && time.m_timeFlags == WTF::MediaTime::Valid;
}

int main() {
    using VectorRange = WebCore::PlatformTimeRanges::VectorRange;
    auto* unreadable = reinterpret_cast<WebCore::PlatformTimeRanges*>(0x10);
    auto* unreadableTime = reinterpret_cast<const WTF::MediaTime*>(0x10);
    auto* unreadableVector = reinterpret_cast<VectorRange*>(0x10);
    auto* unreadableTimeRanges = reinterpret_cast<WebCore::TimeRanges*>(0x10);
    const WTF::MediaTime zero = Seconds(0);
    const WTF::MediaTime five = Seconds(5);
    const WTF::MediaTime ten = Seconds(10);
    const WTF::MediaTime twenty = Seconds(20);
    const char* add = "_ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_";
    const char* move = "_ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE";

    WebCore::PlatformTimeRanges host;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&host);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&host, zero, five);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&host, ten, twenty);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&host) == 2);
    VectorRange hostVector = host.m_ranges;

    unsigned char* readOnlyPage = MapPage();
    auto* readOnly = reinterpret_cast<WebCore::PlatformTimeRanges*>(readOnlyPage);
    _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(readOnly, Seconds(1), Seconds(2));
    auto* readOnlyBool = reinterpret_cast<bool*>(readOnlyPage + 64);
    auto* readOnlyTimeRanges = reinterpret_cast<WebCore::TimeRanges*>(readOnlyPage + 128);
    WebCore::PlatformTimeRanges::Range inPlace[4];
    std::memset(static_cast<void*>(inPlace), 0x5a, sizeof(inPlace));
    inPlace[0] = WebCore::PlatformTimeRanges::Range{Seconds(1), Seconds(2)};
    unsigned char inPlaceBytes[sizeof(inPlace)];
    std::memcpy(inPlaceBytes, static_cast<const void*>(inPlace), sizeof(inPlaceBytes));
    auto* readOnlyInPlace = reinterpret_cast<WebCore::PlatformTimeRanges*>(readOnlyPage + 192);
    readOnlyInPlace->m_ranges = WebCore::PlatformTimeRanges::VectorRange{inPlace, 4, 1};
    unsigned char readOnlyBytes[4096];
    std::memcpy(readOnlyBytes, readOnlyPage, sizeof(readOnlyBytes));
    ProtectReadOnly(readOnlyPage);
    auto unchanged = [&] { return std::memcmp(readOnlyPage, readOnlyBytes, sizeof(readOnlyBytes)) == 0; };

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(readOnlyInPlace, Seconds(3), Seconds(4)); return 0; },
        "_ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_"));
    Require(std::memcmp(inPlaceBytes, static_cast<const void*>(inPlace), sizeof(inPlaceBytes)) == 0);

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRangesC1Ev(unreadable); return 0; }, "_ZN7WebCore18PlatformTimeRangesC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRangesC2Ev(readOnly); return 0; }, "_ZN7WebCore18PlatformTimeRangesC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRangesD1Ev(readOnly); return 0; }, "_ZN7WebCore18PlatformTimeRangesD1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRangesD2Ev(unreadable); return 0; }, "_ZN7WebCore18PlatformTimeRangesD1Ev"));

    WebCore::PlatformTimeRanges target;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&target);
    Require(FaultsAsNotImplemented([&] {
        _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&target, unreadableVector);
        return 0;
    }, move));
    Require(FaultsAsNotImplemented([&] {
        _ZN7WebCore18PlatformTimeRangesC2EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&target, &readOnly->m_ranges);
        return 0;
    }, move));
    Require(FaultsAsNotImplemented([&] {
        _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(readOnly, &host.m_ranges);
        return 0;
    }, move));
    Require(!target.m_ranges.m_buffer && !target.m_ranges.m_size && host.m_ranges.m_buffer == hostVector.m_buffer && host.m_ranges.m_size == 2);

    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore18PlatformTimeRangesaSERKS0_(readOnly, host); }, "_ZN7WebCore18PlatformTimeRangesaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore18PlatformTimeRangesaSERKS0_(&target, *unreadable); }, "_ZN7WebCore18PlatformTimeRangesaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(readOnly, zero, five); return 0; }, add));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(unreadable, zero, five); return 0; }, add));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&host, *unreadableTime, five); return 0; }, add));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&host, zero, *unreadableTime); return 0; }, add));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges5clearEv(readOnly); return 0; }, "_ZN7WebCore18PlatformTimeRanges5clearEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges6invertEv(readOnly); return 0; }, "_ZN7WebCore18PlatformTimeRanges6invertEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(readOnly, host); return 0; }, "_ZN7WebCore18PlatformTimeRanges9unionWithERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(&host, *unreadable); return 0; }, "_ZN7WebCore18PlatformTimeRanges9unionWithERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(readOnly, host); return 0; }, "_ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(&host, *unreadable); return 0; }, "_ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_"));
    Require(unchanged() && host.m_ranges.m_buffer == hostVector.m_buffer && host.m_ranges.m_capacity == hostVector.m_capacity && host.m_ranges.m_size == 2);

    bool valid = false;
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges5startEjRb(unreadable, 0, valid); }, "_ZNK7WebCore18PlatformTimeRanges5startEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges5startEjRb(&host, 0, *readOnlyBool); }, "_ZNK7WebCore18PlatformTimeRanges5startEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges5startEj(unreadable, 0); }, "_ZNK7WebCore18PlatformTimeRanges5startEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges3endEjRb(unreadable, 0, valid); }, "_ZNK7WebCore18PlatformTimeRanges3endEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges3endEjRb(&host, 9, *readOnlyBool); }, "_ZNK7WebCore18PlatformTimeRanges3endEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges8durationEj(unreadable, 0); }, "_ZNK7WebCore18PlatformTimeRanges8durationEj"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(unreadable); }, "_ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(unreadable); }, "_ZNK7WebCore18PlatformTimeRanges13totalDurationEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(unreadable, zero); }, "_ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges7containERKN3WTF9MediaTimeE(&host, *unreadableTime); }, "_ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(unreadable, zero); }, "_ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(&host, *unreadableTime); }, "_ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges6lengthEv(unreadable); }, "_ZNK7WebCore18PlatformTimeRanges6lengthEv"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF3absERKNS_9MediaTimeE(*unreadableTime); }, "_ZN3WTF3absERKNS_9MediaTimeE"));
    Require(unchanged());

    WebCore::PlatformTimeRanges badBuffer;
    badBuffer.m_ranges = VectorRange{reinterpret_cast<WebCore::PlatformTimeRanges::Range*>(0x10), 4, 2};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges5startEj(&badBuffer, 1); }, "_ZNK7WebCore18PlatformTimeRanges5startEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges8durationEj(&badBuffer, 0); }, "_ZNK7WebCore18PlatformTimeRanges8durationEj"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(&target, badBuffer); return 0; }, "_ZN7WebCore18PlatformTimeRanges9unionWithERKS0_"));
    Require(_ZNK7WebCore18PlatformTimeRanges8durationEj(&badBuffer, 2).m_timeFlags == 0);

    unsigned char* rangePage = MapPage();
    auto* pageRanges = reinterpret_cast<WebCore::PlatformTimeRanges::Range*>(rangePage);
    for (std::size_t index = 0; index < 4096 / sizeof(WebCore::PlatformTimeRanges::Range); ++index)
        pageRanges[index] = WebCore::PlatformTimeRanges::Range{Seconds(static_cast<std::int64_t>(index) * 2), Seconds(static_cast<std::int64_t>(index) * 2 + 1)};
    constexpr unsigned oversizedCount = std::numeric_limits<unsigned>::max();
    WebCore::PlatformTimeRanges oversized;
    oversized.m_ranges = VectorRange{pageRanges, oversizedCount, oversizedCount};
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&oversized) == oversizedCount);
    Require(Same(_ZNK7WebCore18PlatformTimeRanges5startEj(&oversized, 5), 10));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(&oversized); }, "_ZNK7WebCore18PlatformTimeRanges13totalDurationEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(&oversized, ten); }, "_ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE"));
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore18PlatformTimeRangesaSERKS0_(&target, oversized); }, "_ZN7WebCore18PlatformTimeRangesaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&oversized, zero, five); return 0; }, add));
    Require(!target.m_ranges.m_buffer && !target.m_ranges.m_size && oversized.m_ranges.m_buffer == pageRanges);

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore10TimeRangesC1Ev(readOnlyTimeRanges); return 0; }, "_ZN7WebCore10TimeRangesC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore10TimeRangesC2Edd(unreadableTimeRanges, 1.0, 2.0); return 0; }, "_ZN7WebCore10TimeRangesC1Edd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore10TimeRanges3addEdd(unreadableTimeRanges, 1.0, 2.0); return 0; }, add));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore10TimeRanges6lengthEv(unreadableTimeRanges); }, "_ZNK7WebCore18PlatformTimeRanges6lengthEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore10TimeRanges7nearestEd(unreadableTimeRanges, 1.0); }, "_ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore10TimeRanges5startEj(unreadableTimeRanges, 0).m_hasValue; }, "_ZNK7WebCore18PlatformTimeRanges5startEjRb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore10TimeRanges3endEj(unreadableTimeRanges, 0).m_hasValue; }, "_ZNK7WebCore18PlatformTimeRanges3endEjRb"));
    Require(unchanged());

    unsigned char* writablePage = MapPage();
    std::memset(writablePage, 0xcc, 4096);
    auto* writable = reinterpret_cast<WebCore::PlatformTimeRanges*>(writablePage);
    _ZN7WebCore18PlatformTimeRangesC1Ev(writable);
    Require(!writable->m_ranges.m_buffer && !writable->m_ranges.m_capacity && !writable->m_ranges.m_size);
    _ZN7WebCore18PlatformTimeRangesaSERKS0_(writable, host);
    Require(writable->m_ranges.m_size == 2 && writable->m_ranges.m_capacity == 2 && writable->m_ranges.m_buffer != hostVector.m_buffer);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(writable, Seconds(30), Seconds(40));
    Require(writable->m_ranges.m_size == 3 && writable->m_ranges.m_capacity == 4);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(writable, Seconds(4), Seconds(12));
    Require(writable->m_ranges.m_size == 2 && writable->m_ranges.m_capacity == 4);
    auto* writableBool = reinterpret_cast<bool*>(writablePage + 64);
    Require(Same(_ZNK7WebCore18PlatformTimeRanges5startEjRb(writable, 0, *writableBool), 0) && *writableBool);
    Require(Same(_ZNK7WebCore18PlatformTimeRanges3endEjRb(writable, 0, *writableBool), 20) && *writableBool);
    Require(_ZNK7WebCore18PlatformTimeRanges3endEjRb(writable, 2, *writableBool).m_timeScale == _ZN3WTF9MediaTime8zeroTimeEv().m_timeScale && !*writableBool);
    Require(Same(_ZNK7WebCore18PlatformTimeRanges13totalDurationEv(writable), 30));
    Require(Same(_ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(writable, Seconds(27)), 30));
    Require(_ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(writable, Seconds(35)) == 1);
    Require(Same(_ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(writable), 40));
    _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(writable, host);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(writable) == 2);
    Require(Same(_ZNK7WebCore18PlatformTimeRanges5startEj(writable, 1), 10) && Same(_ZNK7WebCore18PlatformTimeRanges3endEj(writable, 1), 20));
    _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(writable, *readOnly);
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(writable) == 2 && Same(_ZNK7WebCore18PlatformTimeRanges3endEj(writable, 0), 5));
    _ZN7WebCore18PlatformTimeRanges5clearEv(writable);
    Require(writable->m_ranges.m_size == 0 && writable->m_ranges.m_buffer);
    _ZN7WebCore18PlatformTimeRangesD1Ev(writable);
    Require(!writable->m_ranges.m_buffer && !writable->m_ranges.m_capacity && !writable->m_ranges.m_size);

    _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(writable, &host.m_ranges);
    Require(writable->m_ranges.m_buffer == hostVector.m_buffer && writable->m_ranges.m_size == 2 && !host.m_ranges.m_buffer && !host.m_ranges.m_size);

    auto* writableTimeRanges = reinterpret_cast<WebCore::TimeRanges*>(writablePage + 128);
    _ZN7WebCore10TimeRangesC1Edd(writableTimeRanges, 1.0, 2.0);
    Require(writableTimeRanges->m_refCount == 1 && _ZNK7WebCore10TimeRanges6lengthEv(writableTimeRanges) == 1);
    Require(_ZNK7WebCore10TimeRanges7nearestEd(writableTimeRanges, 3.0) == 2.0);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&writableTimeRanges->m_ranges);
    for (std::size_t index = 160; index < 4096; ++index)
        Require(writablePage[index] == 0xcc);

    _ZN7WebCore18PlatformTimeRangesD1Ev(writable);
    _ZN7WebCore18PlatformTimeRangesD1Ev(&target);
    UnmapPage(writablePage);
    UnmapPage(rangePage);
    UnmapPage(readOnlyPage);
    return 0;
}
