# relinker

## change [Packed relative relocations]

- [Packed relative relocations](../../../core/relinker/relinker/src/parsing/RelrRelocations.cpp) resolve the table and every 8-byte storage unit through the file-backed `PT_LOAD` segments covering each byte, as glibc's loader reads the mapped array, and the Linux patcher and the Windows load image map a relocation slot through the segments covering each of its bytes, so units spanning load segments convert. Overlapping RELR units and overlaps with RELA relocations are rejected; relocation composition is not implemented. No title with such a table is known.
