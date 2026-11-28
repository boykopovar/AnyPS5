# libSceNKWebKit

## add Functional

- [JSC::PropertyDescriptor](../../../core/libs/prx/libSceNKWebKit/src/PropertyDescriptor.cpp) (libSceNKWebKit) - the 32-byte layout in [PropertyDescriptorAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/PropertyDescriptorAbi.hpp) follows [PropertyDescriptor.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/PropertyDescriptor.h) and is unverified against the PS5 module. `JSValue` arguments are taken as their encoded 64-bit word. `setDescriptor` stores non-cell words, masks `CustomValue` from the attributes and throws for empty and cell-like values; `setAccessorDescriptor` still throws `NotImplemented_nid_no_patch`. The cell path needs a `JSCell`/`GetterSetter` ABI and console verification.

## remove `_ZN3JSC18PropertyDescriptor11setWritableEb`

## remove `_ZN3JSC18PropertyDescriptor12setUndefinedEv`

## remove `_ZN3JSC18PropertyDescriptor13setDescriptorENS_7JSValueEj`

## remove `_ZN3JSC18PropertyDescriptor13setEnumerableEb`

## remove `_ZN3JSC18PropertyDescriptor15setConfigurableEb`

## remove `_ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE`

## remove `_ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE`

## remove `_ZNK3JSC18PropertyDescriptor10enumerableEv`

## remove `_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv`

## remove `_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv`

## remove `_ZNK3JSC18PropertyDescriptor8writableEv`
