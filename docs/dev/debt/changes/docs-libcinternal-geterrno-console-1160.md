# libSceLibcInternal

## change [_Geterrno]

- [_Geterrno](../../../core/libs/prx/libSceLibcInternal/Err.cpp) (libSceLibcInternal) returns a private `thread_local` errno slot per calling thread. This matches the PS5 payload probe on firmware 11.60: the pointer differs from `libkernel.sprx` `__error`, stores through it are isolated, failing `open` and `close(-1)` update the libkernel slot without changing it, and a helper thread receives another pointer. The `int*(void)` prototype follows the Dinkumware `<errno.h>` public `errno`-macro description, not a linked copy. The probe compared only with `libkernel.sprx` `__error`; it did not compare with `libc.prx` `__error`, and no title process was observed, so title behavior remains unverified. `libSceLibcInternal.sprx` exports no `__error`.
