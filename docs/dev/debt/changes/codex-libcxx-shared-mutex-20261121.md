# libc

## add Unknown function info

- [std::__1::__shared_mutex_base](../../../core/libs/prx/libc/src/LibcxxSharedMutex.cpp) (libc) implements the five constructor, exclusive-lock and shared-lock exports imported by the Azahar Game bring-up in [#5927](https://github.com/boykopovar/AnyPS5/issues/5927#issuecomment-7022427172). The 32-byte object layout and writer-preferring two-gate algorithm follow [LLVM libc++ 17.0.6](https://github.com/llvm/llvm-project/blob/6009708b4367171ccdbf4b5905cb6a803753fe18/libcxx/src/shared_mutex.cpp) and its [class definition](https://github.com/llvm/llvm-project/blob/6009708b4367171ccdbf4b5905cb6a803753fe18/libcxx/include/shared_mutex), using the existing guest pthread mutex and condition-variable bridges. The console's libc++ version, object layout, exception behavior, and title execution remain unverified.
