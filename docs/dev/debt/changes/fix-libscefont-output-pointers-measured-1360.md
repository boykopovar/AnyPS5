# libSceFont

## change [Unwritable output pointers]

- [Unwritable output pointers](../../../core/libs/prx/libSceFont/src/Library.cpp): `sceFontRendererGetOutlineBufferSize`, `sceFontGetPixelResolution`, `sceFontGetLibrary`, `sceFontGetFontGlyphsCount`, `sceFontGraphicsRegionInit`, `sceFontGraphicsRegionInitCircular`, `sceFontGraphicsRegionInitRoundish` and `sceFontGraphicsFillMethodInit` copy their result out with `GuestCopyOut_nid_no_patch`, restoring `errno`. A PS5 Pro payload process (firmware 13.60) measured the first four: they tolerate a null output and return the handle error, writing zero through a writable output before it (`sceFontGetLibrary` answers `INVALID_FONT_HANDLE`, not `INVALID_PARAMETER`, for a null one), and an unmapped output pointer faults the process - refusing with `INVALID_PARAMETER` there is kept as a deliberate host-safety deviation. The region and fill-method forms are unmeasured; a title process was not measured.
