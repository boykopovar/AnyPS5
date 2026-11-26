#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/ColorTextStreamExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

constexpr const char* kExportName = "_ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE";
constexpr std::uint8_t kSemantic = 1 << 0;
constexpr std::uint8_t kUseColorFunctionSerialization = 1 << 1;
constexpr std::uint8_t kValid = 1 << 2;
constexpr std::uint8_t kOutOfLine = 1 << 3;
constexpr std::uint64_t kSRGB = 17;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ColorTextStream check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

WebCore::Color Inline(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha, std::uint8_t flags = 0) {
    const std::uint64_t packed = (static_cast<std::uint64_t>(red) << 24) | (static_cast<std::uint64_t>(green) << 16)
        | (static_cast<std::uint64_t>(blue) << 8) | alpha;
    return WebCore::Color{packed | (kSRGB << 56) | (static_cast<std::uint64_t>(kValid | flags) << 48)};
}

TextStream NewStream(bool multiLine = false, int indent = 0) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = 0;
    stream.multiLineMode = multiLine;
    stream.indent = indent;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Require((released.impl->hashAndFlags & 4) != 0);
    result.assign(static_cast<const char*>(released.impl->data), released.impl->length);
    return result;
}

std::string Dump(const WebCore::Color& color, bool multiLine = false, int indent = 0) {
    TextStream stream = NewStream(multiLine, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, color);
    Require(&returned == &stream);
    Require(stream.multiLineMode == multiLine);
    Require(stream.indent == indent);
    return Release(stream);
}

bool ThrowsNotImplemented(const WebCore::Color& color, std::string& left) {
    TextStream stream = NewStream();
    _ZN3WTF10TextStreamlsEPKc(&stream, "before ");
    bool thrown = false;
    try {
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, color);
    } catch (const std::runtime_error& error) {
        thrown = std::string(error.what()) == std::string(kExportName) + " not implemented";
    }
    left = Release(stream);
    return thrown;
}

void TestOpaque() {
    Require(Dump(Inline(0x12, 0xAB, 0x34, 0xFF)) == "#12AB34");
    Require(Dump(Inline(0x00, 0x00, 0x00, 0xFF)) == "#000000");
    Require(Dump(Inline(0xFF, 0xFF, 0xFF, 0xFF)) == "#FFFFFF");
    Require(Dump(Inline(0x0A, 0xB0, 0xC1, 0xFF), true, 4) == "#0AB0C1");
}

void TestTranslucent() {
    Require(Dump(Inline(0xFF, 0x00, 0x80, 0x7F)) == "#FF00807F");
    Require(Dump(Inline(0x0A, 0x0B, 0x0C, 0x00)) == "#0A0B0C00");
    Require(Dump(Inline(0xDE, 0xAD, 0xBE, 0xFE)) == "#DEADBEFE");
}

void TestInvalidAndSemantic() {
    Require(Dump(WebCore::Color{}) == "#00000000");
    Require(Dump(Inline(0x12, 0xAB, 0x34, 0xFF, kSemantic)) == "#12AB34");
    Require(Dump(Inline(0x12, 0xAB, 0x34, 0x40, kSemantic)) == "#12AB3440");
}

void TestAppends() {
    TextStream stream = NewStream();
    _ZN3WTF10TextStreamlsEPKc(&stream, "fill ");
    TextStream& first = _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, Inline(0x01, 0x02, 0x03, 0xFF));
    Require(&first == &stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, " stroke ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, Inline(0xA0, 0xB0, 0xC0, 0x80));
    Require(Release(stream) == "fill #010203 stroke #A0B0C080");
}

void TestUnsupported() {
    std::string left;
    WebCore::Color outOfLine{0x0000123456789AB0ULL | (static_cast<std::uint64_t>(kValid | kOutOfLine) << 48) | (kSRGB << 56)};
    Require(ThrowsNotImplemented(outOfLine, left));
    Require(left == "before ");
    Require(ThrowsNotImplemented(Inline(0x12, 0xAB, 0x34, 0xFF, kUseColorFunctionSerialization), left));
    Require(left == "before ");
}

}

int main() {
    TestOpaque();
    TestTranslucent();
    TestInvalidAndSemantic();
    TestAppends();
    TestUnsupported();
    return 0;
}
