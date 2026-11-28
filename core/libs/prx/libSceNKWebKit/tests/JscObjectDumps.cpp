#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using NKWebKit::JscObjectDumpAbi::ClassInfo;
using NKWebKit::JscObjectDumpAbi::FreeList;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC object dump check failed at line %d\n", line);
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
    *self->log += format;
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

std::string Pointer(const void* value) {
    char buffer[64];
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "%p", value) > 0);
    return buffer;
}

const void* Address(std::uintptr_t value) {
    return reinterpret_cast<const void*>(value);
}

std::string DumpClassInfo(const ClassInfo& info) {
    Streams streams;
    _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, streams.Out());
    return streams.log;
}

std::string ExpectedClassInfo(const ClassInfo& info, const char* name) {
    return "<A[B%p:" + Pointer(&info) + "][B%s::[]<B[B%s:" + name + "]B>[B%s:]]A>";
}

std::string DumpFreeList(const FreeList& list) {
    Streams streams;
    _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(&list, streams.Out());
    return streams.log;
}

std::string ExpectedFreeList(const FreeList& list, const char* secret, const char* originalSize) {
    return "<A[B%s:{nextInterval = ][B%p:" + Pointer(list.nextInterval) + "][B%s:, secret = ][B%lu:" + secret
        + "][B%s:, intervalStart = ][B%p:" + Pointer(list.intervalStart) + "][B%s:, intervalEnd = ][B%p:" + Pointer(list.intervalEnd)
        + "][B%s:, originalSize = ][B%u:" + originalSize + "][B%s:}]A>";
}

void TestClassInfo() {
    const ClassInfo window{"Window"};
    Require(DumpClassInfo(window) == ExpectedClassInfo(window, "Window"));
    Require(std::strcmp(window.className, "Window") == 0);

    const ClassInfo object{"Object"};
    Require(DumpClassInfo(object) == ExpectedClassInfo(object, "Object"));
    Require(DumpClassInfo(object) != DumpClassInfo(window));

    const ClassInfo empty{""};
    Require(DumpClassInfo(empty) == ExpectedClassInfo(empty, ""));

    const ClassInfo unnamed{nullptr};
    Streams streams;
    Require(Throws([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&unnamed, streams.Out()); }));
    Require(streams.log.empty());
}

void TestFreeList() {
    const FreeList initial{nullptr, nullptr, const_cast<void*>(Address(1)), 0, 0, 16};
    Require(DumpFreeList(initial) == ExpectedFreeList(initial, "0", "0"));

    char cells[64]{};
    const FreeList active{cells + 16, cells + 48, cells, 0xFEDCBA9876543210ull, 4096, 32};
    const std::string dumped = DumpFreeList(active);
    Require(dumped == ExpectedFreeList(active, "18364758544493064720", "4096"));
    Require(dumped.find("[B%lu:18364758544493064720]") != std::string::npos);
    Require(active.intervalStart == cells + 16 && active.intervalEnd == cells + 48 && active.nextInterval == cells);
    Require(active.secret == 0xFEDCBA9876543210ull && active.originalSize == 4096 && active.cellSize == 32);

    const FreeList limits{const_cast<char*>(static_cast<const char*>(Address(0x7FFF0000))), nullptr, const_cast<void*>(Address(0x7FFF0011)), UINT64_MAX, UINT32_MAX, UINT32_MAX};
    Require(DumpFreeList(limits) == ExpectedFreeList(limits, "18446744073709551615", "4294967295"));
}

void TestNullArguments() {
    Streams streams;
    const ClassInfo info{"Window"};
    const FreeList list{};
    Require(Throws([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, nullptr); }));
    Require(Throws([&] { _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(&list, nullptr); }));
    Require(streams.log.empty());
}

}

int main() {
    TestClassInfo();
    TestFreeList();
    TestNullArguments();
    return 0;
}
