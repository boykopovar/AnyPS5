# libSceJsc

## add Functional

- [JSC::JSCell::getCallData and getConstructData](../../../core/libs/prx/libSceJsc/src/JsCellCallData.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. Both follow pinned [JSCell.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/JSCell.cpp#L91-L99), which returns an empty `CallData` without reading the cell: the 24-byte [CallData](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/CallData.h) (type byte `None`, then the 16-byte union) is zeroed and returned through the hidden SysV result pointer. Closes when: compared with a console module.

## remove | `_ZN3JSC6JSCell11getCallDataEPS0_` | `JeH+i51Qf2I` |

## remove | `_ZN3JSC6JSCell16getConstructDataEPS0_` | `9gXDUNzVOiM` |
