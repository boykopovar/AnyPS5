# relinker

## change [layout builder]

- [layout builder](../../../core/relinker/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp) - Linux output merges kept `PT_GNU_RELRO` ranges whose page spans overlap, identical ranges included, into one range covering the union of the page spans, and still throws for disjoint ranges: glibc's loader and LLD both track only one `PT_GNU_RELRO` segment per image. No observed title has more than one `PT_SCE_RELRO` range; what the console does with one is not checked. Closes when: a title is found needing more than one range, or glibc's own in-progress multi-`PT_GNU_RELRO` support ships.
