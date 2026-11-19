#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>

using NKWebKit::InspectorAbi::ScriptCallFrame;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

namespace {

using FrameEqual = bool (APS5_VABI *)(const ScriptCallFrame*, const ScriptCallFrame*);

static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame7isEqualERKS0_), FrameEqual>);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ScriptCallFrame::isEqual check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

struct TextImpl {
    StringImpl value;
    explicit TextImpl(const char* text) : value{2, static_cast<unsigned>(std::strlen(text)), text, 4U} {}
    TextImpl(const char16_t* text, unsigned length) : value{2, length, text, 0U} {}
};

String MakeString(TextImpl& text) {
    text.value.refCount += 2;
    return String(&text.value);
}

ScriptCallFrame MakeFrame(TextImpl* functionName, TextImpl* scriptName, TextImpl* preRedirectURL,
    std::uint32_t sourceID, unsigned lineNumber, unsigned column) {
    ScriptCallFrame frame;
    if (functionName) frame.functionName = MakeString(*functionName);
    if (scriptName) frame.scriptName = MakeString(*scriptName);
    if (preRedirectURL) frame.preRedirectURL = MakeString(*preRedirectURL);
    frame.sourceID = sourceID;
    frame.lineNumber = lineNumber;
    frame.column = column;
    return frame;
}

bool Equal(const ScriptCallFrame& a, const ScriptCallFrame& b) {
    const bool result = _ZNK9Inspector15ScriptCallFrame7isEqualERKS0_(&a, &b);
    Require(_ZNK9Inspector15ScriptCallFrame7isEqualERKS0_(&b, &a) == result);
    Require(reinterpret_cast<FrameEqual>(&_ZNK9Inspector15ScriptCallFrameeqERKS0_)(&a, &b) == result);
    return result;
}

void TestFields() {
    TextImpl function("handler");
    TextImpl script("app.js");
    TextImpl redirect("https://example.test/app.js");
    TextImpl other("other");

    const ScriptCallFrame base = MakeFrame(&function, &script, &redirect, 4, 10, 20);
    Require(Equal(base, base));
    Require(Equal(base, MakeFrame(&function, &script, &redirect, 4, 10, 20)));
    Require(Equal(base, MakeFrame(&function, &script, &redirect, 0xFFFFFFFFu, 10, 20)));
    Require(!Equal(base, MakeFrame(&other, &script, &redirect, 4, 10, 20)));
    Require(!Equal(base, MakeFrame(&function, &other, &redirect, 4, 10, 20)));
    Require(!Equal(base, MakeFrame(&function, &script, &other, 4, 10, 20)));
    Require(!Equal(base, MakeFrame(&function, &script, &redirect, 4, 11, 20)));
    Require(!Equal(base, MakeFrame(&function, &script, &redirect, 4, 10, 21)));
}

void TestStringContents() {
    static const char16_t wide[] = u"app.js";
    TextImpl narrow("app.js");
    TextImpl sixteen(wide, 6);
    TextImpl prefix("app");
    TextImpl empty("");

    Require(Equal(MakeFrame(&narrow, &narrow, &narrow, 1, 2, 3), MakeFrame(&sixteen, &sixteen, &sixteen, 9, 2, 3)));
    Require(!Equal(MakeFrame(&narrow, nullptr, nullptr, 1, 2, 3), MakeFrame(&prefix, nullptr, nullptr, 1, 2, 3)));
    Require(Equal(MakeFrame(nullptr, nullptr, nullptr, 0, 0, 0), MakeFrame(nullptr, nullptr, nullptr, 5, 0, 0)));
    Require(Equal(MakeFrame(&empty, &empty, &empty, 0, 0, 0), MakeFrame(&empty, &empty, &empty, 0, 0, 0)));
    Require(!Equal(MakeFrame(nullptr, nullptr, nullptr, 0, 0, 0), MakeFrame(&empty, nullptr, nullptr, 0, 0, 0)));
    Require(!Equal(MakeFrame(nullptr, nullptr, nullptr, 0, 0, 0), MakeFrame(nullptr, nullptr, &empty, 0, 0, 0)));
}

void TestNullFrames() {
    TextImpl script("app.js");
    const ScriptCallFrame frame = MakeFrame(nullptr, &script, nullptr, 0, 1, 1);
    RequireThrows([&] { _ZNK9Inspector15ScriptCallFrame7isEqualERKS0_(nullptr, &frame); });
    RequireThrows([&] { _ZNK9Inspector15ScriptCallFrame7isEqualERKS0_(&frame, nullptr); });
}

}

int main() {
    TestFields();
    TestStringContents();
    TestNullFrames();
    return 0;
}
