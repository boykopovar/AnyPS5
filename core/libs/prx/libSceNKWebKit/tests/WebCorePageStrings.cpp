#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore page strings check failed at line %u\n", location.line());
        std::abort();
    }
}

using WatchAdaptationName = NKWebKit::WtfAbi::String (APS5_VABI *)();
using ReasonsAsString = NKWebKit::WtfAbi::String (APS5_VABI *)(std::uint8_t);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19watchAdaptationNameEv), WatchAdaptationName>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore24PerformanceLoggingClient35synchronousScrollingReasonsAsStringEN3WTF9OptionSetINS_26SynchronousScrollingReasonEEE), ReasonsAsString>);

std::string Text(const NKWebKit::WtfAbi::String& value) {
    Require(value.impl != nullptr);
    Require((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

std::string Reasons(std::uint8_t reasons) {
    return Text(_ZN7WebCore24PerformanceLoggingClient35synchronousScrollingReasonsAsStringEN3WTF9OptionSetINS_26SynchronousScrollingReasonEEE(reasons));
}

void TestWatchAdaptationName() {
    Require(Text(_ZN7WebCore19watchAdaptationNameEv()) == "watch");
    std::printf("WebCore::watchAdaptationName: returns watch passed\n");
}

void TestSingleReasons() {
    Require(Reasons(0x00).empty());
    Require(Reasons(0x01) == "forced");
    Require(Reasons(0x02) == "viewport-constrained objects");
    Require(Reasons(0x04) == "non-layer viewport-constrained objects");
    Require(Reasons(0x08) == "image document");
    Require(Reasons(0x10) == "slow-repaint objects");
    Require(Reasons(0x20).empty());
    std::printf("WebCore::PerformanceLoggingClient::synchronousScrollingReasonsAsString: empty set and each reason passed\n");
}

void TestCombinedReasons() {
    Require(Reasons(0x11) == "forced,slow-repaint objects");
    Require(Reasons(0x0A) == "viewport-constrained objects,image document");
    Require(Reasons(0x1F) == "forced,slow-repaint objects,viewport-constrained objects,non-layer viewport-constrained objects,image document");
    Require(Reasons(0x3F) == "forced,slow-repaint objects,viewport-constrained objects,non-layer viewport-constrained objects,image document");
    Require(Reasons(0x21) == "forced");
    std::printf("WebCore::PerformanceLoggingClient::synchronousScrollingReasonsAsString: source order and trailing comma strip passed\n");
}

}

int main() {
    TestWatchAdaptationName();
    TestSingleReasons();
    TestCombinedReasons();
}
