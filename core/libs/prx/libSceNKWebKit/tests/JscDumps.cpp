#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::JscDumpAbi::CellAttributes;
using NKWebKit::JscDumpAbi::HeapRange;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC dump check failed at line %d\n", line);
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

std::string DumpAttributes(std::int8_t destruction, std::int8_t cellKind) {
    Streams streams;
    const CellAttributes attributes{destruction, cellKind};
    _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, streams.Out());
    Require(attributes.destruction == destruction && attributes.cellKind == cellKind);
    return streams.log;
}

std::string ExpectedAttributes(const char* destruction, const char* cellKind) {
    return std::string("<A[B%s:{]<B[B%s:") + destruction + "]B>[B%s:, ]<B[B%s:" + cellKind + "]B>[B%s:}]A>";
}

std::string DumpRange(std::uint16_t begin, std::uint16_t end) {
    Streams streams;
    const HeapRange range{begin, end};
    _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, streams.Out());
    return streams.log;
}

void TestCellAttributes() {
    Require(DumpAttributes(0, 0) == ExpectedAttributes("DoesNotNeedDestruction", "JSCell"));
    Require(DumpAttributes(1, 0) == ExpectedAttributes("NeedsDestruction", "JSCell"));
    Require(DumpAttributes(0, 1) == ExpectedAttributes("DoesNotNeedDestruction", "JSCellWithIndexingHeader"));
    Require(DumpAttributes(1, 1) == ExpectedAttributes("NeedsDestruction", "JSCellWithIndexingHeader"));
    Require(DumpAttributes(0, 2) == ExpectedAttributes("DoesNotNeedDestruction", "Auxiliary"));
    Require(DumpAttributes(1, 2) == ExpectedAttributes("NeedsDestruction", "Auxiliary"));

    for (const CellAttributes& invalid : {CellAttributes{2, 0}, CellAttributes{-1, 0}, CellAttributes{0, 3}, CellAttributes{1, -1}}) {
        Streams streams;
        Require(Throws([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&invalid, streams.Out()); }));
        Require(streams.log.empty());
    }
}

void TestHeapRange() {
    Require(DumpRange(0, 0) == "[A0x%x-0x%x:0x0-0x0]");
    Require(DumpRange(0x10, 0x2A) == "[A0x%x-0x%x:0x10-0x2a]");
    Require(DumpRange(3, 0xFFFF) == "[A0x%x-0x%x:0x3-0xffff]");
    Require(DumpRange(0xFFFF, 0xFFFF) == "[A0x%x-0x%x:0xffff-0xffff]");
    Require(DumpRange(0x8000, 0x1) == "[A0x%x-0x%x:0x8000-0x1]");
}

void TestNullArguments() {
    Streams streams;
    const CellAttributes attributes{};
    const HeapRange range{};
    Require(Throws([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, nullptr); }));
    Require(Throws([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(nullptr, streams.Out()); }));
    Require(Throws([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, nullptr); }));
    Require(streams.log.empty());
}

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TFunction>
bool ThrowsNotImplemented(TFunction function, const char* symbol) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

void TestGuestPointers() {
    GuardedPages pages;
    Streams streams;
    const CellAttributes attributes{0, 0};
    const HeapRange range{1, 2};
    constexpr const char* kCellSymbol = "_ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE";
    constexpr const char* kRangeSymbol = "_ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE";

    Require(ThrowsNotImplemented([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(reinterpret_cast<const CellAttributes*>(pages.Guard()), streams.Out()); }, kCellSymbol));
    Require(ThrowsNotImplemented([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(reinterpret_cast<const CellAttributes*>(pages.Guard() - 1), streams.Out()); }, kCellSymbol));
    Require(ThrowsNotImplemented([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(pages.Guard())); }, kCellSymbol));

    TestStream badVtableStream{reinterpret_cast<void* const*>(pages.Guard()), &streams.log, 'X', nullptr};
    Require(ThrowsNotImplemented([&] {
        _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&badVtableStream));
    }, kCellSymbol));

    TestStream badInnerStream{testVtable, &streams.log, 'Y', reinterpret_cast<TestStream*>(pages.Guard())};
    Require(ThrowsNotImplemented([&] {
        _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&badInnerStream));
    }, kCellSymbol));

    Require(ThrowsNotImplemented([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(reinterpret_cast<const HeapRange*>(pages.Guard()), streams.Out()); }, kRangeSymbol));
    Require(ThrowsNotImplemented([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(reinterpret_cast<const HeapRange*>(pages.Guard() - 2), streams.Out()); }, kRangeSymbol));
    Require(ThrowsNotImplemented([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(pages.Guard())); }, kRangeSymbol));
    Require(ThrowsNotImplemented([&] {
        _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&badVtableStream));
    }, kRangeSymbol));
}

}

int main() {
    TestCellAttributes();
    TestHeapRange();
    TestNullArguments();
    TestGuestPointers();
    return 0;
}
