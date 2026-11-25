# libSceIme

## add Functional

- [sceImeFilterText](../../../core/libs/prx/libSceIme/include/ImeFilterText.hpp) (libSceIme) - validates `param` and `src` via `GuestCopyIn_nid_no_patch` and verifies `out_length` and `dst` writability via `GuestWritable_nid_no_patch` before copying out via `GuestCopyOut_nid_no_patch`, preserving host `errno` and buffering filtered code units locally to prevent overlapping `src`/`dst` corruption and partial writes on fault, returning `0x80bc0031` (`kErrorInvalidAddress`) on unmapped, page-straddling, or unwritable guest memory as a host-safety fallback; `InCodepageA` also bounds-checks `c <= 0xffffu`. Console fault behaviour on bad memory is unmeasured.
