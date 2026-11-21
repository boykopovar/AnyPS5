# libSceNKWebKit

## add Functional

- [Cairo GraphicsContext core](../../../core/libs/prx/libSceNKWebKit/src/GraphicsContext.cpp) (libSceNKWebKit) - implements the immediate-mode `GraphicsContext(PlatformContextCairo*)` path over the cairo context at `PlatformContextCairo` +0: construction and destruction, `save`, `restore`, `translate`, `scale`, `applyDeviceScaleFactor`, `clip`, `fillRect`, `clearRect`, `fillPath`, `strokePath`, the fill/stroke color and thickness setters, antialias, font smoothing and image interpolation setters, shadow state, `hasPlatformContext`, `platformContext` and `getCTM`, plus the `GraphicsContextState` constructors and destructor. `GraphicsContextState` is the 96-byte layout of the 2020 source and `GraphicsContext` is 144 bytes; both are internal to this module. `fillPath` and `strokePath` read the `Path` cairo context at +0, and `fillRect`/`fillPath`/`strokePath` use the cairo source, fill and stroke operations. The shadow offset, blur and color are recorded by `setShadow` and cleared by `clearShadow`, but cairo drawing does not blur or paint them. `setShouldSmoothFonts` and `setImageInterpolationQuality` only record their value, the gradient and pattern pointers are not reference counted because `Gradient` and `Pattern` are unimplemented (no non-null value can be created yet), and the `GraphicsContextImpl` factory constructor still throws. The layout is taken from the 2020 WebKit source, not measured on a console. Closes when: shadows, gradients, smoothing and interpolation are drawn and the layout is checked against a PS5.

## remove `_ZN7WebCore15GraphicsContextD1Ev`

## remove `_ZN7WebCore15GraphicsContextD2Ev`

## remove `_ZN7WebCore15GraphicsContext4saveEv`

## remove `_ZN7WebCore15GraphicsContext7restoreEv`
