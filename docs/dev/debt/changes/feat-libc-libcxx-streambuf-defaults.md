# libc

## add Functional

- [libc++ narrow stream-buffer defaults](../../../core/libs/prx/libc/src/LibcxxStreambuf.cpp) (libc) implements the base `sync`, `setbuf`, `overflow`, `pbackfail`, `showmanyc`, and `underflow` methods imported by the Azahar core (#5927). Their results and lack of object mutation match [LLVM libc++ 18.1.8](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/include/streambuf) and the [PS5 Payload SDK v0.42](https://github.com/ps5-payload-dev/sdk/releases/tag/v0.42) object. Physical firmware export provenance and full title execution are unverified.
