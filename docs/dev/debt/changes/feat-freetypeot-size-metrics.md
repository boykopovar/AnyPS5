# libSceFreeTypeOt

## add Unknown function info

- [FT_New_GlyphSlot](../../../core/libs/prx/libSceFreeTypeOt/src/GlyphSlot.cpp) (libSceFreeTypeOt) - console prototype and FreeType version equivalence are unverified. It follows 2.4.12 [ftobjs.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L245-L420) and [ftgloadr.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftgloadr.c) on the LP64 `FT_DriverRec`, the `FT_Driver_ClassRec` fields up to `done_slot`, the 72-byte `FT_Slot_InternalRec` and the 176-byte `FT_GlyphLoaderRec`, calling the driver's `init_slot` and `done_slot` directly. A `slot_object_size` from 0 to below the 0x130-byte `FT_GlyphSlotRec`, which 2.4.12 dereferences or overruns, throws, as do a null driver class, module class or driver memory that 2.4.12 dereferences or calls.
