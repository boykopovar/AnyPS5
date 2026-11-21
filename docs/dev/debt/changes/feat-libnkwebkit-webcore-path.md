# libSceNKWebKit

## add Functional

- [WebCore::Path and the path DisplayList items](../../../core/libs/prx/libSceNKWebKit/src/Path.cpp) (libSceNKWebKit) - the 32-byte `Path` layout follows the 11.00 `libSceNKWebKit.sprx` (SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`) `Path::Path()` and `Path::isNull` bodies: the cairo context at +0, a 16-byte `Vector<PathElement>` at +8 and the engaged flag at +0x18. Path keeps both the cairo path and its `PathElement` list, as the cairo build of `PathCairo.cpp` does with inline path data disabled; `PathElement` is the 28-byte points-then-type layout. Copying, assignment, destruction, `moveTo`, `addLineTo`, `addQuadCurveTo`, `addBezierCurveTo`, `closeSubpath`, `clear`, both `addRoundedRect` overloads, `elementCount`, `apply` and `fastBoundingRect` are implemented. `apply` invokes the guest `WTF::Function` wrapper through its `{completeDestructor, deletingDestructor, call}` vtable and falls back to the cairo path when the element list is not engaged. The six path item classes (`FillPath`, `StrokePath`, `DrawPath`, `ClipPath`, `ClipOutToPath`, `DrawFocusRingPath`) are constructed and destroyed with their `Path` member. Their `apply` still throws through `ApplyWithoutGraphicsContext` and `StrokePath::localBounds` reports no bounds, so replaying these items awaits `GraphicsContext` support; `FillPath` and `DrawPath` report `fastBoundingRect` as their local bounds. The `Path` layout is inferred from the module disassembly, not measured on a console. Closes when: the item apply paths are implemented and the layout is checked against a PS5.

## remove `_ZN7WebCore4PathC1ERKS0_`

## remove `_ZN7WebCore4PathC2ERKS0_`

## remove `_ZN7WebCore4PathaSERKS0_`

## remove `_ZN7WebCore4Path6moveToERKNS_10FloatPointE`

## remove `_ZN7WebCore4Path9addLineToERKNS_10FloatPointE`

## remove `_ZN7WebCore4Path14addQuadCurveToERKNS_10FloatPointES3_`

## remove `_ZN7WebCore4Path12closeSubpathEv`

## remove `_ZNK7WebCore4Path16fastBoundingRectEv`
