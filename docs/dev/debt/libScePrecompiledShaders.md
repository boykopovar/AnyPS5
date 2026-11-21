# libScePrecompiledShaders technical debt

### Unknown function info

- [scePrecompiledShaderEntries](../../../core/libs/prx/libScePrecompiledShaders/Export.cpp) (libScePrecompiledShaders) - on the console this is a table of Sony's precompiled Piglet shaders, which AnyPS5 does not ship. The table holds AnyPS5's own shaders under the console names, as GLSL ES 3.10 source behind an `APS5GLSL` line, a format of AnyPS5's own, and only the two the OpenOrbis piglet sample uses: `texmap/v_2.vert` and `texmap/f_2.frag`. They are written from the interface the sample uses (attribute `a_vertex` with the position in `xy` and the texture coordinate in `zw`; `u_projectionMatrix`, `u_modelViewMatrix`, `u_textureSpaceMatrix`, `u_opacity` and `s_sampler`), with the opacity applied to alpha, as the sample's `GL_SRC_ALPHA` blending expects. What the console's shaders compute is unverified, the console table's other names are absent, and the entry fields at 0x8 and 0x10 are unknown and 0
