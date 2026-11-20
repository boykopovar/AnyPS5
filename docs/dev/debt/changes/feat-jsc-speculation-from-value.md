# libSceJsc

## add Functional

- [JSC::speculationFromValue](../../../core/libs/prx/libSceJsc/src/SpeculatedType.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. It follows the PS5 11.x [SpeculatedType.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/SpeculatedType.cpp#L582-L607) with the bits of its [SpeculatedType.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/SpeculatedType.h#L75-L102) on the 64-bit encoding of [JSCJSValue.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSCJSValue.h#L461-L500); `USE_BIGINT32` is 0 there, so no value is a BigInt32. A cell, which needs `speculationFromCell` and its structure, throws: libSceJsc creates no cells. Closes when: compared with a console module.

## remove | `_ZN3JSC20speculationFromValueENS_7JSValueE` | `K9Mk0uRkFII` |
