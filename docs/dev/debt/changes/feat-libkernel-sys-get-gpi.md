# libkernel_sys

## remove `3JYhkiIHkJc`

## add Functional

- [`__sceKernelGetGPI`](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) forwards to `sceKernelGetGPI()`, returning the 64-bit general-purpose input register value. Unverified on a retail console. Closes when: compared with a retail PS5.

# libkernel_web

## add Functional

- [`__sceKernelGetGPI`](../../../core/libs/prx/libkernel_web/Export.cpp) (libkernel_web) forwards to `__sceKernelGetGPI` in `libkernel_sys`. Unverified on a retail console. Closes when: compared with the console.
