# shader

## add Functional

- [Native per-vertex inputs of rect-list draws](../../../core/shader/recompiler/SpirvBackend/src/RectListShaders.cpp) (shader recompiler) - with VK_KHR_fragment_shader_barycentric, a rect-list pixel input read per vertex (`v_interp_mov` of P10, P20 or P0, pass-through inputs included) gets the corners of the two generated triangles: the fourth corner is the linear extrapolation of the other three, in the tessellator's vertex order. Affine combinations with the shader's barycentrics land on the rectangle's plane (`agc_driver_pixel_barycentric_emulation`), but a shader that uses one vertex's value on its own, such as a pass-through input read only at P0, can see another corner on one triangle; what the console gives there is not measured. Emulated barycentrics (`APS5_NO_FRAGMENT_SHADER_BARYCENTRIC`) pass the rectangle's three vertices instead. Closes when: native rect-list draws pass the three original vertices, or the console's values are measured.
