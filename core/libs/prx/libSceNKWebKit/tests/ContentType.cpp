#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ContentType;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ContentType check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(const char16_t* text, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKDsj(text, length);
}

std::string Text(const String& string) {
    if (!string.impl) return "<null>";
    Require(string.impl->hashAndFlags & 4);
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

ContentType Construct(const char* text) {
    ContentType value;
    String string = Make(text);
    _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(&value, &string);
    return value;
}

std::string Parameter(const char* type, const char* name) {
    ContentType contentType = Construct(type);
    String parameter = Make(name);
    String result = _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(&contentType, &parameter);
    return Text(result);
}

std::string Container(const char* type) {
    ContentType contentType = Construct(type);
    String result = _ZNK7WebCore11ContentType13containerTypeEv(&contentType);
    return Text(result);
}

template<typename Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestCodecsParameter() {
    const String& first = _ZN7WebCore11ContentType15codecsParameterEv();
    Require(Text(first) == "codecs");
    Require(&_ZN7WebCore11ContentType15codecsParameterEv() == &first);
}

void TestConstructors() {
    String original = Make("video/mp4");
    auto* impl = original.impl;
    const unsigned before = impl->refCount;

    {
        ContentType copied;
        _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(&copied, &original);
        Require(copied.type.impl == impl);
        Require(impl->refCount == before + 2);
        ContentType copiedBase;
        _ZN7WebCore11ContentTypeC2ERKN3WTF6StringE(&copiedBase, &original);
        Require(copiedBase.type.impl == impl);
        Require(impl->refCount == before + 4);

        String source = original;
        ContentType moved;
        _ZN7WebCore11ContentTypeC1EON3WTF6StringE(&moved, &source);
        Require(source.impl == nullptr);
        Require(moved.type.impl == impl);
        Require(impl->refCount == before + 6);
        String sourceBase = original;
        ContentType movedBase;
        _ZN7WebCore11ContentTypeC2EON3WTF6StringE(&movedBase, &sourceBase);
        Require(sourceBase.impl == nullptr);
        Require(movedBase.type.impl == impl);
        Require(impl->refCount == before + 8);
    }
    Require(impl->refCount == before);

    RequireThrows([] { String s = Make("a"); _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(nullptr, &s); });
    RequireThrows([] { ContentType c; _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(&c, nullptr); });
    RequireThrows([] { String s = Make("a"); _ZN7WebCore11ContentTypeC1EON3WTF6StringE(nullptr, &s); });
    RequireThrows([] { ContentType c; _ZN7WebCore11ContentTypeC1EON3WTF6StringE(&c, nullptr); });
}

void TestContainerType() {
    Require(Container("video/mp4") == "video/mp4");
    Require(Container("video/mp4; codecs=\"avc1\"") == "video/mp4");
    Require(Container("  video/mp4  ;codecs=avc1") == "video/mp4");
    Require(Container("\t\r\n video/mp4 \f") == "video/mp4");
    Require(Container(";codecs=avc1") == "");
    Require(Container("   ") == "");
    Require(Container("") == "");
    Require(Container("a;b;c") == "a");

    ContentType none;
    String result = _ZNK7WebCore11ContentType13containerTypeEv(&none);
    Require(result.impl == nullptr);
    RequireThrows([] { String r = _ZNK7WebCore11ContentType13containerTypeEv(nullptr); });

    const char16_t wide[] = u"audio/éx ; q=1";
    ContentType type;
    String string = Make16(wide, sizeof(wide) / sizeof(wide[0]) - 1);
    _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(&type, &string);
    String container = _ZNK7WebCore11ContentType13containerTypeEv(&type);
    Require(container.impl && container.impl->length == 8);
    Require(!(container.impl->hashAndFlags & 4));
    const auto* units = static_cast<const char16_t*>(container.impl->data);
    Require(units[5] == '/' && units[6] == 0x00e9 && units[7] == 'x');
}

void TestParameter() {
    Require(Parameter("video/mp4; codecs=avc1", "codecs") == "avc1");
    Require(Parameter("video/mp4; codecs=\"avc1.4d401e, mp4a.40.2\"", "codecs") == "avc1.4d401e, mp4a.40.2");
    Require(Parameter("video/mp4; CODECS=avc1", "codecs") == "avc1");
    Require(Parameter("video/mp4; codecs=avc1", "CoDeCs") == "avc1");
    Require(Parameter("video/mp4; codecs = avc1 ; profiles=x", "codecs") == "avc1");
    Require(Parameter("video/mp4; codecs=  avc1  ; profiles=x", "codecs") == "avc1");
    Require(Parameter("video/mp4; codecs=avc1; profiles=\"a,b\"", "profiles") == "a,b");
    Require(Parameter("video/mp4; codecs=avc1; profiles=x", "codecs") == "avc1");
    Require(Parameter("video/mp4; codecs=\" avc1 \"", "codecs") == "avc1");
    Require(Parameter("video/mp4; codecs=\"\"", "codecs") == "");
    Require(Parameter("video/mp4; codecs=", "codecs") == "");
    Require(Parameter("video/mp4; codecs=;profiles=x", "codecs") == "");
}

void TestParameterNotFoundArithmetic() {
    Require(Parameter("a/b; codecs=avc1", "codecs") == "avc1");
    Require(Parameter("a/b; codecs=avc1;", "codecs") == "avc1");
    Require(Parameter("\"q\"a/b; codecs=avc1", "codecs") == "avc1");
    Require(Parameter("a/b; codecs=\"avc1", "codecs") == "\"avc1");
    Require(Parameter("a/b; codecs=\"avc1; profiles=x", "codecs") == "\"avc1");
    Require(Parameter("a/b; codecs=x\"y\"z", "codecs") == "y");
    Require(Parameter("a/b; x=1; codecs=abc", "codecs") == "abc");
    Require(Parameter("a/b; mycodecs=abc", "codecs") == "abc");
}

void TestParameterAbsent() {
    Require(Parameter("video/mp4", "codecs") == "<null>");
    Require(Parameter("", "codecs") == "<null>");
    Require(Parameter("video/mp4;", "codecs") == "<null>");
    Require(Parameter("video/mp4; profiles=x", "codecs") == "<null>");
    Require(Parameter("video/mp4; codecs", "codecs") == "<null>");
    Require(Parameter("codecs=avc1", "codecs") == "<null>");
    Require(Parameter("video/mp4;codecs", "codecs") == "<null>");

    ContentType none;
    String name = Make("codecs");
    String result = _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(&none, &name);
    Require(result.impl == nullptr);

    ContentType contentType = Construct("a/b; codecs=avc1");
    String nullName;
    String nullResult = _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(&contentType, &nullName);
    Require(nullResult.impl == nullptr);

    RequireThrows([&] { String r = _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(nullptr, &name); });
    RequireThrows([&] { String r = _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(&contentType, nullptr); });
}

void TestParameterEmptyName() {
    Require(Parameter("a/b; x=1", "") == "1");
    Require(Parameter("a/b", "") == "<null>");
}

}

int main() {
    TestCodecsParameter();
    TestConstructors();
    TestContainerType();
    TestParameter();
    TestParameterNotFoundArithmetic();
    TestParameterAbsent();
    TestParameterEmptyName();
    return 0;
}
