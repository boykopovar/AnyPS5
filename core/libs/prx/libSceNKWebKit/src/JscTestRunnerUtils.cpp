#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>

namespace {

constexpr std::uint64_t EncodedUndefined = 0xa;

}

extern "C" {

const void* APS5_VABI _ZN3JSC14setNeverInlineEPK15OpaqueJSContextPK13OpaqueJSValue(const void* context, const void*) {
    return JSValueMakeUndefined(static_cast<NKWebKit::JSContextRef>(context));
}

std::uint64_t APS5_VABI _ZN3JSC14setNeverInlineEPNS_14JSGlobalObjectEPNS_9CallFrameE(void*, void*) {
    return EncodedUndefined;
}

const void* APS5_VABI _ZN3JSC16setNeverOptimizeEPK15OpaqueJSContextPK13OpaqueJSValue(const void* context, const void*) {
    return JSValueMakeUndefined(static_cast<NKWebKit::JSContextRef>(context));
}

std::uint64_t APS5_VABI _ZN3JSC16setNeverOptimizeEPNS_14JSGlobalObjectEPNS_9CallFrameE(void*, void*) {
    return EncodedUndefined;
}

}
