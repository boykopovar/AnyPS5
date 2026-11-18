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
float unpackField(uint word, uint offset, uint mantissaBits) {
    uint raw = bitfieldExtract(word, int(offset), int(mantissaBits + 5u));
    uint mantissa = raw & ((1u << mantissaBits) - 1u);
    uint exponent = raw >> mantissaBits;
    if (exponent == 31u) return uintBitsToFloat(0x7f800000u | (mantissa << (23u - mantissaBits)));
    if (exponent == 0u) return float(mantissa) * exp2(-14.0 - float(mantissaBits));
    return uintBitsToFloat(((exponent + 112u) << 23u) | (mantissa << (23u - mantissaBits)));
}

uint packField(float value, uint mantissaBits) {
    uint bits = floatBitsToUint(value);
    uint infinity = 31u << mantissaBits;
    uint exponent = (bits >> 23u) & 0xffu;
    uint mantissa = bits & 0x7fffffu;
    uint top = mantissa >> (23u - mantissaBits);
    bool negative = (bits >> 31u) != 0u;
    if (exponent == 0xffu) return negative && mantissa == 0u ? 0u : infinity | (top == 0u && mantissa != 0u ? 1u : top);
    if (negative) return 0u;
    int biased = int(exponent) - 112;
    if (biased >= 31) return infinity - 1u;
    if (biased >= 1) return (uint(biased) << mantissaBits) | top;
    if (biased >= -int(mantissaBits)) return (mantissa | 0x800000u) >> min(uint(24 - int(mantissaBits) - biased), 31u);
    return 0u;
}
#endif

void main() {
#if defined(PACKED_SAMPLES)
    ivec2 texel = ivec2(gl_FragCoord.xy);
    int samples = textureSamples(source);
    vec3 sum = vec3(0.0);
    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
        uint word = texelFetch(source, texel, sampleIndex).x;
        sum += vec3(unpackField(word, 0u, 5u), unpackField(word, 10u, 6u), unpackField(word, 21u, 6u));
    }
    vec3 average = sum / float(samples);
    color = uvec4(packField(average.x, 5u) | (packField(average.y, 6u) << 10u) | (packField(average.z, 6u) << 21u), 0u, 0u, 0u);
#else
    color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);
#endif
}
