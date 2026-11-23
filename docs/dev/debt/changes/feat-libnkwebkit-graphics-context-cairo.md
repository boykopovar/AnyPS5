# libSceNKWebKit

## add Functional

- [Cairo GraphicsContext core](../../../core/libs/prx/libSceNKWebKit/src/GraphicsContext.cpp) (libSceNKWebKit) - the 96-byte state and 144-byte context layouts come from the 2020 source, not a console module. Shadows are stored but not drawn; font smoothing and interpolation only record; only `drawNativeImage`'s per-draw operator is applied, because `setCompositeOperation` and transparency layers are unimplemented; `alpha` multiplies fill, stroke and underline source alpha; gradients and patterns are not reference counted and the impl-factory constructor throws. The immediate-mode path over `PlatformContextCairo` +0 implements save/restore, transforms, clip, rect and path fill/stroke, `strokeRect`, `drawNativeImage`, `drawLineForText`, colors, antialias, shadow state and `getCTM`. Closes when: these gaps are drawn and the layout is checked on a PS5.

## remove `_ZN7WebCore15GraphicsContextD1Ev`

## remove `_ZN7WebCore15GraphicsContextD2Ev`

## remove `_ZN7WebCore15GraphicsContext4saveEv`

## remove `_ZN7WebCore15GraphicsContext7restoreEv`
