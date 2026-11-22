# libSceIme

## remove [sceImeSetConsecutiveClauseConversionEnabled]

## add Functional

- [sceImeSetConsecutiveClauseConversionEnabled](../../../core/libs/prx/libSceIme/Export.cpp) (libSceIme) - stores its argument in the module state and returns 0, following a payload process on a PS5 (firmware 13.590.001) that returned 0 with errno unchanged before opening a panel for values 0, 1, 2, 0xff, 0x100, 0x101, 0xffffffff and 0xffffffff00000000. The effect on conversion and the result while a panel is open remain unmeasured, the stored flag is not read by any host path, and the argument width is not established. The name is from its NID (`row9sfnYNx4`) in [sce_symbols](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). PPSA24191 calls it with 1 before each `sceImeOpen` and opens the panel only when it returns 0.
