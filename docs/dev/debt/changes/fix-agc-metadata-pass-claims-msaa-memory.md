# libSceAgcDriver

## change [Multisampled color targets]

- [Multisampled color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) keep samples on the host; guest access to live samples throws. Guarded access retires a target if its samples are not host-only (sampled before any draw, storage write or absorbed fill; rebuilt next use) or if its memory was reused since last use: a watched CPU write over those bytes, a bind over a guarded range of another color target (not its sRGB twin), depth/stencil or HTILE, or a CB fast-clear eliminate or DCC decompress over an overlapping single-sample color target. One-fragment FMASK guards are never claimed. Any such bind or pass counts as reuse, so sampling afterward loses its samples. Unmapping or removing a whole guarded range (color plane or EQAA FMASK) retires the target and its guards; partial unmaps keep guards. Retired owners drop on next lookup; recorded work and views keep their images until released.
