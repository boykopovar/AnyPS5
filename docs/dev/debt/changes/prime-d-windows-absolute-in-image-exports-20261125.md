# relinker

## change [Windows executable exports]

- [Windows executable exports](../../../../core/relinker/elfpatcher/src/windows/WindowsExportBuilder.cpp) include non-TLS `SHN_ABS` symbols whose complete ranges lie in an appropriate `PT_LOAD`, as RVAs in the PE export table. Absolute function symbols require executable, file-backed coverage; their full declared range is checked unless `--function-symbol-roots` is enabled, when only the entry byte is needed. Absolute symbols whose ranges cross a normalized PT_LOAD boundary remain omitted even when an original overlapping PT_LOAD covers the full range, because the PE export builder requires one representable load segment; dynamic lookup of external values remains unsupported. Executable export discovery requires `DT_OS_SYMTABSZ`; standard ELF hash tables alone do not retain executable definitions.
