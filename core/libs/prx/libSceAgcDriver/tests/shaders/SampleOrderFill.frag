#version 450
#ifdef STENCIL
#extension GL_ARB_shader_stencil_export : require
#endif

layout(std430, binding = 0) readonly buffer Values {
    uvec2 values[];
};

layout(push_constant) uniform Fill {
    uint width;
    uint count;
} fill;

void main() {
    const uvec2 at = uvec2(gl_FragCoord.xy);
    const uint slot = (at.y * fill.width + at.x) * fill.count + uint(gl_SampleID);
    gl_FragDepth = uintBitsToFloat(values[slot].x);
#ifdef STENCIL
    gl_FragStencilRefARB = int(values[slot].y);
#endif
}
