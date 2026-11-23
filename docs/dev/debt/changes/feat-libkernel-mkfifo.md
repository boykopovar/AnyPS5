# libkernel

## add Functional

- [mkfifo](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) - creates the FIFO through the host `mkfifo` at the same resolved path as `mkdir`, with the host umask applied to `mode`, as `mkdir` does. The console's umask and error mapping are unverified. Windows has no FIFOs and throws `NotImplemented`.
