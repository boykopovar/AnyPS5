#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PlatformTimeRanges dump check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct TestStream {
    void* const* vtable;
    std::string* log;
    char tag;
    TestStream* target;
};

static void APS5_VABI Destroy(TestStream*) {
    std::abort();
}

static void APS5_VABI Vprintf(TestStream* self, const char* format, VaList* arguments) {
    char buffer[128];
    Require(vsnprintf_nid_postfix(buffer, sizeof(buffer), format, arguments) >= 0);
    *self->log += '[';
    *self->log += self->tag;
    *self->log += ':';
    *self->log += buffer;
    *self->log += ']';
}

static void APS5_VABI Flush(TestStream*) {
    std::abort();
}

static TestStream* APS5_VABI Begin(TestStream* self) {
    *self->log += '<';
    *self->log += self->tag;
    return self->target;
}

static void APS5_VABI End(TestStream* self) {
    *self->log += self->tag;
    *self->log += '>';
}

static void* const testVtable[] = {
    reinterpret_cast<void*>(&Destroy),
    reinterpret_cast<void*>(&Destroy),
    reinterpret_cast<void*>(&Vprintf),
    reinterpret_cast<void*>(&Flush),
    reinterpret_cast<void*>(&Begin),
    reinterpret_cast<void*>(&End),
};

struct Streams {
    std::string log;
    TestStream inner{testVtable, &log, 'B', nullptr};
    TestStream outer{testVtable, &log, 'A', &inner};

    Streams() {
        inner.target = &inner;
    }

    NKWebKit::WtfDumpAbi::PrintStream* Out() {
        return reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&outer);
    }
};

template<typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static std::string Dump(const WebCore::PlatformTimeRanges& ranges) {
    Streams streams;
    _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&ranges, streams.Out());
    return streams.log;
}

static std::string RationalLog(const char* value, const char* scale, const char* asDouble) {
    return std::string("<B[B:{]B><B[B:") + value + "][B:/][B:" + scale + "][B: = ]B><B[B:" + asDouble + "][B:}]B>";
}

static std::string DoubleLog(const char* asDouble) {
    return std::string("<B[B:{]B><B[B:") + asDouble + "][B:}]B>";
}

static WTF::MediaTime Rational(std::int64_t value, std::uint32_t scale) {
    return WTF::MediaTime(value, scale, WTF::MediaTime::Valid);
}

static WTF::MediaTime Double(double value) {
    WTF::MediaTime time(0, 1, WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue);
    time.m_timeValueAsDouble = value;
    return time;
}

int main() {
    {
        WebCore::PlatformTimeRanges ranges;
        _ZN7WebCore18PlatformTimeRangesC1Ev(&ranges);
        Require(Dump(ranges).empty());
        _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges);
    }
    {
        WebCore::PlatformTimeRanges ranges;
        const WTF::MediaTime start = Rational(1, 2);
        const WTF::MediaTime end = Rational(3, 2);
        _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(&ranges, start, end);
        const std::string expected = "<A[B:[]" + RationalLog("1", "2", "0.500000") + "[B:..]" + RationalLog("3", "2", "1.500000") + "[B:] ]A>";
        Require(Dump(ranges) == expected);
        _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges);
    }
    {
        WebCore::PlatformTimeRanges ranges;
        _ZN7WebCore18PlatformTimeRangesC1Ev(&ranges);
        const WTF::MediaTime laterStart = Double(4.0);
        const WTF::MediaTime laterEnd = Double(6.25);
        const WTF::MediaTime earlierStart = Rational(-3, 4);
        const WTF::MediaTime earlierEnd = Rational(1, 1);
        _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, laterStart, laterEnd);
        _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, earlierStart, earlierEnd);
        Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 2);
        const std::string expected = "<A[B:[]" + RationalLog("-3", "4", "-0.750000") + "[B:..]" + RationalLog("1", "1", "1.000000") + "[B:] ]A>"
            + "<A[B:[]" + DoubleLog("4.000000") + "[B:..]" + DoubleLog("6.250000") + "[B:] ]A>";
        Require(Dump(ranges) == expected);
        _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges);
    }
    {
        Streams streams;
        WebCore::PlatformTimeRanges ranges;
        _ZN7WebCore18PlatformTimeRangesC1Ev(&ranges);
        Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&ranges, nullptr); }));
        WebCore::PlatformTimeRanges broken{{nullptr, 0, 1}};
        Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&broken, streams.Out()); }));
        Require(streams.log.empty());
        _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges);
    }
    return 0;
}
