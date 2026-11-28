#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKFrameInfo check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}  // namespace

int main() {
    Require(WKFrameInfoGetTypeID() == static_cast<std::uint32_t>(NKWebKit::ObjectType::FrameInfo));

    auto defaultInfo = NKWebKit::Make<NKWebKit::FrameInfo>();
    Require(WKGetTypeID(defaultInfo.Get()) == WKFrameInfoGetTypeID());
    const void* defaultHandle = WKFrameInfoCreateFrameHandleRef(defaultInfo.Get());
    Require(defaultHandle != nullptr);
    Require(WKGetTypeID(defaultHandle) == WKFrameHandleGetTypeID());
    Require(WKFrameHandleGetFrameID(defaultHandle) == 0);

    auto customInfo = NKWebKit::Make<NKWebKit::FrameInfo>(42);
    const void* customHandle = WKFrameInfoCreateFrameHandleRef(customInfo.Get());
    Require(customHandle != nullptr);
    Require(WKGetTypeID(customHandle) == WKFrameHandleGetTypeID());
    Require(WKFrameHandleGetFrameID(customHandle) == 42);

    auto largeInfo = NKWebKit::Make<NKWebKit::FrameInfo>(0x0123456789ABCDEFull);
    const void* largeHandle = WKFrameInfoCreateFrameHandleRef(largeInfo.Get());
    Require(largeHandle != nullptr);
    Require(WKGetTypeID(largeHandle) == WKFrameHandleGetTypeID());
    Require(WKFrameHandleGetFrameID(largeHandle) == 0x0123456789ABCDEFull);

    auto maxInfo = NKWebKit::Make<NKWebKit::FrameInfo>(UINT64_MAX);
    const void* maxHandle = WKFrameInfoCreateFrameHandleRef(maxInfo.Get());
    Require(maxHandle != nullptr);
    Require(WKGetTypeID(maxHandle) == WKFrameHandleGetTypeID());
    Require(WKFrameHandleGetFrameID(maxHandle) == UINT64_MAX);

    RequireInvalidArgument([] { WKFrameInfoCreateFrameHandleRef(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::Frame>();
    RequireInvalidArgument([&] { WKFrameInfoCreateFrameHandleRef(frame.Get()); });

    return 0;
}
