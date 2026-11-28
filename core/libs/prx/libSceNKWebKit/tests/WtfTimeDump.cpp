#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <algorithm>
#include <array>
#include <cerrno>
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
#include <unistd.h>
#endif

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
    void (*onBegin)() = nullptr;
};

static void APS5_VABI Destroy(TestStream*) {
    std::abort();
}

static bool IsNanText(const char* text) {
    while (*text) {
        if ((*text == 'n' || *text == 'N') && (text[1] == 'a' || text[1] == 'A') && (text[2] == 'n' || text[2] == 'N')) {
            return true;
        }
        ++text;
    }
    return false;
}

static void APS5_VABI Vprintf(TestStream* self, const char* format, VaList* arguments) {
    char buffer[4096];
    Require(vsnprintf_nid_postfix(buffer, sizeof(buffer), format, arguments) >= 0);
    *self->log += '[';
    *self->log += self->tag;
    *self->log += ':';
    if (!std::strcmp(format, "%lf") && IsNanText(buffer)) {
        *self->log += "<nan>";
    } else {
        *self->log += buffer;
    }
    *self->log += ']';
}

static void APS5_VABI Flush(TestStream*) {
    std::abort();
}

static TestStream* APS5_VABI Begin(TestStream* self) {
    *self->log += '<';
    *self->log += self->tag;
    if (self->onBegin) self->onBegin();
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

template<typename TFunction>
static void RequireFault(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false);
}

struct GuardedPages {
    char* bytes;
    std::size_t pageSize;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        bytes = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(bytes != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(bytes + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        bytes = static_cast<char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(bytes != MAP_FAILED);
        Require(mprotect(bytes + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        Require(VirtualFree(bytes, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(bytes, pageSize * 2) == 0);
#endif
    }

    char* End() { return bytes + pageSize; }
};

static void FaultingPointers() {
    GuardedPages pages;
    const auto* unavailable = pages.End();
    Streams streams;
    RequireFault([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(reinterpret_cast<const WTF::Seconds*>(unavailable), streams.Out()); });
    RequireFault([&] { _ZNK3WTF8WallTime4dumpERNS_11PrintStreamE(reinterpret_cast<const WTF::WallTime*>(unavailable), streams.Out()); });
    RequireFault([&] { _ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE(reinterpret_cast<const WTF::MonotonicTime*>(unavailable), streams.Out()); });
    RequireFault([&] { _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(reinterpret_cast<const WTF::TimeWithDynamicClockType*>(unavailable), streams.Out()); });
    RequireFault([&] { _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(reinterpret_cast<const NKWebKit::HttpAbi::ASCIILiteral*>(unavailable), streams.Out()); });
    Require(streams.log.empty());
    RequireFault([&] { _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(reinterpret_cast<const WTF::MediaTime*>(unavailable), streams.Out()); });
    Require(streams.log == "<A[B:{]A>");
    streams.log.clear();
    RequireFault([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(reinterpret_cast<const WTF::Seconds*>(pages.End() - 1), streams.Out()); });
    WTF::Seconds seconds{1.25};
    auto* badStream = reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(pages.End());
    RequireFault([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, badStream); });
    streams.outer.vtable = reinterpret_cast<void* const*>(pages.End());
    RequireFault([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, streams.Out()); });
    Require(streams.log.empty());
    streams.outer.vtable = testVtable;
    streams.outer.target = nullptr;
    RequireFault([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, streams.Out()); });
    Require(streams.log == "<A");
    streams.log.clear();
    streams.outer.target = &streams.inner;
    std::array<void*, 6> missing{};
    std::copy(std::begin(testVtable), std::end(testVtable), missing.begin());
    missing[2] = nullptr;
    streams.inner.vtable = missing.data();
    RequireFault([&] { _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(&seconds, streams.Out()); });
    Require(streams.log == "<A");
    streams.log.clear();
    streams.inner.vtable = testVtable;
    auto* literalRecord = reinterpret_cast<NKWebKit::HttpAbi::ASCIILiteral*>(pages.End() - sizeof(NKWebKit::HttpAbi::ASCIILiteral));
    NKWebKit::HttpAbi::ASCIILiteral literal{pages.bytes};
    std::memcpy(literalRecord, &literal, sizeof(literal));
    std::memset(pages.bytes, 'x', 2048);
    pages.bytes[2048] = 0;
    _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(literalRecord, streams.Out());
    Require(streams.log == "<A[B:" + std::string(2048, 'x') + "]A>");
    streams.log.clear();
    pages.End()[-2] = 'z';
    pages.End()[-1] = 0;
    literal.characters = pages.End() - 2;
    _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(&literal, streams.Out());
    Require(streams.log == "<A[B:z]A>");
    streams.log.clear();
    pages.End()[-1] = 'z';
    errno = 71;
    RequireFault([&] { _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(&literal, streams.Out()); });
    Require(streams.log == "<A" && errno == 71);
}

static WTF::MediaTime* changingTime;
static int beginCount;

static void ChangeMediaTime() {
    ++beginCount;
    if (beginCount == 1) {
        changingTime->m_timeValue = 9;
        changingTime->m_timeScale = 3;
    } else if (beginCount == 2) {
        changingTime->m_timeValue = 15;
        changingTime->m_timeScale = 3;
    }
}

static void CallbackChangesAreObserved() {
    Streams streams;
    WTF::MediaTime time(1, 1, WTF::MediaTime::Valid);
    changingTime = &time;
    beginCount = 0;
    streams.outer.onBegin = ChangeMediaTime;
    _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&time, streams.Out());
    Require(streams.log == "<A[B:{]A><A[B:9][B:/][B:3][B: = ]A><A[B:5.000000][B:}]A>");
}

int main() {
    FaultingPointers();
    CallbackChangesAreObserved();
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
        const WTF::MediaTime& time = _ZN3WTF9MediaTime14indefiniteTimeEv();
        Require(std::isnan(_ZNK3WTF9MediaTime8toDoubleEv(&time)));
        Require(DumpMedia(time) == "<A[B:{]A><A[B:0][B:/][B:1][B: = ]A><A[B:<nan>][B:}]A>");
    }
    {
        const WTF::MediaTime& time = _ZN3WTF9MediaTime11invalidTimeEv();
        Require(std::isnan(_ZNK3WTF9MediaTime8toDoubleEv(&time)));
        Require(DumpMedia(time) == "<A[B:{]A><A[B:-1][B:/][B:1][B: = ]A><A[B:<nan>][B:}]A>");
    }
    {
        WTF::MediaTime time(0, 0, WTF::MediaTime::Valid | WTF::MediaTime::Indefinite | WTF::MediaTime::DoubleValue);
        time.m_timeValueAsDouble = 12.5;
        Require(std::isnan(_ZNK3WTF9MediaTime8toDoubleEv(&time)));
        Require(DumpMedia(time) == "<A[B:{]A><A[B:<nan>][B:}]A>");
    }
    {
        WTF::MediaTime time(0, 0, WTF::MediaTime::DoubleValue);
        time.m_timeValueAsDouble = 12.5;
        Require(std::isnan(_ZNK3WTF9MediaTime8toDoubleEv(&time)));
        Require(DumpMedia(time) == "<A[B:{]A><A[B:<nan>][B:}]A>");
    }

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
