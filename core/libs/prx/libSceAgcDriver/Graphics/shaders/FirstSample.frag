#version 450

#if defined(UINT_SAMPLES)
layout(set = 0, binding = 0) uniform usampler2DMS source;
layout(location = 0) out uvec4 color;
#elif defined(SINT_SAMPLES)
layout(set = 0, binding = 0) uniform isampler2DMS source;
layout(location = 0) out ivec4 color;
#else
layout(set = 0, binding = 0) uniform sampler2DMS source;
layout(location = 0) out vec4 color;
#endif

void main() {
    color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);
}
