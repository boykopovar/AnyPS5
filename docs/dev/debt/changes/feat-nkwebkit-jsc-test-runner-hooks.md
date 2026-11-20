# libSceNKWebKit

## add Silent stubs

- [JSC::setNeverInline and JSC::setNeverOptimize](../../../core/libs/prx/libSceNKWebKit/src/JscTestRunnerUtils.cpp) (libSceNKWebKit) - not compared with a console module. Pinned [TestRunnerUtils.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/JavaScriptCore/runtime/TestRunnerUtils.cpp) and [JSCTestRunnerUtils.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/JavaScriptCore/API/JSCTestRunnerUtils.cpp) return `undefined` for every argument and only flag the function's executable as a JIT tier-up hint. The HLE runs scripts without JIT tiers, so the hint is dropped: the C API overloads return the context's `undefined` value (`nullptr` for a null context) and the `JSGlobalObject*, CallFrame*` overloads return the encoded `undefined` (`0xa`), as `optimizeNextInvocation` already does. Closes when: compared with a console module.

## remove `_ZN3JSC14setNeverInlineEPK15OpaqueJSContextPK13OpaqueJSValue`

## remove `_ZN3JSC14setNeverInlineEPNS_14JSGlobalObjectEPNS_9CallFrameE`

## remove `_ZN3JSC16setNeverOptimizeEPK15OpaqueJSContextPK13OpaqueJSValue`

## remove `_ZN3JSC16setNeverOptimizeEPNS_14JSGlobalObjectEPNS_9CallFrameE`
