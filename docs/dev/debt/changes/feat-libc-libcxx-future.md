# libc

## add Functional

- [libc++ future](../../../core/libs/prx/libc/src/LibcxxFuture.cpp) (libc) - the `std::__1` `promise<void>` and `future<void>` members, `__assoc_sub_state` (`wait`, `__sub_wait`, `set_value`, `set_exception`, `__on_zero_shared`, vtable, RTTI), `future_error` and `future_category` imported by RetroArch Azahar (PPSA99169, #5927) follow the [LLVM libc++ 17 future.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/future.cpp) layout and the Azahar relocations, not a console module; no firmware provider was measured. The `__shared_count` base RTTI and release are internal, not exports. `~promise` builds the `broken_promise` `exception_ptr` with `__cxa_init_primary_exception` instead of libc++ 17's throw and catch, so no handler sees it. `future(state)` with a null state and `future_error` with a null category throw `std::invalid_argument`.
