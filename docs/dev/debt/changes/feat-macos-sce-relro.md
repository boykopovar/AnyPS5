# relinker

## change On Windows output, macOS output, and Linux output with `--lazy-binding`, a `PT_SCE_RELRO` range stays writable

- On Windows output and Linux output with `--lazy-binding`, a `PT_SCE_RELRO` range stays writable after relocation: the [segment filter](../../../core/relinker/elfpatcher/src/general/SegmentFilter.cpp) drops `PT_GNU_RELRO` when lazy binding is on, and the [Windows](../../../core/relinker/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp) entry stub has no equivalent of it at all. Default (eager-binding) Linux output gets `PT_GNU_RELRO`, and [macOS](../../../core/relinker/elfpatcher/src/macos/MacOsImage.cpp) output calls `mprotect` after dyld fixups and import tracing. Matching that on Windows needs a `VirtualProtect` call added to the entry stub after relocations finish (follow-up).
