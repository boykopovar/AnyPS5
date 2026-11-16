# libSceAgcDriver

## add Functional

- [Initial compressed multisample texture reads](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) without a resident owner accept the existing uniform DCC clear subset on pipe-aligned R_X surfaces. A texture descriptor supplies no register clear color, so key 0x20 still throws. Expanded or mixed DCC, compressed payloads, arrays, mips, compressed storage writes and untracked metadata remain unsupported; the native importer retains its range and generation checks. An owner created this way that no draw has rendered to takes DCC control, CMASK and FMASK from its first color-target use by importing again under that layout, after the same store checks; another address, DCC address, alpha placement, alignment, tile mode, element size or XOR, a drawn owner and a later change still throw. Console equivalence is unmeasured.
