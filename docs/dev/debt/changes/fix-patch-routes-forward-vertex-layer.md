# libSceAgcDriver

## add Functional

- [Vertex layer through generated stages](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp) (libSceAgcDriver) - a rect-list draw, or a draw whose emulated barycentrics go through generated tessellation or geometry stages, carries the vertex layer export through those stages as a varying at the first location no vertex output covers. The rect-list evaluation stage writes vertex 0's layer for the whole rectangle, or vertex 2's with `PROVOKING_VTX_LAST`; the barycentric tessellation stages write the provoking vertex's layer for the triangle, and the barycentric geometry stage writes it to each vertex it emits. A vertex shader that writes gl_ViewportIndex, or gl_Layer as a block member, on these routes throws. Which vertex's layer the console uses for a rect list or a triangle whose vertices disagree is not measured.
