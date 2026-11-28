# libSceVenc

## change Encoder engine
- [Encoder engine](../../../core/libs/prx/libSceVenc/Export.cpp): parameter validation matches 13.60 `libSceVenc.sprx` (`0x3620`, `0x3f30`, `0x5690`, `0x9220`, `0xfb00`), and `sceVencQueryHeader` handles codec 3 (`0`) and unsupported codecs (`0x80C70004`); codecs 1/2 header generation and encoder calls still throw `NotImplemented_nid_no_patch`.

## change Structure layouts
- [Structure layouts](../../../core/libs/prx/libSceVenc/Export.cpp): parameter size headers (+0 `0x60..0x68`; +8 `0x30..0x38`; +0x10/+0x20/+0x58/+0x60 `0x10`; +0x18/+0x38 `0x20`; +0x28 `0x20..0x28`; +0x30 `0x10..0x18`; +0x40 `0x30`; +0x48 `0x28`; +0x50 `0x18`; nested +8 under +0x38/+0x40/+0x48 `0x20`), memory (+0 `0x20`, sub `0x18`/`0x20`/`0x20`) and header (`0x118`/`0x1a0`/`8`) follow 13.60.
