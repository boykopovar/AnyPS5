# libSceFreeTypeOt

## add Functional

- [`ft_validator_error`](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) has an unmeasured console ABI and behavior. NID `CfkNVHnT78w` comes from the PS5 3.20 inventory. The error at offset 20 and jump buffer at offset 24 follow the LP64 [FreeType 2.4.12 validator](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/include/freetype/internal/ftvalid.h#L74-L95). The export stores the error and calls guest `longjmp` with 1, using the existing guest libc jump-buffer format. Inaccessible memory throws as a host-safety fallback. The jump buffer must have been saved by a matching guest call in a live frame on the same thread.
