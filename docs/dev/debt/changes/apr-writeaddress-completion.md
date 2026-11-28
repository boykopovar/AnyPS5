# libkernel

## add Functional

- [Apr::_execute compact AMM PRT command](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - the command-buffer parser decodes the compact native read, compact gather and the compact write-address record (low 12 bits `0x205`), but not a further compact record whose first dword is `0x00001006`, which GTA VI (PPSA38500) appends after an `AmmMapAsPrt` command while backing a partially-resident texture; the parser throws `APR: truncated command buffer` because the 4-byte record is shorter than a command header. The record's field layout and the physical-page allocation it requests are not reverse-engineered, so it is left throwing rather than decoded to avoid an unmeasured guess at PRT page backing. Closes when: the compact AMM PRT command encoding is measured.
