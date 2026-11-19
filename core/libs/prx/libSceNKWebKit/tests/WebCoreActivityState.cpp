#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ActivityStateExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WebCoreAbi::ActivityStateFlags;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ActivityState dump check failed at line %u\n", location.line());
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

std::string Dump(std::uint32_t flags, const char* prefix = nullptr) {
    TextStream stream = NewStream();
    if (prefix) _ZN3WTF10TextStreamlsEPKc(&stream, prefix);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamENS0_9OptionSetINS_13ActivityState4FlagEEE(stream, ActivityStateFlags{flags});
    Check(&returned == &stream);
    return Release(stream);
}

void TestEmpty() {
    Check(Dump(0).empty());
    Check(Dump(0, "state: ") == "state: ");
}

void TestSingleFlags() {
    Check(Dump(1u << 0) == "active window");
    Check(Dump(1u << 1) == "focused");
    Check(Dump(1u << 2) == "visible");
    Check(Dump(1u << 3) == "visible or occluded");
    Check(Dump(1u << 4) == "in-window");
    Check(Dump(1u << 5) == "visually idle");
    Check(Dump(1u << 6) == "audible");
    Check(Dump(1u << 7) == "loading");
    Check(Dump(1u << 8) == "capturing media");
}

void TestCombinedFlagsKeepDeclarationOrder() {
    Check(Dump((1u << 7) | (1u << 1)) == "focused, loading");
    Check(Dump((1u << 8) | (1u << 4) | (1u << 0)) == "active window, in-window, capturing media");
    Check(Dump(0x1ffu) == "active window, focused, visible, visible or occluded, in-window, visually idle, audible, loading, capturing media");
    Check(Dump(1u << 2, "[") == "[visible");
}

void TestUndeclaredBitsAreIgnored() {
    Check(Dump(1u << 9).empty());
    Check(Dump(0xfffffe00u).empty());
    Check(Dump((1u << 9) | (1u << 6)) == "audible");
    Check(Dump(0xffffffffu) == "active window, focused, visible, visible or occluded, in-window, visually idle, audible, loading, capturing media");
}

}  // namespace

int main() {
    TestEmpty();
    TestSingleFlags();
    TestCombinedFlagsKeepDeclarationOrder();
    TestUndeclaredBitsAreIgnored();
    return 0;
}
