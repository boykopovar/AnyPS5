# libSceAgcDriver

## add Functional

- [Vertex layer through generated stages](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp) (libSceAgcDriver) - a rect-list draw, or an emulated-barycentric draw through generated tessellation stages, carries the vertex layer as a varying at the first location no vertex output covers. Rect lists take vertex 0's layer, or vertex 2's with `PROVOKING_VTX_LAST`; the patches take the provoking vertex's. A layer-writing emulated-barycentric draw takes the patches when the device has tessellation, and a fan or a restarted indexed strip, which the patches cannot take, is refused by name, as is the geometry stage, whose input order Vulkan does not fix against the provoking vertex. gl_ViewportIndex, or gl_Layer as a block member, throws. The console's layer for a rect or triangle whose vertices disagree is not measured.
