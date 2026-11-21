# libSceNKWebKit

## add Functional

- [BasicColorMatrixFilterOperation and BasicComponentTransferFilterOperation](../../../core/libs/prx/libSceNKWebKit/src/BasicFilterOperations.cpp) (libSceNKWebKit) - console ABI, vtable layout and floating-point parity are unverified. Ten exports (`create`, `C1`/`C2`, `transformColor`, `blend` for both classes) follow pinned WebKit [FilterOperation.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/filters/FilterOperation.cpp) and [ColorMatrix.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/ColorMatrix.h) on the 24-byte `FilterOperation` layout with `WTF::fastMalloc`/`fastFree` and local vtables (RTTI typeinfo is null). Null or unreadable/unwritable guest pointers throw `NotImplemented_nid_no_patch` while preserving `errno`. Closes when: compared with a console module.
