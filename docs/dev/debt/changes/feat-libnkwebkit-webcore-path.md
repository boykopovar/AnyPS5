# libSceNKWebKit

## add Functional

- [WebCore::Path and the path DisplayList items](../../../core/libs/prx/libSceNKWebKit/src/Path.cpp) (libSceNKWebKit) - the 32-byte `Path` layout is inferred from the 11.00 `libSceNKWebKit.sprx` (SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`) `Path::Path` and `Path::isNull` bodies, not measured on a console. The six path items construct and destroy their `Path`; `apply` throws through `ApplyWithoutGraphicsContext`, `StrokePath::localBounds` reports no bounds and `FillPath`/`DrawPath` report `fastBoundingRect`. Path implements copy/assignment/destruction, `moveTo`, `addLineTo`, `addQuadCurveTo`, `addBezierCurveTo`, `closeSubpath`, `clear`, both `addRoundedRect` overloads, `elementCount`, `apply` and `fastBoundingRect`; `apply` calls the guest WTF::Function wrapper and falls back to the cairo path when the element list is disengaged. Closes when: the item apply paths are implemented and the layout is checked against a PS5.

## remove `_ZN7WebCore4PathC1ERKS0_`

## remove `_ZN7WebCore4PathC2ERKS0_`

## remove `_ZN7WebCore4PathaSERKS0_`

## remove `_ZN7WebCore4Path6moveToERKNS_10FloatPointE`

## remove `_ZN7WebCore4Path9addLineToERKNS_10FloatPointE`

## remove `_ZN7WebCore4Path14addQuadCurveToERKNS_10FloatPointES3_`

## remove `_ZN7WebCore4Path12closeSubpathEv`

## remove `_ZNK7WebCore4Path16fastBoundingRectEv`
