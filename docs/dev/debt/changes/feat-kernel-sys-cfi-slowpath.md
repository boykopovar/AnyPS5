# libkernel_sys

## remove `Bq2a8HmmUoY`

## remove `-cbfK7bCKQU`

## add Functional

- [__cfi_slowpath and __cfi_slowpath_diag](../../../core/libs/prx/libkernel_sys/CfiSlowPath.cpp) (libkernel_sys) follow LLVM 18.1.8's [compiler-rt `CfiSlowPathCommon`](https://github.com/llvm/llvm-project/blob/llvmorg-18.1.8/compiler-rt/lib/cfi/cfi.cpp#L345-L367), unmeasured on a console. How the console's libkernel finds a module's `__cfi_check` is unknown (the name is in no NID database); here it is the module's own export `__cfi_check`, `iSB5fJWVqIU` or either with `#guest`. A target outside every module's `PT_LOAD` pages, or below its `__cfi_check` page, throws `std::runtime_error` where compiler-rt traps; with diagnostic data `__cfi_slowpath_diag` reports it through `__ubsan_handle_cfi_check_fail` and returns. A module without `__cfi_check`, host libraries included, is unchecked; otherwise its `__cfi_check(id, target, diagData)` is called. Linux only; elsewhere both throw through `NotImplemented_nid_no_patch`.
