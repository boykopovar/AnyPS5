# libkernel

## change [hw.realmem]

- [hw.realmem](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - `sysctl` {`CTL_HW`, 12} and `sysctlbyname` report the same 8-byte value as `hw.physmem` (the guest physical-page count times 16 KiB), read-only with `EPERM` for a write and `ENOMEM` for a short buffer. Payload processes on PS5 firmware 12.70 and a PS5 Pro on 13.60 instead returned `ENOENT` for reads and size queries of both `hw.physmem` and `hw.realmem`; the 13.60 write probe returned `EPERM`, so the kernel reaches permission checking even though reads cannot resolve the node. `hw.usermem64` also returned `ENOENT`. Title-process behavior was not measured. Dolphin reads `hw.realmem` as machine memory in RetroArch's `dolphin_libretro` core.

## change [vm.overcommit]

- [vm.overcommit](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - `sysctlbyname` returns -1 with `ENOENT` without changing the caller buffer or length for reads, size queries, a null length and writes. A payload process on a PS5 (firmware 12.70) measured the read, size-query and null-length cases; a PS5 Pro payload process (13.60) reproduced them and measured the write as `ENOENT` too. `sysctlnametomib` also returns `ENOENT`. A title process was not measured. Firmware 10.01 kernel data contains a read-write node holding 0, so the missing name appears to be process/namespace visibility rather than absence from the kernel. PS5MC (PPSA99420) probes the name in mimalloc's overcommit detection.
