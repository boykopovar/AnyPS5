# libSceJsc

## add Functional

- [JSC::errorMesasgeForTransfer](../../../core/libs/prx/libSceJsc/src/ArrayBufferContents.cpp) (libSceJsc) - not compared with a console module; the console JSC version and the 64-byte `ArrayBuffer` layout (`GCIncomingRefCounted` header, `ArrayBufferContents` at 16 with the packed `m_shared` pointer at 30, `m_isWasmMemory` at 60) are unverified. It follows pinned [ArrayBuffer.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/ArrayBuffer.cpp#L410-L418): a shared buffer, then a WebAssembly memory, then the C API message; the locked-buffer assert is not checked, as in release builds. A null buffer throws. Closes when: compared with a console module.

## remove | `_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE` | `PGtPAOHyWqk` |
