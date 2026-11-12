#version 450
#extension GL_GOOGLE_include_directive : require

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"

layout(set = 0, binding = 0) uniform sampler2D source;
layout(push_constant) uniform Parameters {
    ivec2 origin;
    ivec2 size;
    float sharpness;
} parameters;
layout(location = 0) out vec4 color;

AF4 FsrRcasLoadF(ASU2 p) {
    return texelFetch(source, clamp(p, ASU2(0), parameters.size - 1), 0);
}

void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {
}

#define FSR_RCAS_F 1
#include "ffx_fsr1.h"

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy) - parameters.origin;
    if (any(lessThan(pixel, ivec2(0))) || any(greaterThanEqual(pixel, parameters.size))) {
        color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    AU4 con;
    FsrRcasCon(con, parameters.sharpness);
    AF3 sharpened;
    FsrRcasF(sharpened.r, sharpened.g, sharpened.b, AU2(pixel), con);
    color = vec4(clamp(sharpened, 0.0, 1.0), 1.0);
}
