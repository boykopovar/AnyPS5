# libSceJsc

## remove | `_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv` | `iGxZRUg9NPM` |

## add Functional

- [ClassInfo static setter or readonly query](../../../core/libs/prx/libSceJsc/src/ClassInfo.cpp) (libSceJsc) - console layout and behavior are unmeasured; the console JavaScriptCore version is unverified. The parent-chain query follows WebKit [Structure.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/Structure.cpp#L1384-L1393). LP64 prefixes derived from that revision's [ClassInfo.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/ClassInfo.h#L192-L205) and [Lookup.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/Lookup.h#L107-L114) put `parentClass` at byte 8, `staticPropHashTable` at byte 16 and `HashTable::hasSetterOrReadonlyProperties` at byte 8. Only these prefixes are modeled. Closes when: compared with a console module.
