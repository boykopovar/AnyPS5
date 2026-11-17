# libSceAgcDriver

## change [Label read traps]

- [Label read traps](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp) (libSceAgcDriver, macOS, Linux): at a label, a pending linear or small storage image that shares no page with another image a label stores or one being stored, is not direct memory mapped twice and, on Linux, stores over a host import (CPU stores skip trapped pages), gets a [read trap](../../../core/libs/prx/libc/src/GuestWriteWatch.cpp) instead of its store. The first access to its pages stores it whole through the host-read hook; a store of another image over it, eviction or explicit write-back stores it first. Other stores release the trap once their GPU copy is recorded, so an earlier read sees old bytes; a CPU store first releases the traps on its pages until the next label. A `GpuMutex` holder that touches a trapped page opens it unstored to all threads until the next label or its store; a system call reading one without the hook fails with `EFAULT`, and other double mappings go undetected.

## add Functional

- [Read traps over host pointer imports](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) (libSceAgcDriver, Linux): a label read trap is set only where every import over the image's pages is a dma-buf, or where the driver is NVIDIA's proprietary one (`ReadTrapsOverHostPointerImportsUsable`). On RADV a submission fails with `VK_ERROR_DEVICE_LOST` while a host pointer import has a `PROT_NONE` page; NVIDIA pins the pages at import and no device was lost in runs there, but neither driver documents it. Other drivers are not measured and keep host pointer imports untrapped, so their images there are stored at the label. A retired host pointer import that recorded work still holds is not checked.

# libc

## add Functional

- On Linux the [write watch](../../../core/libs/prx/libc/src/GuestWriteWatch.cpp) read traps (`GuestWriteWatchTrapReads`) make pages `PROT_NONE`, unlike its `userfaultfd` write recording, so they share the gaps of the macOS read traps above: a system call that reads a trapped page fails with `EFAULT`, another thread can read a range the resolver has partly stored with the CPU, and a thread that holds what the resolver needs and touches a trapped range waits forever. They are resolved from a SIGSEGV/SIGBUS handler installed with the first resolver, which passes other faults to the handler installed before it; libkernel's fault delivery also asks the watch first. In addition:
  - a trap is set only on pages known to be read-write. The protection is read from `/proc/self/maps` at the first trap in a registered range and then follows `GuestWriteWatchProtect`, which libkernel's `mprotect` and the guest arena use. A plain `mprotect` of a watched page after that is not seen, and a trap released there gives the page back read-write;
  - registering memory drops its traps without restoring their pages, since every caller registers a range it has just mapped; a range registered again without a new mapping keeps inaccessible pages. Unregistering restores them without running the resolver.
