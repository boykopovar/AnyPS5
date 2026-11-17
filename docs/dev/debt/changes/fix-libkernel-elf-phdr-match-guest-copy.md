# libkernel

## change [__elf_phdr_match_addr]

- [__elf_phdr_match_addr](../../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) takes the `ModuleInfoEx` record passed by LEGO Voyagers' bundled libc after `sceKernelGetModuleInfoFromAddr` ([#8675](https://github.com/boykopovar/AnyPS5/issues/8675)), including its zero `st_size`. The executable-segment check and requirement for eight bytes remaining retain #8676's behaviour; exact boundary, malformed-record and invalid-pointer behaviour are not console-measured. A null or unreadable record throws; the four segment records and their count are read with one checked copy. The earlier `dl_phdr_info` signature was inferred from FreeBSD, not observed in a title or on a console.
