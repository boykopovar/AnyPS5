#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(set = 0, binding = 0) uniform usampler2DMS source;
layout(push_constant) uniform Constants {
    uint samples;
} constants;
layout(location = 0) out uint color;

const uvec3 offsets = uvec3(0u, 10u, 21u);
const uvec3 mantissas = uvec3(5u, 6u, 6u);

uint encode(uint64_t units, uint mantissa) {
    const uint leading = 1u << mantissa;
    if (units < leading) return uint(units);
    const uint high = uint(units >> 32u);
    const int highest = high != 0u ? 32 + findMSB(high) : findMSB(uint(units));
    const uint shift = uint(highest) - mantissa;
    return ((shift + 1u) << mantissa) | (uint(units >> shift) & (leading - 1u));
}

void main() {
    const ivec2 texel = ivec2(gl_FragCoord.xy);
    u64vec3 sum = u64vec3(0);
    bvec3 infinity = bvec3(false);
    bvec3 nan = bvec3(false);
    for (uint sampleIndex = 0u; sampleIndex < constants.samples; ++sampleIndex) {
        const uint word = texelFetch(source, texel, int(sampleIndex)).x;
        for (uint field = 0u; field < 3u; ++field) {
            const uint mantissa = mantissas[field];
            const uint leading = 1u << mantissa;
            const uint code = word >> offsets[field];
            const uint exponent = (code >> mantissa) & 31u;
            const uint fraction = code & (leading - 1u);
            if (exponent == 31u) {
                infinity[field] = infinity[field] || fraction == 0u;
                nan[field] = nan[field] || fraction != 0u;
            } else {
                const uint significand = fraction + (exponent == 0u ? 0u : leading);
                sum[field] += uint64_t(significand) << (max(exponent, 1u) - 1u);
            }
        }
    }
    const uint averageShift = uint(findLSB(constants.samples));
    uint word = 0u;
    for (uint field = 0u; field < 3u; ++field) {
        const uint mantissa = mantissas[field];
        uint code = encode(sum[field] >> averageShift, mantissa);
        if (infinity[field] || nan[field]) {
            code = (31u << mantissa) | (nan[field] ? (1u << (mantissa - 1u)) : 0u);
        }
        word |= code << offsets[field];
    }
    color = word;
}
