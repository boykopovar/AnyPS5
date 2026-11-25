# libSceFontGs.native

## change [sceFontGraphicsAgcSurfaceInit]

- [sceFontGraphicsAgcSurfaceInit](../../../core/libs/prx/libSceFontGs.native/Surface.cpp) (libSceFontGs.native) - the surface layout and format acceptance are measured against the FontGsm module bundled with PPSA28050. Physical console and other module versions have not been compared. Swap values with bits outside 0x1800 select swap zero. Low bits in the number type produce invalid parameter. Unchecked reserved format encodings and inaccessible buffers use the host invalid-parameter fallback.
