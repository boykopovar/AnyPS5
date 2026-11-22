# libkernel_sys

## add Functional

- [sceKernelNormalizePath](../../../core/libs/prx/libkernel_sys/NormalizePath.cpp) (libkernel_sys) - an unreadable input path or unwritable output buffer returns `SCE_KERNEL_ERROR_EFAULT` (`0x8002000e`) with `errno` preserved as a host-safety fallback, where firmware 13.40 `libkernel_sys.sprx` (SHA-256 `c4d42cae35edf564e1cd6cfc8fe6815b8ed07a13f95d61aef04047cd804f068f`, export `KB7Vc2+5S54` at `0x29ce0`, identical to `libkernel.sprx` `0x28fd0`) faults; a faulting output across a page boundary may be partly written. Behaviour is read from disassembly and verified against the mapped firmware module, not measured on a console. Closes when: measured on a PS5
