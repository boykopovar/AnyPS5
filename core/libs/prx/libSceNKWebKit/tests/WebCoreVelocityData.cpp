#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;
using WebCore::FloatPoint;
using WebCore::HistoricalVelocityData;
using WebCore::VelocityData;
using WTF::MonotonicTime;

constexpr unsigned NumberRespectingIntegers = 2;
constexpr double kThreshold = 0x1.1111111111111p-6;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "VelocityData check failed at line %u\n", location.line());
        std::abort();
    }
}

VelocityData Feed(HistoricalVelocityData& history, FloatPoint position, double scale, double timestamp) {
    return _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
        &history, position, scale, MonotonicTime{timestamp});
}

bool Same(const VelocityData& v, float horizontal, float vertical, float scale, double time) {
    return v.horizontalVelocity == horizontal && v.verticalVelocity == vertical && v.scaleChangeRate == scale
        && v.lastUpdateTime.m_value == time;
}

bool Entry(const HistoricalVelocityData& history, unsigned index, double time, float x, float y, double scale) {
    const auto& data = history.m_positionHistory[index];
    return data.timestamp.m_value == time && data.position.m_x == x && data.position.m_y == y && data.scale == scale;
}

void TestRingBuffer() {
    HistoricalVelocityData history;

    Check(Same(Feed(history, {10.0f, 20.0f}, 1.0, 1.0), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == 1.0);
    Check(Entry(history, 1, 1.0, 10.0f, 20.0f, 1.0));

    Check(Same(Feed(history, {20.0f, 40.0f}, 2.0, 1.5), 20.0f, 40.0f, 2.0f, 1.5));
    Check(history.m_historySize == 2);
    Check(history.m_latestDataIndex == 2);
    Check(Entry(history, 2, 1.5, 20.0f, 40.0f, 2.0));

    Check(Same(Feed(history, {30.0f, 60.0f}, 3.0, 2.0), 20.0f, 40.0f, 2.0f, 2.0));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 0);
    Check(Entry(history, 0, 2.0, 30.0f, 60.0f, 3.0));

    Check(Same(Feed(history, {70.0f, 0.0f}, 3.0, 2.5), 40.0f, -13.333333015441895f, 1.3333333730697632f, 2.5));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == 2.5);
    Check(Entry(history, 1, 2.5, 70.0f, 0.0f, 3.0));

    Check(Same(Feed(history, {80.0f, 0.0f}, 3.0, 2.51), 59.405941009521484f, -39.60396194458008f, 0.9900990128517151f, 2.51));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == 2.5);
    Check(Entry(history, 1, 2.51, 80.0f, 0.0f, 3.0));
    Check(Entry(history, 2, 1.5, 20.0f, 40.0f, 2.0));
    Check(Entry(history, 0, 2.0, 30.0f, 60.0f, 3.0));

    Check(Same(Feed(history, {50.0f, 25.0f}, 0.5, 3.0), 20.0f, -10.0f, -1.0f, 3.0));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 2);
    Check(history.m_lastAppendTimestamp.m_value == 3.0);
    Check(Entry(history, 2, 3.0, 50.0f, 25.0f, 0.5));

    Check(Same(Feed(history, {60.0f, 30.0f}, 4.5, 3.5), 20.0f, -20.0f, 1.0f, 3.5));
    Check(history.m_historySize == 3);
    Check(history.m_latestDataIndex == 0);
    Check(Entry(history, 0, 3.5, 60.0f, 30.0f, 4.5));
}

