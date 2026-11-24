# relinker

## add Functional

- [Cache flushes](../../../core/relinker/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp) (relinker) - every conversion target (`--to-intel`, `--to-rosetta`, `--to-avx`) replaces the memory forms of CLFLUSH, CLFLUSHOPT and CLWB with NOPs of the same length, because on the host the guest's cache maintenance has no effect the runtime needs and a flush that touches a page under a label read trap counts as a read and stores the whole image. A relink without a conversion, as on AMD hosts, keeps these instructions. A title that relies on a flush for timing or on its fault is not known. Closes when: a conversion-independent pass replaces them on every host, or a read trap ignores cache flushes itself.
