# libSceAgcDriver

## add Functional

- [Stencil operation-value attachment fallback](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp): console behavior is unmeasured. Rect lists, mesh/tessellation, face/channel splits, partial masks and non-COPY color ROPs throw. Without native stencil export, a replay-unsafe operation value requires independentBlend, unused color location 7, a single-sample depth/stencil target, one layer, late unordered tests, ALWAYS/full-byte pass replacement and KEEP on failure on both faces. The supported path copies existing stencil into R8_UINT, draws once and copies the result back.
