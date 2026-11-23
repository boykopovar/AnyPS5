# libSceFontGs.native

## change [sceFontGraphicsAgcSurfaceInit]

- [sceFontGraphicsAgcSurfaceInit](../../../core/libs/prx/libSceFontGs.native/Surface.cpp) (libSceFontGs.native) - the surface layout and format acceptance are measured against the FontGsm module bundled with PPSA28050. Physical console and other module versions have not been compared. Reserved format encodings outside the public field ranges and unreadable or unwritable buffers return invalid parameter as host fallbacks.
