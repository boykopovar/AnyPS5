# libSceJsc

## add Functional

- [Handle set write barrier](../../../core/libs/prx/libSceJsc/src/HandleSet.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and layouts are unverified. It follows pinned [HandleSet.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/heap/HandleSet.cpp#L70-L89) and [HandleSet.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/heap/HandleSet.h#L43-L102): a handle slot is the 8-byte `JSValue` at offset 0 of a 24-byte node with `prev` at 8 and `next` at 16, the strong list sentinels start at offset 24 of `HandleSet` and the immediate list sentinels at 72, and cells use the 64-bit `NotCellMask` of 0xfffe000000000002. A null argument or a node outside a list throws. Closes when: compared with a console module.

## remove | `_ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_` | `oSOlHR1+AKw` |
