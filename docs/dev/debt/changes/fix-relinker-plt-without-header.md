# relinker

## add Functional

- In images with a `PT_SCE_DYNLIBDATA` segment, the [code extent](../../../core/relinker/relinker/src/analysis/SceCodeExtent.cpp) finds a PLT of two forms only: the lazy-binding one, by its first entry (a `push` of the second `DT_PLTGOT` slot and a `jmp` through the third), and one without that entry, as in CUSA24767 01.71, whose 16-byte entries are each a `jmp` through their own slot, in relocation order from the fourth slot, followed by ten `int3` bytes. A PLT whose entries have another size, padding or slot order fails with `PLT not found before or after the unwind functions`. Which linker versions write the second form is not established.
