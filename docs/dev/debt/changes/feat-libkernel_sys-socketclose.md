# libkernel_sys

## remove `t+rTx5NW+qU`

## add Functional

- [__sys_socketclose](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) - not measured on a console. It closes its descriptor through libkernel's `close` whether or not it is a socket, as syscall 114 does in [fpPS4](https://github.com/red-prig/fpPS4/blob/8157760fd67a5ad02a53925701325fff2ded2c05/sys/kern/uipc_syscalls.pas#L136-L139), which calls its `kern_close`, and in [RPCSX](https://github.com/RPCSX/rpcsx/blob/e8ae1481ab7ba04d5c6bef89dd852aabba2c88ff/kernel/orbis/src/sys/sys_sce.cpp#L61-L69); whether the console refuses a descriptor that is not a socket is unknown. Errors return -1 with errno, as FreeBSD 11.4's [`__sys_` stubs](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/amd64/SYS.h#L39-L50) do. It inherits the limits of [`close`](libkernel.md#unknown-function-info), such as `EPERM` for the host's descriptors 0 to 2.
