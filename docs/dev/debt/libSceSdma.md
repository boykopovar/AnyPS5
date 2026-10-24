# libSceSdma technical debt

### Unknown function info

- [libSceSdma pending exports](../../../core/libs/prx/libSceSdma/Export.cpp) - 26 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSdma.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

- `_sceSdmaNop` — NID `vPP6vKHY8Tg` — its name and NID occur in the [PS5 3.20 libSceSdma import-stub list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSdma.c). The signature and behavior are unverified; the declaration in [Export.cpp](../../../core/libs/prx/libSceSdma/Export.cpp) uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only.
