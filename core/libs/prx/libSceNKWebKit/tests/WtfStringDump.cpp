#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

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

}

int main() {
    TestPrintInternalStringImpl();
    TestURLDump();
    TestNullArguments();
    return 0;
}
