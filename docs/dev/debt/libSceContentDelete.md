# libSceContentDelete technical debt

### Functional

- [sceContentDeleteById](../../../core/libs/prx/libSceContentDelete/Export.cpp) and `sceContentDeleteByPath`: content database access and deletion are not implemented. Calls with valid arguments after initialization throw; the backend result cannot be inferred from an empty host library. Null paths after initialization throw instead of reproducing the original unchecked string access.
- [sceContentDeleteInitialize](../../../core/libs/prx/libSceContentDelete/Export.cpp): validates the confirmed 16 KiB heap requirement and lifecycle errors, but does not create the original allocator, mutex and database client. Initialization-parameter fields other than the 64-bit heap size at offset 8 remain unresolved.

### Unknown function info

- [sceContentDeleteGlobalMstat](../../../core/libs/prx/libSceContentDelete/Export.cpp), `sceContentDeleteMemoryInitialize` and `sceContentDeleteMemoryTerminate`: neither exports nor callers were found in the supplied decompiled library corpus; their signatures remain unresolved.
