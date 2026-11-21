#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Check(bool value, int line) {
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

void APS5_VABI Unexpected(TestStream*) {
    std::abort();
}

void APS5_VABI Vprintf(TestStream* self, const char* format, VaList* arguments) {
    char buffer[128];
    Require(vsnprintf_nid_postfix(buffer, sizeof(buffer), format, arguments) >= 0);
    *self->log += '[';
    *self->log += self->tag;
    *self->log += ':';
    *self->log += buffer;
    *self->log += ']';
}

TestStream* APS5_VABI Begin(TestStream* self) {
    *self->log += '<';
    *self->log += self->tag;
    return self->target;
}

void APS5_VABI End(TestStream* self) {
    *self->log += self->tag;
    *self->log += '>';
}

void* const testVtable[] = {
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Unexpected),
    reinterpret_cast<void*>(&Vprintf),
    reinterpret_cast<void*>(&Unexpected),
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
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using Range = WebCore::PlatformTimeRanges::Range;

WebCore::PlatformTimeRanges Ranges(Range* buffer, unsigned capacity, unsigned size) {
    WebCore::PlatformTimeRanges ranges;
    ranges.m_ranges.m_buffer = buffer;
    ranges.m_ranges.m_capacity = capacity;
    ranges.m_ranges.m_size = size;
    return ranges;
}

std::string Dump(const WebCore::PlatformTimeRanges& ranges) {
    Streams streams;
    _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&ranges, streams.Out());
    return streams.log;
}

std::string Rational(const char* value, const char* scale, const char* number) {
    return std::string("<B[B:{]B><B[B:") + value + "][B:/][B:" + scale + "][B: = ]B><B[B:" + number + "][B:}]B>";
}

std::string Double(const char* number) {
    return std::string("<B[B:{]B><B[B:") + number + "][B:}]B>";
}

std::string Entry(const std::string& start, const std::string& end) {
    return "<A[B:[]" + start + "[B:..]" + end + "[B:] ]A>";
}

void TestEmpty() {
    Require(Dump(Ranges(nullptr, 0, 0)).empty());
    Range unused[1] = {{WTF::MediaTime(1, 1, WTF::MediaTime::Valid), WTF::MediaTime(2, 1, WTF::MediaTime::Valid)}};
    Require(Dump(Ranges(unused, 1, 0)).empty());
}

void TestSingleRange() {
    Range buffer[1] = {{WTF::MediaTime(1, 2, WTF::MediaTime::Valid), WTF::MediaTime(3, 2, WTF::MediaTime::Valid)}};
    const WebCore::PlatformTimeRanges ranges = Ranges(buffer, 1, 1);
    Require(Dump(ranges) == Entry(Rational("1", "2", "0.500000"), Rational("3", "2", "1.500000")));
    Require(buffer[0].start.m_timeValue == 1 && buffer[0].start.m_timeScale == 2);
    Require(buffer[0].end.m_timeValue == 3 && buffer[0].end.m_timeScale == 2);
}

void TestMixedRanges() {
    WTF::MediaTime doubleStart(0, 0, WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue);
    doubleStart.m_timeValueAsDouble = 0.25;
    Range buffer[3] = {
        {doubleStart, WTF::MediaTime(-9000000000, 1000, WTF::MediaTime::Valid)},
        {WTF::MediaTime(7, 1, WTF::MediaTime::Valid), WTF::MediaTime(0, 1, WTF::MediaTime::Valid | WTF::MediaTime::PositiveInfinite)},
        {WTF::MediaTime(-1, 1, WTF::MediaTime::Valid | WTF::MediaTime::NegativeInfinite), WTF::MediaTime(0, 1, WTF::MediaTime::Valid)},
    };
    Require(Dump(Ranges(buffer, 4, 3))
            == Entry(Double("0.250000"), Rational("-9000000000", "1000", "-9000000.000000"))
                   + Entry(Rational("7", "1", "7.000000"), Rational("0", "1", "inf"))
                   + Entry(Rational("-1", "1", "-inf"), Rational("0", "1", "0.000000")));
    Require(Dump(Ranges(buffer, 3, 2))
            == Entry(Double("0.250000"), Rational("-9000000000", "1000", "-9000000.000000"))
                   + Entry(Rational("7", "1", "7.000000"), Rational("0", "1", "inf")));
}

void TestAddedRanges() {
    WebCore::PlatformTimeRanges ranges;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&ranges);
    Require(Dump(ranges).empty());
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, WTF::MediaTime(5, 1, WTF::MediaTime::Valid), WTF::MediaTime(6, 1, WTF::MediaTime::Valid));
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&ranges, WTF::MediaTime(1, 1, WTF::MediaTime::Valid), WTF::MediaTime(2, 1, WTF::MediaTime::Valid));
    Require(_ZNK7WebCore18PlatformTimeRanges6lengthEv(&ranges) == 2);
    Require(Dump(ranges) == Entry(Rational("1", "1", "1.000000"), Rational("2", "1", "2.000000")) + Entry(Rational("5", "1", "5.000000"), Rational("6", "1", "6.000000")));
    _ZN7WebCore18PlatformTimeRangesD1Ev(&ranges);
}

void TestInvalidArguments() {
    Streams streams;
    Range buffer[1] = {};
    const WebCore::PlatformTimeRanges valid = Ranges(buffer, 1, 1);
    const WebCore::PlatformTimeRanges overflowing = Ranges(buffer, 1, 2);
    const WebCore::PlatformTimeRanges missing = Ranges(nullptr, 1, 1);
    Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&valid, nullptr); }));
    Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&overflowing, streams.Out()); }));
    Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&missing, streams.Out()); }));
    Require(streams.log.empty());
}

}

int main() {
    TestEmpty();
    TestSingleRange();
    TestMixedRanges();
    TestAddedRanges();
    TestInvalidArguments();
    std::puts("libSceNKWebKit PlatformTimeRanges dump passed");
    return 0;
}
