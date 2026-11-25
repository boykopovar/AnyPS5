# relinker

## change [Windows executable exports]

- [Windows executable exports](../../../core/relinker/elfpatcher/src/windows/WindowsExportBuilder.cpp) include non-TLS `SHN_ABS` symbols whose complete ranges lie in an appropriate `PT_LOAD`, as RVAs in the PE export table. Absolute function symbols require executable mapping and file-backed coverage when `--function-symbol-roots` is enabled. Absolute values outside the loaded image remain omitted because a PE export RVA cannot represent an arbitrary address; dynamic lookup of those external values remains unsupported. Executable export discovery requires `DT_OS_SYMTABSZ`; standard ELF hash tables alone do not retain executable definitions.
