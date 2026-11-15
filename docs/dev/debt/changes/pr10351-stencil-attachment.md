# libSceAgcDriver

## add Functional

- [Stencil operation-value attachment fallback](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp): a replay-unsafe exported operation value without native stencil export uses free color location 7 as R8_UINT, copies existing stencil in, draws once and copies the result back. It requires independentBlend, a single-sample depth/stencil target, one layer, late unordered tests, ALWAYS/full-byte pass replacement and KEEP on failure on both faces. Rect lists, mesh/tessellation, face/channel splits and partial masks throw. Console behavior is unmeasured.
