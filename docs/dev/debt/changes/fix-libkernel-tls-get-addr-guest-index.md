# libkernel

## add Functional

- [__tls_get_addr](../../../core/libs/prx/libkernel/Module/src/Tls.cpp) guest pointer safety assumes an unreadable `KernelTlsIndex` or Windows `module` slot address can be rejected with `std::invalid_argument` while preserving `errno`, matching its existing null-index contract rather than crashing the host.
