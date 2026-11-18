# libSceAgcDriver

## change whose record range overlaps a buffer or storage image written by its own dispatch

- An [image table](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) whose record range overlaps a buffer or storage image written by its own dispatch faults (`table written by its dispatch`) when a selected entry's record lies in the written range. For a table read through a 64-bit pointer with a constant, 4-byte-aligned stride (no indirection, pointer hops or carried offsets), only the records the write overlaps are poisoned, so an entry whose record the write misses is still read from the snapshot. A table read through a buffer, a nested or indirect table, and a pointer table without such a layout still fault when any entry is selected, even if the write misses the selected record. A selected column's range is its record and the bytes of the record before the descriptor.
