# relinker

## change A guest image with more than one `PT_SCE_RELRO` range

- Multiple distinct `PT_SCE_RELRO` headers still throw on Linux output, including overlapping ranges ([layout builder](../../../core/relinker/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp)). Exact duplicates of the normalized `PT_GNU_RELRO` header share a single output slot. Console behavior is unmeasured and no observed title has multiple RELRO ranges. Closes when: a title is found needing more than one distinct range, or glibc's own multi-`PT_GNU_RELRO` support ships.
