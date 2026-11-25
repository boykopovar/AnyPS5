# libc

## add Functional

- [libc++ thread state and thread-local data](../../../core/libs/prx/libc/src/LibcxxThreadState.cpp) (libc) provide the LLVM libc++17 `__thread_struct` constructor and destructor and the process-wide `__thread_local_data` key used by its inline `__thread_specific_ptr` accessors. The guest object holds one pointer and the key uses the existing libkernel pthread-specific storage and thread-exit destructor. Azahar's separately converted RetroArch core imports all three names; its unqualified provider and console ABI are not measured. The private state's deferred condition notifications and async readiness paths are not supplied because Azahar does not import their registration methods; a core using those methods remains unsupported.
