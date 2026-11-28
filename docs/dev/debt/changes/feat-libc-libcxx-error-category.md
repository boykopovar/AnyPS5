# libc

## add Functional

- [libc++ error category](../../../core/libs/prx/libc/src/LibcxxErrorCategory.cpp) (libc) implements the base `default_error_condition` and both `equivalent` overloads imported by the Azahar core (#5927). The [LLVM libc++ 18.1.8 implementation](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/error_category.cpp) and [PS5 Payload SDK v0.42](https://github.com/ps5-payload-dev/sdk/releases/tag/v0.42) support the base mapping and the virtual call to the category’s override. Firmware export provenance and full title execution still need verification.
