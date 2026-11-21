# libSceFreeType

## change src/GuestObjects.cpp)

- [libSceFreeType](../../../core/libs/prx/libSceFreeType/src/GuestObjects.cpp) (libSceFreeType) - private fields (`driver`, `memory`, `stream`, `sizes_list`, `extensions`) and `subglyphs` stay null. Face, size and glyph-slot records are copies. Only edits to outline points and flags are written back before `FT_Render_Glyph`; replacing arrays or exceeding the host `FT_Pos` range throws. `FT_New_Face` reads the whole file into memory and throws for files of 2 GiB or more on Windows.
