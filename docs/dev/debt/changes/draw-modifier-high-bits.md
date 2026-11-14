# libSceAgc

## add Unknown function info

- [sceAgcDcbDrawIndexIndirect](../../../core/libs/prx/libSceAgc/DcbDraw/src/DrawIndexed.cpp) (libSceAgc) - modifier bit 1 is not decoded. With it set the console also writes an SGPR location (the base 0x8c plus a field of the modifier) in the high half of the second dword and bit 28 of the third ([measured](https://github.com/mattias800/prosper/blob/d44c89db977a162af271d986a8d1febbc31e702f/prosper/tests/data/console_oracle/agcbuild.golden.tsv#L114-L115)): modifier 2 gives 0x008c0280 and 0x10000280, 0x22222222 gives 0x00940280 and 0x10000280. The two measurements do not fix which modifier bits hold the field, so both high halves stay zero here. The non-indexed `sceAgcDcbDrawIndirect` writes no such fields for the same modifiers ([measured](https://github.com/mattias800/prosper/blob/d44c89db977a162af271d986a8d1febbc31e702f/prosper/tests/data/console_oracle/agcbuild.golden.tsv#L126-L128)).
