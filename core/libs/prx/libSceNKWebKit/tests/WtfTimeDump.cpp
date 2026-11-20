#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF time dump check failed at line %d\n", line);
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

static std::string DumpTime(WTF::ClockType type, double value) {
    Streams streams;
    WTF::TimeWithDynamicClockType time{value, type};
    _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(&time, streams.Out());
    return streams.log;
}

static std::string DumpMedia(const WTF::MediaTime& time) {
    Streams streams;
    _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&time, streams.Out());
    return streams.log;
}

int main() {
    {
        Streams streams;
        WTF::Seconds seconds{1.5};
        _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, streams.Out());
        Require(streams.log == "<A[B:1.500000][B: sec]A>");
    }
    {
        Streams streams;
        WTF::Seconds seconds{-0.25};
        _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, streams.Out());
        Require(streams.log == "<A[B:-0.250000][B: sec]A>");
    }
    {
        Streams streams;
        WTF::WallTime time{1700000000.125};
        _ZNK3WTF8WallTime4dumpERNS_11PrintStreamE(&time, streams.Out());
        Require(streams.log == "<A[B:Wall(][B:1700000000.125000][B: sec)]A>");
    }
    {
        Streams streams;
        WTF::MonotonicTime time{42.0};
        _ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE(&time, streams.Out());
        Require(streams.log == "<A[B:Monotonic(][B:42.000000][B: sec)]A>");
    }

    Require(DumpTime(WTF::ClockType::Wall, 2.5) == "<A<B[B:Wall]B>[B:(][B:2.500000][B: sec)]A>");
    Require(DumpTime(WTF::ClockType::Monotonic, 3.0) == "<A<B[B:Monotonic]B>[B:(][B:3.000000][B: sec)]A>");
    Require(DumpTime(WTF::ClockType::Approximate, 0.5) == "<A<B[B:Approximate]B>[B:(][B:0.500000][B: sec)]A>");
    {
        Streams streams;
        WTF::TimeWithDynamicClockType time{1.0, static_cast<WTF::ClockType>(3)};
        Require(Throws([&] { _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(&time, streams.Out()); }));
        Require(streams.log.empty());
    }

    Require(DumpMedia(WTF::MediaTime(3, 2, WTF::MediaTime::Valid)) == "<A[B:{]A><A[B:3][B:/][B:2][B: = ]A><A[B:1.500000][B:}]A>");
    Require(DumpMedia(WTF::MediaTime(-9000000000, 1000, WTF::MediaTime::Valid))
            == "<A[B:{]A><A[B:-9000000000][B:/][B:1000][B: = ]A><A[B:-9000000.000000][B:}]A>");
    {
        WTF::MediaTime time(0, 0, WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue);
        time.m_timeValueAsDouble = 0.25;
        Require(DumpMedia(time) == "<A[B:{]A><A[B:0.250000][B:}]A>");
    }
    Require(DumpMedia(WTF::MediaTime(0, 1, WTF::MediaTime::Valid | WTF::MediaTime::PositiveInfinite))
            == "<A[B:{]A><A[B:0][B:/][B:1][B: = ]A><A[B:inf][B:}]A>");
    Require(DumpMedia(WTF::MediaTime(-1, 1, WTF::MediaTime::Valid | WTF::MediaTime::NegativeInfinite))
            == "<A[B:{]A><A[B:-1][B:/][B:1][B: = ]A><A[B:-inf][B:}]A>");

    {
        Streams streams;
        NKWebKit::HttpAbi::ASCIILiteral literal{"Content-Type"};
        _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(&literal, streams.Out());
        Require(streams.log == "<A[B:Content-Type]A>");
    }

    {
        Streams streams;
        WTF::Seconds seconds{1.0};
        WTF::WallTime wall{1.0};
        WTF::MonotonicTime monotonic{1.0};
        WTF::TimeWithDynamicClockType time{1.0, WTF::ClockType::Wall};
        WTF::MediaTime media(1, 1, WTF::MediaTime::Valid);
        NKWebKit::HttpAbi::ASCIILiteral literal{"x"};
        Require(Throws([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, nullptr); }));
        Require(Throws([&] { _ZNK3WTF8WallTime4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF8WallTime4dumpERNS_11PrintStreamE(&wall, nullptr); }));
        Require(Throws([&] { _ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE(&monotonic, nullptr); }));
        Require(Throws([&] { _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(&time, nullptr); }));
        Require(Throws([&] { _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&media, nullptr); }));
        Require(Throws([&] { _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(&literal, nullptr); }));
        Require(streams.log.empty());
    }

    std::puts("libSceNKWebKit WTF time dumps passed");
    return 0;
}
