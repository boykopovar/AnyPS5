# libSceJsc

## add Functional

- [JSC::FreeList](../../../core/libs/prx/libSceJsc/src/FreeList.cpp) (libSceJsc) - the console JavaScriptCore version is unverified. `initializeBump` and `initializeList` follow [FreeList.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/heap/FreeList.cpp#L49-L66), whose export names match these; the 40-byte layout (`m_scrambledHead`, `m_secret`, `m_payloadEnd`, `m_remaining`, `m_originalSize`, `m_cellSize`) comes from [FreeList.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/heap/FreeList.h) and is not measured on a console. A null list throws where WebKit writes through it.

## remove `_ZN3JSC8FreeList14initializeBumpEPcj`

## remove `_ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj`
