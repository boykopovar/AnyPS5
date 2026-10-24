# libSceFreeTypeOptOl technical debt

### Unknown function info

- [FT_Get_PFR_Advance, FT_Get_PFR_Kerning and FT_Get_PFR_Metrics](../../../core/libs/prx/libSceFreeTypeOptOl/Export.cpp) use the existing LP64 guest FreeType face adapter and the bundled [FreeType 2.13.3 PFR API](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftpfr.c), with host operations in [Pfr.cpp](../../../core/libs/prx/libSceFreeType/src/Pfr.cpp). The console's prototypes, FreeType version, configuration and errors are unmeasured; the firmware 3.20 inventory establishes the names, NIDs and module association, not the module/library versions. Signed positions, scales and vector components use 64-bit guest storage even on Windows, where host FreeType uses 32-bit long. Invalid PFR advance indices write zero as the bundled PFR service does, while non-PFR advance failures leave the output untouched; non-PFR metrics write fallback values before returning the format error. Console equivalence of these rules remains unverified.

### Functional

- [PFR faces](../../../core/libs/prx/libSceFreeTypeOptOl/Export.cpp) share the existing libSceFreeType ownership and lifetime rules. The optional module forwards to native helpers in that library rather than constructing another FreeType runtime. Private face records, stale or arbitrary pointers, caller-replaced internal records and simultaneous operations on the same face are not made safe by these adapters; callers must obey FreeType's face lifetime and threading requirements. The synthetic PFR regression exercises metrics, signed advances, metric-space kerning and an empty outline; it does not test bitmap-only PFR faces or every format feature.