void TestFirstSampleInsideThreshold() {
    HistoricalVelocityData history;
    Check(Same(Feed(history, {5.0f, 6.0f}, 2.0, kThreshold), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 0);
    Check(history.m_latestDataIndex == 0);
    Check(history.m_lastAppendTimestamp.m_value == 0.0);
    Check(Entry(history, 0, kThreshold, 5.0f, 6.0f, 2.0));

    Check(Same(Feed(history, {7.0f, 8.0f}, 1.0, 1.0), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);
    Check(Entry(history, 1, 1.0, 7.0f, 8.0f, 1.0));
}

void TestDeltaEqualToThreshold() {
    constexpr double first = 0x1.1111111111113p-6;
    constexpr double second = 0x1.1111111111112p-5;
    static_assert(second - first == kThreshold);

    HistoricalVelocityData history;
    Feed(history, {0.0f, 0.0f}, 1.0, first);
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);

    Check(Same(Feed(history, {4.0f, 4.0f}, 1.0, second), 0.0f, 0.0f, 0.0f, 0.0));
    Check(history.m_historySize == 1);
    Check(history.m_latestDataIndex == 1);
    Check(history.m_lastAppendTimestamp.m_value == first);
    Check(Entry(history, 1, second, 4.0f, 4.0f, 1.0));
}

TextStream NewStream(unsigned flags, bool multiLine, int indent) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = flags;
    stream.multiLineMode = multiLine;
    stream.indent = indent;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

std::string Dump(const VelocityData& value, unsigned flags = 0, bool multiLine = true, int indent = 0) {
    TextStream stream = NewStream(flags, multiLine, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(stream, value);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

void TestDump() {
    Check(Dump(VelocityData{}) == "\n(timestamp 0.00)");
    Check(Dump(VelocityData{1.5f, 0.0f, -2.25f, MonotonicTime{3.0}}) == "\n(timestamp 3.00)\n(horizontalVelocity 1.50)\n(scaleChangeRate -2.25)");
    Check(Dump(VelocityData{0.0f, 0.5f, 0.0f, MonotonicTime{1.25}}) == "\n(timestamp 1.25)\n(verticalVelocity 0.50)");
    Check(Dump(VelocityData{1.0f, 2.0f, 3.0f, MonotonicTime{4.0}}, 0, false) == " (timestamp 4.00) (horizontalVelocity 1.00) (verticalVelocity 2.00) (scaleChangeRate 3.00)");
    Check(Dump(VelocityData{1.0f, 0.0f, 0.0f, MonotonicTime{4.0}}, 0, true, 1) == "\n  (timestamp 4.00)\n  (horizontalVelocity 1.00)");
    Check(Dump(VelocityData{2.0f, 0.0f, 0.5f, MonotonicTime{7.0}}, NumberRespectingIntegers) == "\n(timestamp 7)\n(horizontalVelocity 2)\n(scaleChangeRate 0.50)");
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(base != nullptr);
        DWORD previous = 0;
        Check(VirtualProtect(base + pageSize * 3, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Check(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Check(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Check(mprotect(base + pageSize * 3, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 4);
#endif
    }

    void ProtectReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Check(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
#else
        Check(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 3; }
};

template<typename TFunction>
void RequireNotImplemented(TFunction&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Check(threw);
    Check(NotImplementedCalls_nid_no_patch() == before + 1);
}

template<typename TFunction>
void RequireInvalidArgument(TFunction&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
}

void TestGuestPointers() {
    GuardedPages pages;
    const VelocityData sample{1.5f, 0.0f, -2.25f, MonotonicTime{3.0}};
    std::memcpy(pages.ReadOnly(), &sample, sizeof(sample));
    pages.ProtectReadOnly();

    TextStream stream = NewStream(0, true, 0);
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
        stream, *reinterpret_cast<const VelocityData*>(pages.ReadOnly()));
    Check(Release(stream) == "\n(timestamp 3.00)\n(horizontalVelocity 1.50)\n(scaleChangeRate -2.25)");

    RequireInvalidArgument([&] {
        _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
            nullptr, {1.0f, 2.0f}, 1.0, MonotonicTime{1.0});
    });

    HistoricalVelocityData badHistory;
    badHistory.m_historySize = HistoricalVelocityData::maxHistoryDepth + 1;
    RequireInvalidArgument([&] {
        _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
            &badHistory, {1.0f, 2.0f}, 1.0, MonotonicTime{1.0});
    });

    HistoricalVelocityData badIndex;
    badIndex.m_latestDataIndex = HistoricalVelocityData::maxHistoryDepth;
    RequireInvalidArgument([&] {
        _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
            &badIndex, {1.0f, 2.0f}, 1.0, MonotonicTime{1.0});
    });

    RequireNotImplemented([&] {
        _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
            reinterpret_cast<HistoricalVelocityData*>(pages.Guard()), {1.0f, 2.0f}, 1.0, MonotonicTime{1.0});
    });
    RequireNotImplemented([&] {
        _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
            reinterpret_cast<HistoricalVelocityData*>(pages.ReadOnly()), {1.0f, 2.0f}, 1.0, MonotonicTime{1.0});
    });

    unsigned char* const splitHistory = pages.ReadOnly() - 8;
    std::memset(splitHistory, 0x5A, 8);
    RequireNotImplemented([&] {
        _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
            reinterpret_cast<HistoricalVelocityData*>(splitHistory), {1.0f, 2.0f}, 1.0, MonotonicTime{1.0});
    });
    for (std::size_t i = 0; i < 8; ++i) Check(splitHistory[i] == 0x5A);

    TextStream validStream = NewStream(0, true, 0);
    RequireNotImplemented([&] {
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
            validStream, *reinterpret_cast<const VelocityData*>(pages.Guard()));
    });
    RequireNotImplemented([&] {
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
            validStream, *reinterpret_cast<const VelocityData*>(pages.Guard() - 8));
    });
    RequireNotImplemented([&] {
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
            *reinterpret_cast<TextStream*>(pages.Guard()), sample);
    });
    RequireNotImplemented([&] {
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
            *reinterpret_cast<TextStream*>(pages.ReadOnly()), sample);
    });
    Check(Release(validStream).empty());
}

}  // namespace

int main() {
    TestRingBuffer();
    TestFirstSampleInsideThreshold();
    TestDeltaEqualToThreshold();
    TestDump();
    TestGuestPointers();
    return 0;
}
