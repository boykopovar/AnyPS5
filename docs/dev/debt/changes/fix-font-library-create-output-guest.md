# libSceFont

## add Unknown function info

- [Library creation guest output](../../../core/libs/prx/libSceFont/src/Library.cpp): `sceFontCreateLibraryWithEdition` and `sceFontCreateLibrary` clear their output through checked guest writes, reject inaccessible output before allocating and unwind initialized driver resources when the final publication write fails. The invalid-output error is a host-safety fallback, not a console-measured rule.
