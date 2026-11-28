#version 450
#extension GL_EXT_fragment_shader_barycentric : require

layout(location = 0) out vec4 color;

void main() {
#if LINEAR
    vec3 weights = gl_BaryCoordNoPerspEXT;
#else
    vec3 weights = gl_BaryCoordEXT;
#endif
#if FRAG_COORD
    weights = gl_FragCoord.w > 0.0 ? weights : vec3(0.0);
#endif
    color = vec4(weights, 1.0);
}
