# libSceAgc

## change [sceAgcCbBranch]

- [sceAgcCbBranch](../../../core/libs/prx/libSceAgc/Cb/src/Branch.cpp) (libSceAgc) - the mode is masked to 2 bits, the compare function to 3 bits and the low 3 bits of the compare address are cleared, as [prosper's console measurements](https://github.com/mattias800/prosper/blob/d44c89db977a162af271d986a8d1febbc31e702f/prosper/tests/data/console_oracle/agcbuild.golden.tsv) (captured 2026-10-08) show for the modes 0x11, 1 and 0x22, the compare functions 0x22, 2 and 0x44 and the addresses 0x33333333, 3 and 0. A PS5 (firmware 11.60, payload process; a title process may differ) cleared exactly the low 3 bits for the compare functions 0, 2 and 4 in two runs: offsets 1, 3, 4 and 7 from an aligned address gave that address, offsets 8, 12 and 15 gave it plus 8, and a null address stayed 0. The cache policies, targets and sizes come from the stack and are not measured; they are still checked.
