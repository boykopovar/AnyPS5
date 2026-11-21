# libSceAgcDriver

## add Functional

- [Stencil ownership handoff](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - Shared-stencil transitions across different extents or formats, multisampled and nonmirrorable planes remain unverified. Matching mirrorable owners transfer stencil publication while depth ownership remains independent. Console behavior and commercial titles have not been measured.
