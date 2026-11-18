# libScePosixForWebKit

## remove mmap_np, msetname, pthread_setschedparam_np, setenv_np, sysinfo

## add Functional

- [mmap_np, msetname, setenv_np and scePosixForWebKit_sysinfo](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) - from firmware 13.42 disassembly (#21870), unmeasured on a console. `setenv_np` manages a 128-slot module environment table (not libkernel's environment), which `scePosixForWebKit_sysinfo` (`9Vqxt-qBPB4`) reads once on `TOTAL_MEMORY_SIZE_ASSUMPTION` (default `0x80000000`) to write `{total, 1}` with `EFAULT` on null. `msetname` prefixes non-`SceNK` names up to 31 bytes via `sceKernelSetVirtualRangeName`. `mmap_np` checks invalid `MAP_FIXED`/alignment `EINVAL` on anonymous private mappings before throwing, and forwards other mappings to `mmap` with tag naming except on `ENOMEM`. Host safety: unreadable strings or unwritable `sysinfo` throw `std::invalid_argument` preserving `errno`.

- [pthread_setschedparam_np](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) - for `SCHED_OTHER`, the host does not apply the console's timesharing scheduling class, and a null or unreadable parameter returns `EFAULT` instead of taking the console fault. Firmware 13.42 reads the priority first and calls the import identified as `pthread_setschedother_np(thread, priority)`, represented here by libkernel's measured priority setter. Other policies forward to `pthread_setschedparam` as in that firmware.
