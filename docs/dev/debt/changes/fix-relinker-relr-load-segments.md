# relinker

## change [Packed relative relocations]

- [Packed relative relocations](../../../core/relinker/relinker/src/parsing/RelrRelocations.cpp) read tables and 8-byte addends across contiguous PT_LOAD segments, including file-backed/zero-fill transitions. Tables must remain file-backed; overlapping RELR units and overlaps with RELA relocations are rejected; relocation composition is not implemented.
