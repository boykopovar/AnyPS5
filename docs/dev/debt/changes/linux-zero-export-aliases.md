# relinker

## change [Guest exports at address 0 on Linux]

- [Guest exports at address 0 on Linux](../../../core/relinker/elfpatcher/src/linux/LinuxGuestModuleWriter.cpp) - the host loader skips a defined symbol whose value is 0 (`check_match` in glibc's `elf/dl-lookup.c`), and the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk) links a shared object's code at address 0, so its first function is exported at 0, as in the SDK's `hello_so` sample. Such a function export is written as a `jmp` to it in the segment the module gains, so a pointer to it taken through the symbol differs from one the module computes itself. A module whose `jmp` would lie 2 GiB or more above address 0 fails to convert. A data or untyped export at address 0 stays unresolvable on Linux, and an import of one is not reported at conversion: the host loader stops on it as an undefined symbol.
