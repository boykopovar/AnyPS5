#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RectEdgesDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "RectEdges dump check failed at line %u\n", location.line());
        std::abort();
    }
}

TextStream NewStream(bool multiLine, unsigned flags, int indent) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = multiLine;
    stream.formattingFlags = flags;
    stream.indent = indent;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

std::string Dump(const WebCore::RectEdges<float>& edges, bool multiLine = true, unsigned flags = 0, int indent = 0) {
    TextStream stream = NewStream(multiLine, flags, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(stream, edges);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

constexpr unsigned NumberRespectingIntegers = 1u << 1;

void TestSideOrder() {
    Check(Dump({{1, 2, 3, 4}}) == "[top 1.00 right 2.00 bottom 3.00 left 4.00]");
}

void TestZero() {
    Check(Dump({}) == "[top 0.00 right 0.00 bottom 0.00 left 0.00]");
    Check(Dump({}, true, NumberRespectingIntegers) == "[top 0 right 0 bottom 0 left 0]");
}

void TestNoIndentOrLineBreak() {
    Check(Dump({{5, 6, 7, 8}}, true, NumberRespectingIntegers, 2) == "[top 5 right 6 bottom 7 left 8]");
    Check(Dump({{5, 6, 7, 8}}, false, NumberRespectingIntegers) == "[top 5 right 6 bottom 7 left 8]");
}

void TestFractions() {
    Check(Dump({{1.5f, -2, 0.25f, 10}}) == "[top 1.50 right -2.00 bottom 0.25 left 10.00]");
    Check(Dump({{1.5f, -2, 0.25f, 10}}, true, NumberRespectingIntegers) == "[top 1.50 right -2 bottom 0.25 left 10]");
}

void TestAppendsToExistingText() {
    TextStream stream = NewStream(false, NumberRespectingIntegers, 0);
    _ZN3WTF10TextStreamlsEPKc(&stream, "insets ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(stream, {{1, 2, 3, 4}});
    _ZN3WTF10TextStreamlsEPKc(&stream, "!");
    Check(Release(stream) == "insets [top 1 right 2 bottom 3 left 4]!");
}

}  // namespace

int main() {
    TestSideOrder();
    TestZero();
    TestNoIndentOrLineBreak();
    TestFractions();
    TestAppendsToExistingText();
    return 0;
}
