# libSceJsc

## add Functional

- [JSC::JSCell::destroy](../../../core/libs/prx/libSceJsc/src/JsCellDestroy.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. It follows pinned [JSCell.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/JSCell.cpp#L39-L42), which only runs `JSCell::~JSCell()`. [JSCell.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/JSCell.h) declares no destructor and holds only trivial members (structure ID, indexing type, type, inline flags, cell state), so the destructor is trivial and the export returns without touching the cell. Closes when: compared with a console module.

## remove | `_ZN3JSC6JSCell7destroyEPS0_` | `85S6tE7Yj9Q` |
