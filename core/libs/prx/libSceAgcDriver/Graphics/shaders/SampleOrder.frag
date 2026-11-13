#version 450
#extension GL_EXT_samplerless_texture_functions : require
#ifdef STENCIL_EXPORT
#extension GL_ARB_shader_stencil_export : require
#endif

layout(binding = 0) uniform texture2DMS depthSource;
layout(binding = 1) uniform utexture2DMS stencilSource;

layout(push_constant) uniform Order {
    uvec4 map;
    uint bit;
} order;

void main() {
    const ivec2 at = ivec2(gl_FragCoord.xy);
    const bool right = (at.x & 1) != 0;
    const uint top = right ? order.map.y : order.map.x;
    const uint bottom = right ? order.map.w : order.map.z;
    const uint map = (at.y & 1) != 0 ? bottom : top;
    if (map == 0x76543210u) discard;
    const int source = int((map >> (uint(gl_SampleID) * 4u)) & 7u);
#ifdef STENCIL_BITS
    if (((texelFetch(stencilSource, at, source).r >> order.bit) & 1u) == 0u) discard;
#else
    gl_FragDepth = texelFetch(depthSource, at, source).r;
#ifdef STENCIL_EXPORT
    gl_FragStencilRefARB = int(texelFetch(stencilSource, at, source).r);
#endif
#endif
}
