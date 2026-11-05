# libSceFontGs technical debt

### Unknown function info

- [sceFontGraphicsGnmSurfaceInit](../../../core/libs/prx/libSceFontGs/Export.cpp) (libSceFontGs) - still throws through `NotImplemented_nid_no_patch`. The PS4 10.71 body (`libSceFontGs.sprx` offset 0x41e0) returns `INVALID_PARAMETER` for a null surface, zeroes the 0x100-byte surface when byte 0x0d of the config is 0x44 (otherwise only the word at 0x0e), and returns 0x8046005c unless that byte with bit 2 set is 0x44, dword 0x24 is 1 and dwords 0x18 and 0x1c are 0. It then reads config fields 0x00 to 0x54 into an internal helper; the surface layout it writes is not mapped yet.
- [sceFontSelectGraphicsGnm](../../../core/libs/prx/libSceFontGs/Export.cpp) (libSceFontGs) - throws for index 0 and returns `nullptr` for other indices. The PS4 10.71 body (offset 0x4090) returns a static backend table for index 0; its entries are not mapped, and the host `libSceFont` graphics device does not consume it.

### Not console-verified

- [sceFontGraphicsGnmDrawupFillTextureImage, sceFontGraphicsGnmDrawupFillTexturePattern, sceFontGraphicsSequenceGetGnmCommandsForChain and sceFontGraphicsSequenceGetGnmCommandsForSubmit](../../../core/libs/prx/libSceFontGs/Export.cpp) (libSceFontGs) - transliterated from the PS4 10.71 `libSceFontGs.sprx` bodies (offsets 0x40a0, 0x4150, 0x4580 and 0x4640; the names hash to `UsBLoyXsQZs`, `uksHKVEsxu4`, `NtbMKwRg6qg` and `DI4LrTUrD04`). They match the `libSceFontGs.native` AGC bridges except for the surface tag 0x48 and that the two sequence getters pass every error to `sceFontGraphicsReportError`. The sequence getters were matched by single PS4 runs on prepared sequences without a GPU submission; the drawup bridges and the PS5 exports with these NIDs were not checked on a console.
