# libc

## add Functional

- [libc++ shared pointer control block](../../../core/libs/prx/libc/src/LibcxxSharedControl.cpp) (libc) supplies the five functions and weak-count RTTI imported by RetroArch's Azahar core (PPSA99169, #5927). The 64-bit layout and atomic reference counts follow [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/include/__memory/shared_ptr.h) and the Itanium ABI. The last weak release calls the guest's derived finalizer, and the weak-count RTTI encodes its private base. The stock core's Windows PE export limit remains, and the console provider and behavior have not been measured.
