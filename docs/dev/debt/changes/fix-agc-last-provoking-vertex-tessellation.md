# libSceAgcDriver

## add Functional

- [Last provoking vertex without VK_EXT_provoking_vertex](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp) (libSceAgcDriver) - points, lines, fans, mesh and tessellation paths, and draws whose vertex shader writes `Layer` or `ViewportIndex` still throw when the pixel shader shows the provoking vertex. A vertex-path triangle list or strip on a device with tessellation shaders recompiles its pixel shader with emulated barycentrics and draws through the barycentric tessellation stages, which add two shader stages per draw; that cost is not measured.
