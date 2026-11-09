#version 450
#extension GL_GOOGLE_include_directive : require

#include "PqToSdr.glsl"

layout(set = 0, binding = 0) uniform usampler2D source;
layout(set = 0, binding = 1, std430) readonly buffer PqLuminance { float pqNits[]; };
layout(push_constant) uniform Parameters {
    ivec2 origin;
    ivec2 size;
    ivec2 sourceSize;
    int stride;
    uint mode;
    float whiteNits;
} parameters;
layout(location = 0) out vec4 color;

vec4 decode(uint word) {
    if ((parameters.mode & 16u) != 0u) {
        vec3 nits = vec3(pqNits[word & 0x3ffu], pqNits[(word >> 10u) & 0x3ffu], pqNits[(word >> 20u) & 0x3ffu]);
        return vec4(srgbEncode(pqToSdrLinear((parameters.mode & 2u) != 0u ? nits : nits.zyx, parameters.whiteNits)), 1.0);
    }
    uvec4 channels;
    float levels = 255.0;
    if ((parameters.mode & 1u) != 0u) {
        channels = uvec4(word & 0x3ffu, (word >> 10u) & 0x3ffu, (word >> 20u) & 0x3ffu, 0u);
        if ((parameters.mode & 4u) != 0u) levels = 1023.0;
        else channels >>= 2u;
        channels.w = uint(levels);
    } else {
        channels = uvec4(word & 0xffu, (word >> 8u) & 0xffu, (word >> 16u) & 0xffu, word >> 24u);
    }
    vec4 value = vec4(channels) / levels;
    return (parameters.mode & 2u) != 0u ? value : value.zyxw;
}

vec4 texel(ivec2 position) {
    return decode(texelFetch(source, clamp(position, ivec2(0), parameters.sourceSize - 1), 0).x);
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy) - parameters.origin;
    if (any(lessThan(pixel, ivec2(0))) || any(greaterThanEqual(pixel, parameters.size))) {
        color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    if ((parameters.mode & 8u) != 0u) {
        color = texel(pixel * parameters.stride);
        return;
    }
    if (parameters.size == parameters.sourceSize) {
        color = texel(pixel);
        return;
    }
    vec2 position = (vec2(pixel) + 0.5) * vec2(parameters.sourceSize) / vec2(parameters.size) - 0.5;
    vec2 base = floor(position);
    vec2 weight = position - base;
    ivec2 corner = ivec2(base);
    vec4 top = mix(texel(corner), texel(corner + ivec2(1, 0)), weight.x);
    vec4 bottom = mix(texel(corner + ivec2(0, 1)), texel(corner + ivec2(1, 1)), weight.x);
    color = mix(top, bottom, weight.y);
}
