# libSceNKWebKit

## add Functional

- [Cairo GraphicsContext core](../../../core/libs/prx/libSceNKWebKit/src/GraphicsContext.cpp) (libSceNKWebKit) - the 96-byte state and 144-byte context layouts come from the 2020 source, not a console module. Shadows are stored but not drawn; font smoothing and interpolation only record; `drawNativeImage` rejects non-default orientation, blend and interpolation; gradients and patterns are not reference counted (unimplemented) and the impl-factory constructor throws. The immediate-mode path over `PlatformContextCairo` +0 implements save/restore, transforms, clip, rect and path fill/stroke, `strokeRect`, `drawNativeImage`, colors, antialias, shadow state and `getCTM`. Closes when: these gaps are drawn and the layout is checked on a PS5.

## remove `_ZN7WebCore15GraphicsContextD1Ev`

## remove `_ZN7WebCore15GraphicsContextD2Ev`

## remove `_ZN7WebCore15GraphicsContext4saveEv`

## remove `_ZN7WebCore15GraphicsContext7restoreEv`
