#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/TouchActionExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WebCoreAbi::TouchAction;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "TouchAction dump check failed at line %u\n", location.line());
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

std::string Dump(std::uint8_t value, const char* prefix = nullptr) {
    TextStream stream = NewStream();
    if (prefix) _ZN3WTF10TextStreamlsEPKc(&stream, prefix);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(stream, static_cast<TouchAction>(value));
    Check(&returned == &stream);
    return Release(stream);
}

void TestEachValue() {
    Check(Dump(1u << 0) == "auto");
    Check(Dump(1u << 1) == "none");
    Check(Dump(1u << 2) == "manipulation");
    Check(Dump(1u << 3) == "pan-x");
    Check(Dump(1u << 4) == "pan-y");
    Check(Dump(1u << 5) == "pinch-zoom");
}

void TestAppendsAfterExistingText() {
    Check(Dump(1u << 3, "touch-action: ") == "touch-action: pan-x");
    Check(Dump(1u << 5, "[") == "[pinch-zoom");
}

void TestUndeclaredValuesWriteNothing() {
    Check(Dump(0).empty());
    Check(Dump((1u << 3) | (1u << 4)).empty());
    Check(Dump(1u << 6).empty());
    Check(Dump(0xff).empty());
    Check(Dump((1u << 3) | (1u << 4), "touch-action: ") == "touch-action: ");
}

}  // namespace

int main() {
    TestEachValue();
    TestAppendsAfterExistingText();
    TestUndeclaredValuesWriteNothing();
    return 0;
}
