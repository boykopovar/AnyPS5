#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF stream print check failed at line %d\n", line);
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

static TestStream* APS5_VABI BeginNull(TestStream* self) {
    *self->log += '<';
    *self->log += self->tag;
    return nullptr;
}

static void* const testVtable[] = {
    reinterpret_cast<void*>(&Destroy),
    reinterpret_cast<void*>(&Destroy),
    reinterpret_cast<void*>(&Vprintf),
    reinterpret_cast<void*>(&Flush),
    reinterpret_cast<void*>(&Begin),
    reinterpret_cast<void*>(&End),
};

static void* const nullBeginVtable[] = {
    reinterpret_cast<void*>(&Destroy),
    reinterpret_cast<void*>(&Destroy),
    reinterpret_cast<void*>(&Vprintf),
    reinterpret_cast<void*>(&Flush),
    reinterpret_cast<void*>(&BeginNull),
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

int main() {
    {
        Streams streams;
        const char* format = "%s|%d|%u|%ld|%c|%.2f|%s|%d";
        _ZN3WTF11PrintStream20printfVariableFormatEPKcz(streams.Out(), format, "ab", -3, 4000000000u, static_cast<std::int64_t>(-5000000000LL), 'q', 2.5, "tail", 42);
        Require(streams.log == "[A:ab|-3|4000000000|-5000000000|q|2.50|tail|42]");
    }
    {
        Streams streams;
        _ZN3WTF11PrintStream20printfVariableFormatEPKcz(streams.Out(), "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %d", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.5, 10);
        Require(streams.log == "[A:1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.5 10]");
    }
    {
        Streams streams;
        _ZN3WTF11PrintStream20printfVariableFormatEPKcz(streams.Out(), "100%%");
        _ZN3WTF11PrintStream20printfVariableFormatEPKcz(streams.Out(), "");
        Require(streams.log == "[A:100%][A:]");
    }
    {
        Streams streams;
        Require(Throws([&] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(nullptr, "%d", 1); }));
        Require(Throws([&] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(streams.Out(), nullptr); }));
        TestStream noVtable{nullptr, &streams.log, 'C', nullptr};
        Require(Throws([&] { _ZN3WTF11PrintStream20printfVariableFormatEPKcz(reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&noVtable), "%d", 1); }));
        Require(streams.log.empty());
    }
    {
        unsigned char region[32] = {};
        NKWebKit::WtfStreamPrintAbi::MetaAllocatorHandle handle{};
        handle.refCount = 1;
        handle.left = reinterpret_cast<void*>(std::uintptr_t{0x1111});
        handle.right = reinterpret_cast<void*>(std::uintptr_t{0x2222});
        handle.parentAndRed = 0x3331;
        handle.allocator = reinterpret_cast<void*>(std::uintptr_t{0x4444});
        handle.start = region + 4;
        handle.end = region + 28;
        Streams streams;
        _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, streams.Out());
        Require(streams.log == "<A[B:" + Pointer(region + 4) + "][B:...][B:" + Pointer(region + 28) + "]A>");
        Require(Pointer(region + 4) != Pointer(region + 28));
        Require(Throws([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
        Require(Throws([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, nullptr); }));
    }
    {
        Streams streams;
        TestStream broken{nullBeginVtable, &streams.log, 'N', nullptr};
        auto* out = reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&broken);
        NKWebKit::WtfStreamPrintAbi::MetaAllocatorHandle handle{};
        Require(Throws([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, out); }));
        Require(streams.log == "<N");
    }
    return 0;
}
