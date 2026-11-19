# libSceJsc

## add Functional

- [JSC::ClassInfo::hasStaticSetterOrReadonlyProperties](../../../core/libs/prx/libSceJsc/src/ClassInfo.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. It follows WebKit 73a35b43 [Structure.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/runtime/Structure.cpp#L1340-L1349): it walks `parentClass` (byte 8) from `this` and returns true when a `staticPropHashTable` (byte 16) has `hasSetterOrReadonlyProperties` (byte 8) set. The pinned 11.x fork has neither the function nor the flag, so the export dates from the older layout. A null `this` throws. Closes when: compared with a console module.

## remove | `_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv` | `iGxZRUg9NPM` |
