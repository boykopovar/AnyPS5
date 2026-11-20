#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC test runner utils check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}

int main() {
    auto* ctx = JSGlobalContextCreate(nullptr);
    Require(ctx != nullptr);
    const auto undefined = JSValueMakeUndefined(ctx);
    const NKWebKit::JSValueRef values[] = {nullptr, undefined, JSValueMakeNull(ctx), JSValueMakeBoolean(ctx, true), JSValueMakeNumber(ctx, 42.5)};
    for (const auto value : values) {
        const void* inlineResult = _ZN3JSC14setNeverInlineEPK15OpaqueJSContextPK13OpaqueJSValue(ctx, value);
        const void* optimizeResult = _ZN3JSC16setNeverOptimizeEPK15OpaqueJSContextPK13OpaqueJSValue(ctx, value);
        Require(inlineResult == undefined);
        Require(optimizeResult == undefined);
        Require(JSValueIsUndefined(ctx, static_cast<NKWebKit::JSValueRef>(inlineResult)));
    }
    Require(_ZN3JSC14setNeverInlineEPK15OpaqueJSContextPK13OpaqueJSValue(nullptr, undefined) == nullptr);
    Require(_ZN3JSC16setNeverOptimizeEPK15OpaqueJSContextPK13OpaqueJSValue(nullptr, undefined) == nullptr);
    JSGlobalContextRelease(ctx);

    alignas(8) unsigned char globalObject[16] = {};
    alignas(8) unsigned char callFrame[64] = {};
    Require(_ZN3JSC14setNeverInlineEPNS_14JSGlobalObjectEPNS_9CallFrameE(nullptr, nullptr) == 0xa);
    Require(_ZN3JSC14setNeverInlineEPNS_14JSGlobalObjectEPNS_9CallFrameE(globalObject, callFrame) == 0xa);
    Require(_ZN3JSC16setNeverOptimizeEPNS_14JSGlobalObjectEPNS_9CallFrameE(nullptr, nullptr) == 0xa);
    Require(_ZN3JSC16setNeverOptimizeEPNS_14JSGlobalObjectEPNS_9CallFrameE(globalObject, callFrame) == 0xa);
    return 0;
}
