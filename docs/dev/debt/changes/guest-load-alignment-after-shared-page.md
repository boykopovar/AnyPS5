# relinker

## change [Moved Linux guest loads]

- [Moved Linux guest loads](../../../core/relinker/elfpatcher/src/linux/LinuxGuestModuleWriter.cpp) - a guest module's `PT_LOAD` whose `p_align` exceeds the page and shares its first page with a load that moves is copied at an offset congruent to its virtual address modulo that alignment instead of failing the conversion, so the copy keeps the declared `p_align` and the output grows by at most the alignment. Alignments of 0x80000000 and above still fail. The synthetic test maps each output the way Linux maps a module, and no live load of such a module on Linux or on a console has been run.
