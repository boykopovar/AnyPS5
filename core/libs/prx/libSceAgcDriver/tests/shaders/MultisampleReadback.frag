#version 450

layout(push_constant) uniform Push { uint layer; uint width; uint height; uint samples; uint elementBytes; } params;
#if !READ_DEPTH
layout(location = 0) out uvec4 color;
#endif

void main() {
    uint x = uint(gl_FragCoord.x);
    uint y = uint(gl_FragCoord.y);
#if READ_DEPTH
    uint element = (x + y * params.width + params.layer * params.width * params.height) * params.samples + uint(gl_SampleID);
    gl_FragDepth = 0.125 + float(element + 1u) / 1048576.0;
#else
    uint value = x + y * params.width + params.layer * params.width * params.height + uint(gl_SampleID) * 31u;
    color = uvec4(0x91352700u ^ value, 0x45219b00u ^ (value * 3u), 0xc7391d00u ^ (value * 5u), 0x6b43e100u ^ (value * 7u));
    if (params.elementBytes == 1u) color.x &= 255u;
    else if (params.elementBytes == 2u) color.x &= 65535u;
#endif
}
