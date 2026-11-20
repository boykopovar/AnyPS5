# libSceJsc

## add Functional

- [JSC::CallFrame::isAnyWasmCallee](../../../core/libs/prx/libSceJsc/src/CallFrame.cpp) (libSceJsc) - not compared with a console module; assumes the console build has no WebAssembly. Pinned [CallFrame.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/interpreter/CallFrame.cpp#L171-L182) is true only for a Wasm callee or a `WebAssemblyModuleType` cell, and [OptionsPlayStation.cmake](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/cmake/OptionsPlayStation.cmake#L28-L31) turns off FTL, which [WebKitFeatures.cmake](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/cmake/WebKitFeatures.cmake#L251) makes WebAssembly depend on. It returns false without reading the frame; a null frame throws. Closes when: compared with a console module.

## remove | `_ZN3JSC9CallFrame15isAnyWasmCalleeEv` | `rsnmpSOdNzQ` |
