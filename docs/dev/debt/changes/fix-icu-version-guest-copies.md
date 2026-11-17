# libSceIcu

## add Functional

- [Version helper buffers](../../../core/libs/prx/libSceIcu/Version.cpp) (libSceIcu) - `u_versionFromString`, `u_versionFromUString`, `u_versionToString`, `u_getUnicodeVersion` and `u_getVersion` read and write their buffers through checked copies. An unreadable string or version is handled as a null one; an unwritable output is left unwritten. `u_versionFromString` reads at most 256 bytes of its string. ICU dereferences these buffers, so this is a host-safety fallback.
