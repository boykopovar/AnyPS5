# libSceNpCppWebApi

## add Functional

- [Common::LibContext arena lifetime](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - `Common::terminate` keeps the arena mapped and the mspace alive until the same `LibContext` is initialized again, where the module bundled with PPSA21564/PPSA23732 releases it as it terminates. A `String`, `Vector` body or title-provided object that outlives `terminate` is therefore released against the retained arena instead of faulting on unmapped host memory, and the arena is reclaimed on reinitialization or at process exit. Reinitializing the same `LibContext` drains the arena operations still in flight before releasing it, so the old mapping is unmapped inside `initialize` rather than later from another thread; the host-side state record lives outside the arena. Console behavior for an object used after `terminate` is unverified and is guest misuse.
