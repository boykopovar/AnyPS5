#version 450
#ifdef DATA_WORDS
layout(set = 0, binding = 0, std430) readonly buffer Source { uint words[]; } source;
#define SOURCE source
#else
layout(set = 0, binding = 0, std430) readonly buffer Source { uint words[]; } source[1];
#define SOURCE source[0]
#endif
#ifdef BORDER_COLOR_SAMPLER
layout(set = 0, binding = 57) uniform sampler borderSampler[1];
#endif
layout(location = 0) out vec4 outputColor;
void main() {
    outputColor = uintBitsToFloat(uvec4(SOURCE.words[0], SOURCE.words[1], SOURCE.words[2], SOURCE.words[3]));
}
