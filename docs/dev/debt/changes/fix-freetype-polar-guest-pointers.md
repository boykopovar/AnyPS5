# libSceFreeType

## change [FT_Vector_Length](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp)

- [FT_Vector_Length](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp) (libSceFreeType) - console signature is unverified; the adapter forwards to bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/fttrigon.c). A guest value outside the host `FT_Fixed` range throws on Windows, where the native type is 32 bits; results for in-range values match the native CORDIC. The vector is read with `GuestCopyIn_nid_no_patch`; an unreadable vector throws, a host-safety fallback where FreeType faults

## change [FT_Vector_Rotate](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp)

- [FT_Vector_Rotate](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp) (libSceFreeType) - console signature is unverified; the adapter forwards to bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/fttrigon.c). Angles use their full guest 64-bit range, reduced by whole turns before the native CORDIC; a nonzero whole turn retains the native nonzero-angle path. A vector component outside the host `FT_Fixed` range still throws on Windows, where the native type is 32 bits. The vector is read with `GuestCopyIn_nid_no_patch` and written back with `GuestCopyOut_nid_no_patch`; an unreadable or unwritable vector throws, a host-safety fallback where FreeType faults

## change [FT_Vector_Polarize](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp)

- [FT_Vector_Polarize](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp) (libSceFreeType) - console signature is unverified; the adapter forwards to bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/fttrigon.c). A guest value outside the host `FT_Fixed` range throws on Windows, where the native type is 32 bits; results for in-range values match the native CORDIC. The vector is read with `GuestCopyIn_nid_no_patch` and the length and angle are written with `GuestCopyOut_nid_no_patch` once both are known to be writable; an unreadable vector or an unwritable length or angle throws before either is written, a host-safety fallback where FreeType faults

## change [FT_Vector_From_Polar](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp)

- [FT_Vector_From_Polar](../../../core/libs/prx/libSceFreeType/src/VectorPolar.cpp) (libSceFreeType) - console signature is unverified; the adapter forwards to bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/fttrigon.c). Angles use their full guest 64-bit range with the same whole-turn reduction as `FT_Vector_Rotate`. A length outside the host `FT_Fixed` range still throws on Windows, where the native type is 32 bits. The vector is written with `GuestCopyOut_nid_no_patch`; an unwritable vector throws, a host-safety fallback where FreeType faults
