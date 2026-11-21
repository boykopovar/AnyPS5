# relinker

## add Functional

- An input ELF whose `e_phnum` is `PN_XNUM` (0xffff), which the gABI uses for 65535 or more program headers and whose actual count is in `sh_info` of section header 0, fails the conversion in the [ELF reader](../../../core/relinker/relinker/src/parsing/ElfReader.cpp): reading that count is not implemented, and the output program header table cannot hold that many headers either. No title with that many program headers is known.
