#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ColorDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

constexpr std::uint64_t kValid = 1ULL << 50;
constexpr std::uint64_t kSrgb = 17ULL << 56;
constexpr std::uint64_t kSemantic = 1ULL << 48;
constexpr std::uint64_t kUseColorFunctionSerialization = 1ULL << 49;
constexpr std::uint64_t kOutOfLine = 1ULL << 51;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Color dump check failed at line %u\n", location.line());
        std::abort();
    }
}

TextStream NewStream() {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = true;
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

std::string Dump(std::uint64_t bits, const char* prefix = nullptr) {
    TextStream stream = NewStream();
    if (prefix) _ZN3WTF10TextStreamlsEPKc(&stream, prefix);
    const WebCore::Color color{bits};
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, color);
    Check(&returned == &stream);
    return Release(stream);
}

void TestOpaqueColor() {
    Check(Dump(0xFF0080FFULL | kValid | kSrgb) == "#FF0080");
    Check(Dump(0x000000FFULL | kValid | kSrgb) == "#000000");
    Check(Dump(0xFFFFFFFFULL | kValid | kSrgb) == "#FFFFFF");
}

void TestTranslucentColor() {
    Check(Dump(0x0A141E80ULL | kValid | kSrgb) == "#0A141E80");
    Check(Dump(0x01020300ULL | kValid | kSrgb) == "#01020300");
}

void TestInvalidColor() {
    Check(Dump(0) == "#00000000");
}

void TestSemanticFlagDoesNotChangeTheText() {
    Check(Dump(0x11223344ULL | kValid | kSrgb | kSemantic) == "#11223344");
}

void TestAppendsAfterExistingText() {
    Check(Dump(0x336699FFULL | kValid | kSrgb, "color ") == "color #336699");
}

void TestChainedDumps() {
    TextStream stream = NewStream();
    const WebCore::Color first{0xFF000080ULL | kValid | kSrgb};
    const WebCore::Color second{0x00FF00FFULL | kValid | kSrgb};
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, first);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, second);
    Check(Release(stream) == "#FF000080 #00FF00");
}

void TestUnsupportedColorsThrowWithoutWriting() {
    for (const std::uint64_t bits : {0x0A141E80ULL | kValid | kSrgb | kUseColorFunctionSerialization, 0x123450ULL | kValid | kOutOfLine}) {
        TextStream stream = NewStream();
        _ZN3WTF10TextStreamlsEPKc(&stream, "before");
        const WebCore::Color color{bits};
        bool thrown = false;
        try {
            _ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE(stream, color);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        Check(thrown);
        Check(Release(stream) == "before");
    }
}

}  // namespace

int main() {
    TestOpaqueColor();
    TestTranslucentColor();
    TestInvalidColor();
    TestSemanticFlagDoesNotChangeTheText();
    TestAppendsAfterExistingText();
    TestChainedDumps();
    TestUnsupportedColorsThrowWithoutWriting();
    std::puts("WebCore Color dump checks passed");
    return 0;
}
