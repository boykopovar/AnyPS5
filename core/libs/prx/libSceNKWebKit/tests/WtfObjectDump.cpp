#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF object dump check failed at line %d\n", line);
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
    char buffer[256];
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

static std::string Pointer(const void* value) {
    char buffer[64];
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "%p", value) > 0);
    return buffer;
}

static std::string DumpBits(const WTF::BitVector& vector) {
    Streams streams;
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, streams.Out());
    return streams.log;
}

static std::string DumpMediaOnInner(const WTF::MediaTime& time) {
    Streams streams;
    _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&time, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&streams.inner));
    return streams.log;
}

static std::string Expected(const std::string& bits) {
    std::string result;
    for (char bit : bits) result += std::string("<A[B:") + bit + "]A>";
    return result;
}

int main() {
    {
        alignas(16) unsigned char storage[64]{};
        Streams streams;
        _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(storage, streams.Out());
        Require(streams.log == "<A[B:Thread:][B:" + Pointer(storage) + "]A>");
    }

    {
        unsigned char memory[256]{};
        NKWebKit::WtfDumpAbi::MetaAllocatorHandle handle{1, nullptr, nullptr, nullptr, nullptr, memory + 16, memory + 144};
        Streams streams;
        _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, streams.Out());
        Require(streams.log == "<A[B:" + Pointer(memory + 16) + "][B:...][B:" + Pointer(memory + 144) + "]A>");
    }

    {
        NKWebKit::WtfDumpAbi::HeapRange range{2, 0xFFFF};
        Streams streams;
        _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, streams.Out());
        Require(streams.log == "[A:0x2-0xffff]");
        NKWebKit::WtfDumpAbi::HeapRange empty{0, 0};
        Streams emptyStreams;
        _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&empty, emptyStreams.Out());
        Require(emptyStreams.log == "[A:0x0-0x0]");
    }

    {
        alignas(8) NKWebKit::WtfDumpAbi::ClassInfo info{{"Window"}};
        Streams streams;
        _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, streams.Out());
        Require(streams.log == "<A[B:" + Pointer(&info) + "][B::[]<B[B:Window]B>[B:]]A>");
    }

    {
        Streams streams;
        WebCore::PlatformTimeRanges ranges{};
        _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&ranges, streams.Out());
        Require(streams.log.empty());
    }
    {
        WebCore::PlatformTimeRanges::Range storage[2]{
            {WTF::MediaTime(3, 2, WTF::MediaTime::Valid), WTF::MediaTime(5, 2, WTF::MediaTime::Valid)},
            {WTF::MediaTime(7, 1, WTF::MediaTime::Valid), WTF::MediaTime(0, 1, WTF::MediaTime::Valid | WTF::MediaTime::PositiveInfinite)},
        };
        WebCore::PlatformTimeRanges ranges{{storage, 2, 2}};
        Streams streams;
        _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&ranges, streams.Out());
        std::string expected;
        for (const auto& range : storage)
            expected += "<A[B:[]" + DumpMediaOnInner(range.start) + "[B:..]" + DumpMediaOnInner(range.end) + "[B:] ]A>";
        Require(streams.log == expected);
        Require(streams.log.find("<B[B:{]B><B[B:3][B:/][B:2][B: = ]B><B[B:1.500000][B:}]B>") != std::string::npos);
    }

    {
        char cells[128]{};
        NKWebKit::WtfDumpAbi::FreeList list{cells + 32, cells + 96, cells + 96, 18446744073709551615ull, 4294967295u, 48};
        Streams streams;
        _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(&list, streams.Out());
        Require(streams.log == "<A[B:{nextInterval = ][B:" + Pointer(cells + 96) + "][B:, secret = ][B:18446744073709551615][B:, intervalStart = ][B:"
                + Pointer(cells + 32) + "][B:, intervalEnd = ][B:" + Pointer(cells + 96) + "][B:, originalSize = ][B:4294967295][B:}]A>");
    }
    {
        NKWebKit::WtfDumpAbi::FreeList list{nullptr, nullptr, reinterpret_cast<void*>(std::uintptr_t{1}), 0, 0, 0};
        Streams streams;
        _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(&list, streams.Out());
        Require(streams.log == "<A[B:{nextInterval = ][B:" + Pointer(reinterpret_cast<void*>(std::uintptr_t{1})) + "][B:, secret = ][B:0][B:, intervalStart = ][B:"
                + Pointer(nullptr) + "][B:, intervalEnd = ][B:" + Pointer(nullptr) + "][B:, originalSize = ][B:0][B:}]A>");
    }

    {
        WTF::BitVector vector{(std::uintptr_t{1} << 63) | 0b1011};
        std::string bits = "11-1";
        bits.append(59, '-');
        Require(DumpBits(vector) == Expected(bits));
    }
    {
        WTF::BitVector vector{(std::uintptr_t{1} << 63) | (std::uintptr_t{1} << 62)};
        std::string bits(62, '-');
        bits += '1';
        Require(DumpBits(vector) == Expected(bits));
    }
    {
        alignas(16) std::uintptr_t storage[3]{70, 0, 0};
        storage[1] = (std::uintptr_t{1} << 0) | (std::uintptr_t{1} << 63);
        storage[2] = (std::uintptr_t{1} << 1) | (std::uintptr_t{1} << 6) | (std::uintptr_t{1} << 7);
        WTF::BitVector vector{reinterpret_cast<std::uintptr_t>(storage) >> 1};
        std::string bits(70, '-');
        bits[0] = '1';
        bits[63] = '1';
        bits[65] = '1';
        Require(DumpBits(vector) == Expected(bits));
    }
    {
        alignas(16) std::uintptr_t storage[1]{0};
        WTF::BitVector vector{reinterpret_cast<std::uintptr_t>(storage) >> 1};
        Require(DumpBits(vector).empty());
    }

    {
        Streams streams;
        unsigned char thread[8]{};
        NKWebKit::WtfDumpAbi::MetaAllocatorHandle handle{};
        NKWebKit::WtfDumpAbi::FreeList list{};
        NKWebKit::WtfDumpAbi::HeapRange range{};
        NKWebKit::WtfDumpAbi::ClassInfo info{};
        WebCore::PlatformTimeRanges ranges{};
        WebCore::PlatformTimeRanges brokenRanges{{nullptr, 1, 1}};
        WTF::BitVector inlineVector{std::uintptr_t{1} << 63};
        WTF::BitVector emptyValue{0};
        WTF::BitVector deletedValue{1};
        Require(Throws([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, nullptr); }));
        Require(Throws([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, nullptr); }));
        Require(Throws([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, nullptr); }));
        Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&ranges, nullptr); }));
        Require(Throws([&] { _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(&brokenRanges, streams.Out()); }));
        Require(Throws([&] { _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(&list, nullptr); }));
        Require(Throws([&] { _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(thread, nullptr); }));
        Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&inlineVector, nullptr); }));
        Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&emptyValue, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&deletedValue, streams.Out()); }));
        Require(streams.log.empty());
    }

    std::puts("libSceNKWebKit WTF object dumps passed");
    return 0;
}
