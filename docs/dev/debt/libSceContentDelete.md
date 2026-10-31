# libSceContentDelete technical debt

### Functional

- [sceContentDeleteById](../../../core/libs/prx/libSceContentDelete/Export.cpp) and `sceContentDeleteByPath`: content database access and deletion are not implemented. Calls with valid arguments after initialization go through `NotImplemented_nid_no_patch`: the console's error for content that is not in the library is unknown, and it cannot be inferred from an empty host library. Null paths after initialization throw instead of reproducing the original unchecked string access.
- [sceContentDeleteInitialize](../../../core/libs/prx/libSceContentDelete/Export.cpp): validates the confirmed 16 KiB heap requirement and lifecycle errors, but does not create the original allocator, mutex and database client. Initialization-parameter fields other than the 64-bit heap size at offset 8 remain unresolved.

### Unknown function info

- [sceContentDeleteByIdListForSf](../../../core/libs/prx/libSceContentDelete/Export.cpp): the shim declares `const std::int64_t*` and `std::size_t` parameters. The decompiled 3.20 export validates `count < 1` (i.e. zero count) and a null pointer as `InvalidArgument` (0x809D5001) before reaching the unpatched body, and `NotInitialized` (0x809D5002) when the library state is unset. List semantics, memory access, results and side effects beyond the argument validation remain unresolved.
- [sceContentDeleteGlobalMstat](../../../core/libs/prx/libSceContentDelete/Export.cpp), `sceContentDeleteMemoryInitialize` and `sceContentDeleteMemoryTerminate`: neither matching exports nor callers were found in the supplied decompiled library corpus. Their current `int APS5_VABI(void)` shim declarations do not establish the console ABI. The bodies call `NotImplemented_nid_no_patch` without accessing arguments or state; signatures, lifecycle requirements, results and side effects remain unresolved.
