# libc

## add Functional

- [std::__1 libc++ atomic monitor, wait and notify one](../../../core/libs/prx/libc/src/AtomicWait.cpp) (libc) - implements the three 64-bit `__cxx_atomic_impl<long>` imports listed for Azahar in RetroArch (PPSA99169, #5927). [LLVM libc++ 17's FreeBSD implementation](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/atomic.cpp#L74-L90) reads the signed 64-bit atomic with acquire order, waits once while it equals the monitored value, and wakes one waiter after a notification. This implementation uses the existing libkernel-backed 64-bit wait and wake exports, and rejects invalid or unaligned guest storage before dereferencing it. The console provider, exact firmware ABI, and behavior if guest code unmaps an atomic during a wait have not been measured.
