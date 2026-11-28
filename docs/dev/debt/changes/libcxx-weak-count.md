# libc

## add Functional

- [libc++ weak reference promotion and release](../../../core/libs/prx/libc/src/LibcxxWeakCount.cpp) follow LLVM's [shared count layout](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/include/__memory/shared_ptr.h) and [reference counting](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/memory.cpp). The Azahar core in RetroArch PPSA99169 v1.0.0-beta.1 imports both and has a control-block vtable with the final weak cleanup at byte 32 from the address point. Every weak release decrements the signed biased count. The final callback sees -1 even for a sole owner, where LLVM can leave 0. The console provider and complete core execution are unverified. Checking that the control block is aligned and writable and rejecting an unreadable vtable or null cleanup entry are host safeguards for invalid objects.
