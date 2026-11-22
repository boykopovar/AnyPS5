# relinker

## add Functional

- With an empty GNU hash and no ELF section headers, the [relinker](../../../core/relinker/relinker/src/parsing/ElfReader.cpp) uses symoffset as the dynamic symbol count, which can omit imports at or beyond that index.
