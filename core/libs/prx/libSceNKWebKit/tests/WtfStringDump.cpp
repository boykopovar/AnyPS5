#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string dump check failed at line %d\n", line);
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
    Require(std::string_view(format) == "%s");
    char buffer[512];
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

String Latin1(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

String Utf16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

std::string PrintImpl(const StringImpl* impl) {
    Streams streams;
    unsigned before = impl ? impl->refCount : 0;
    _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(streams.Out(), impl);
    if (impl) Require(impl->refCount == before);
    return streams.log;
}

std::string DumpURL(const URL& url) {
    Streams streams;
    unsigned before = url.impl ? url.impl->refCount : 0;
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, streams.Out());
    if (url.impl) Require(url.impl->refCount == before);
    return streams.log;
}

URL Parse(std::string_view text) {
    const String input = Latin1(text);
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

void TestPrintInternalStringImpl() {
    Require(PrintImpl(nullptr) == "[A:(null StringImpl*)]");
    Require(PrintImpl(Latin1("").impl) == "[A:]");
    Require(PrintImpl(Latin1("plain ascii").impl) == "[A:plain ascii]");
    Require(PrintImpl(Latin1("caf\xE9 \xA0\xFF").impl) == "[A:caf\xC3\xA9 \xC2\xA0\xC3\xBF]");
    Require(PrintImpl(Utf16(u"\u20AC \U0001F600 \u00E9").impl) == "[A:\xE2\x82\xAC \xF0\x9F\x98\x80 \xC3\xA9]");
    Require(PrintImpl(Utf16(u"ascii in utf16").impl) == "[A:ascii in utf16]");
    std::u16string lone = u"a";
    lone.push_back(static_cast<char16_t>(0xD800));
    lone += u"b";
    lone.push_back(static_cast<char16_t>(0xDC00));
    Require(PrintImpl(Utf16(lone).impl) == "[A:a\xEF\xBF\xBD" "b\xEF\xBF\xBD]");
    Require(PrintImpl(Latin1(std::string_view("before\0after", 12)).impl) == "[A:before]");
    Require(PrintImpl(Latin1("100% %s %d").impl) == "[A:100% %s %d]");
}

void TestURLDump() {
    URL url = Parse("HTTP://Example.COM/a/../b?q#f");
    Require(DumpURL(url) == "<A[B:http://example.com/b?q#f]A>");
    NKWebKit::WtfAbi::Release(url.impl);

    URL invalid = Parse("not a url");
    Require(DumpURL(invalid) == "<A[B:not a url]A>");
    NKWebKit::WtfAbi::Release(invalid.impl);

    URL null{};
    Require(DumpURL(null) == "<A[B:]A>");

    URL wide{};
    String wideText = Utf16(u"http://a/\u00E9");
    wide.impl = wideText.impl;
    Require(DumpURL(wide) == "<A[B:http://a/\xC3\xA9]A>");
}

void TestNullArguments() {
    String text = Latin1("x");
    URL url{};
    Require(Throws([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(nullptr, text.impl); }));
    Require(Throws([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, nullptr); }));
    Streams streams;
    Require(Throws([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(nullptr, streams.Out()); }));
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
    const String valid = Latin1("hello");
    URL validUrl{};
    validUrl.impl = valid.impl;
    constexpr const char* kPrintSymbol = "_ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE";
    constexpr const char* kUrlSymbol = "_ZNK3WTF3URL4dumpERNS_11PrintStreamE";

    Require(ThrowsNotImplemented([&] {
        _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(pages.Guard()), valid.impl);
    }, kPrintSymbol));
    TestStream badVtableStream{reinterpret_cast<void* const*>(pages.Guard()), &streams.log, 'X', nullptr};
    Require(ThrowsNotImplemented([&] {
        _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&badVtableStream), valid.impl);
    }, kPrintSymbol));
    Require(ThrowsNotImplemented([&] {
        _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(streams.Out(), reinterpret_cast<const StringImpl*>(pages.Guard()));
    }, kPrintSymbol));
    StringImpl badDataImpl{2, 8, pages.Guard() - 4, 4};
    Require(ThrowsNotImplemented([&] {
        _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(streams.Out(), &badDataImpl);
    }, kPrintSymbol));
    Require(streams.log.empty());

    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(reinterpret_cast<const URL*>(pages.Guard()), streams.Out());
    }, kUrlSymbol));
    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(reinterpret_cast<const URL*>(pages.Guard() - 8), streams.Out());
    }, kUrlSymbol));
    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&validUrl, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(pages.Guard()));
    }, kUrlSymbol));
    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&validUrl, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&badVtableStream));
    }, kUrlSymbol));

    URL badImplUrl{};
    badImplUrl.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&badImplUrl, streams.Out());
    }, kUrlSymbol));
    badImplUrl.impl = &badDataImpl;
    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&badImplUrl, streams.Out());
    }, kUrlSymbol));
    Require(streams.log.empty());

    TestStream badInnerStream{testVtable, &streams.log, 'Y', reinterpret_cast<TestStream*>(pages.Guard())};
    Require(ThrowsNotImplemented([&] {
        _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&validUrl, reinterpret_cast<NKWebKit::WtfDumpAbi::PrintStream*>(&badInnerStream));
    }, kUrlSymbol));
    Require(streams.log == "<YY>");
}

}

int main() {
    TestPrintInternalStringImpl();
    TestURLDump();
    TestNullArguments();
    TestGuestPointers();
    return 0;
}
