# libc

## add Functional

- [Host catch of guest exceptions on macOS](../../../core/libs/prx/libc/src/exception/Runtime.cpp) (libc) - catching a guest exception in host code while another guest exception is caught terminates, because libc++abi's `__cxa_begin_catch`, which host code calls, holds one foreign exception at a time (the nested `system_error` catch of `guest_libc_internal_exception_virtuals`). A host `catch` names a libc++ type, so a guest type matches it by name without libc++'s inline namespace (`NSt3__112system_errorE` and `St12system_error`); template types are not matched, and a handler reads the object through the guest layout. libc exports `_Unwind_Resume`, so cleanups in libc and in images linked against it resume through the guest unwinder; an image that binds it to libSystem aborts when a guest exception crosses one of its cleanups.
