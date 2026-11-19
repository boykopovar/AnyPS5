#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ScrollableAreaParametersDumpExports.hpp"
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
using WebCore::ScrollableAreaParameters;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ScrollableAreaParameters dump check failed at line %u\n", location.line());
        std::abort();
    }
}

std::string Dump(ScrollableAreaParameters parameters, bool multiLine, int indent = 0) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = multiLine;
    stream.indent = indent;
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(stream, parameters);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

void TestDefaults() {
    ScrollableAreaParameters parameters{WebCore::ScrollElasticityNone, WebCore::ScrollElasticityNone, WebCore::ScrollbarAuto, WebCore::ScrollbarAuto, false, false, false, false, false};
    Check(Dump(parameters, true) ==
          "\n(horizontal scroll elasticity 1)"
          "\n(vertical scroll elasticity 1)"
          "\n(horizontal scrollbar mode 0)"
          "\n(vertical scrollbar mode 0)");
    Check(Dump(parameters, false) ==
          " (horizontal scroll elasticity 1)"
          " (vertical scroll elasticity 1)"
          " (horizontal scrollbar mode 0)"
          " (vertical scrollbar mode 0)");
}

void TestAllFlags() {
    ScrollableAreaParameters parameters{WebCore::ScrollElasticityAllowed, WebCore::ScrollElasticityAutomatic, WebCore::ScrollbarAlwaysOn, WebCore::ScrollbarAlwaysOff, true, true, true, true, true};
    Check(Dump(parameters, true, 1) ==
          "\n  (horizontal scroll elasticity 2)"
          "\n  (vertical scroll elasticity 0)"
          "\n  (horizontal scrollbar mode 2)"
          "\n  (vertical scrollbar mode 1)"
          "\n  (has enabled horizontal scrollbar 1)"
          "\n  (has enabled vertical scrollbar 1)"
          "\n  (horizontal scrollbar hidden by style 1)"
          "\n  (vertical scrollbar hidden by style 1)");
}

void TestSomeFlags() {
    ScrollableAreaParameters parameters{WebCore::ScrollElasticityNone, WebCore::ScrollElasticityAllowed, WebCore::ScrollbarAlwaysOff, WebCore::ScrollbarAuto, false, true, true, false, true};
    Check(Dump(parameters, false) ==
          " (horizontal scroll elasticity 1)"
          " (vertical scroll elasticity 2)"
          " (horizontal scrollbar mode 1)"
          " (vertical scrollbar mode 0)"
          " (has enabled vertical scrollbar 1)"
          " (horizontal scrollbar hidden by style 1)");
}

void TestUnnamedEnumValues() {
    ScrollableAreaParameters parameters{static_cast<WebCore::ScrollElasticity>(200), static_cast<WebCore::ScrollElasticity>(3), static_cast<WebCore::ScrollbarMode>(255), static_cast<WebCore::ScrollbarMode>(7), false, false, false, false, false};
    Check(Dump(parameters, false) ==
          " (horizontal scroll elasticity 200)"
          " (vertical scroll elasticity 3)"
          " (horizontal scrollbar mode 255)"
          " (vertical scrollbar mode 7)");
}

}  // namespace

int main() {
    TestDefaults();
    TestAllFlags();
    TestSomeFlags();
    TestUnnamedEnumValues();
    return 0;
}
