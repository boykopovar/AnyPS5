# libSceFreeTypeOt

## add Functional

- [`FT_Get_Track_Kerning`](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `e2KoMPOAdZg`) has an unmeasured console ABI and behavior. On Windows it forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/src/TrackKerning.cpp) and inherits its guest pointer checks and native fixed-point range. Other hosts resolve the function through the existing libSceFreeType dependency.
