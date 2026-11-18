#version 450

#if defined(UINT_SAMPLES)
layout(set = 0, binding = 0) uniform usampler2DMS source;
layout(location = 0) out uvec4 color;
#elif defined(SINT_SAMPLES)
layout(set = 0, binding = 0) uniform isampler2DMS source;
layout(location = 0) out ivec4 color;
#elif defined(PACKED_SAMPLES)
layout(set = 0, binding = 0) uniform usampler2DMS source;
layout(location = 0) out uvec4 color;
#else
layout(set = 0, binding = 0) uniform sampler2DMS source;
layout(location = 0) out vec4 color;
#endif

#if defined(PACKED_SAMPLES)
uint averageField(ivec2 texel, int samples, uint offset, uint mantissaBits) {
    uint mask = (1u << mantissaBits) - 1u;
    uvec2 sum = uvec2(0u);
    bool nan = false;
    bool infinity = false;
    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
        uint raw = bitfieldExtract(texelFetch(source, texel, sampleIndex).x, int(offset), int(mantissaBits + 5u));
        uint exponent = raw >> mantissaBits;
        uint mantissa = raw & mask;
        if (exponent == 31u) {
            nan = nan || mantissa != 0u;
            infinity = infinity || mantissa == 0u;
            continue;
        }
        uint significand = exponent == 0u ? mantissa : mantissa | (1u << mantissaBits);
        uint shift = exponent == 0u ? 0u : exponent - 1u;
        uint carry;
        sum.x = uaddCarry(sum.x, significand << shift, carry);
        sum.y += carry + (shift == 0u ? 0u : significand >> (32u - shift));
    }
    if (nan) return (31u << mantissaBits) | (1u << (mantissaBits - 1u));
    if (infinity) return 31u << mantissaBits;
    uint divide = uint(findLSB(uint(samples)));
    uvec2 units = divide == 0u ? sum : uvec2((sum.x >> divide) | (sum.y << (32u - divide)), sum.y >> divide);
    if (units.y == 0u && units.x <= mask) return units.x;
    int top = units.y != 0u ? 32 + findMSB(units.y) : findMSB(units.x);
    uint drop = uint(top) - mantissaBits;
    uint kept = drop == 0u ? units.x : drop >= 32u ? units.y >> (drop - 32u) : (units.x >> drop) | (units.y << (32u - drop));
    return ((drop + 1u) << mantissaBits) | (kept & mask);
}
#endif

void main() {
#if defined(PACKED_SAMPLES)
    ivec2 texel = ivec2(gl_FragCoord.xy);
    int samples = textureSamples(source);
    color = uvec4(averageField(texel, samples, 0u, 5u) | (averageField(texel, samples, 10u, 6u) << 10u) | (averageField(texel, samples, 21u, 6u) << 21u), 0u, 0u, 0u);
#else
    color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);
#endif
}
