# libkernel

## change [kern.proc.pathname]

- [kern.proc.pathname](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - `sysctl` {`CTL_KERN`, `KERN_PROC`, `KERN_PROC_PATHNAME`, pid} fails with `ENOENT` and leaves the buffer and the length alone, as a payload process on a PS5 (firmware 11.60) [measured](https://github.com/boykopovar/AnyPS5/pull/22788#issuecomment-6974328583) for pid -1, its own pid, pid 1 and 0x7fffffff, with a 1,024-byte buffer, a 1-byte buffer and a size query; a write fails with `EPERM`, as measured there. Whether a title process gets the same is unverified, and a MIB without exactly one pid keeps `EINVAL`, which was not measured.
