# libSceNKWebKit

## add Functional

- [convertToInteger and convertToIntegerClamp](../../../core/libs/prx/libSceNKWebKit/src/DomConvertNumbers.cpp) (libSceNKWebKit) - not compared with a console module. The `JSC::JSValue` argument is read as the JSVALUE64 encoding of the pinned [JSCJSValue.h](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/JavaScriptCore/runtime/JSCJSValue.h#L393-L483): int32 under `NumberTag`, doubles offset by 2^49, and the null, undefined and boolean immediates. Cells (strings, objects, symbols, BigInt heap values) and BigInt32 immediates throw because `JSValue::toNumber` needs the JSC runtime; `JSGlobalObject` is never read. `convertToIntegerEnforceRange` stays a throwing stub because it raises a JS `TypeError`. Closes when: compared with a console module and cell values go through the engine.

## remove `_ZN7WebCore16convertToIntegerIaEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerIhEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerIiEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerIjEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerIlEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerImEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerIsEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore16convertToIntegerItEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampIaEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampIhEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampIiEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampIjEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampIlEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampImEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampIsEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`

## remove `_ZN7WebCore21convertToIntegerClampItEET_RN3JSC14JSGlobalObjectENS2_7JSValueE`
