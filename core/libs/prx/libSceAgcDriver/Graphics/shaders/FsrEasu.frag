#version 450
#extension GL_GOOGLE_include_directive : require

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"

layout(set = 0, binding = 0) uniform sampler2D source;
layout(push_constant) uniform Parameters {
    vec2 sourceSize;
    vec2 outputSize;
} parameters;
layout(location = 0) out vec4 color;

AF4 FsrEasuRF(AF2 p) {
    return textureGather(source, p, 0);
}

AF4 FsrEasuGF(AF2 p) {
    return textureGather(source, p, 1);
}

AF4 FsrEasuBF(AF2 p) {
    return textureGather(source, p, 2);
}

#define FSR_EASU_F 1
#include "ffx_fsr1.h"

void main() {
    AU4 con0, con1, con2, con3;
    FsrEasuCon(con0, con1, con2, con3, parameters.sourceSize.x, parameters.sourceSize.y, parameters.sourceSize.x, parameters.sourceSize.y, parameters.outputSize.x, parameters.outputSize.y);
    AF3 upscaled;
    FsrEasuF(upscaled, AU2(gl_FragCoord.xy), con0, con1, con2, con3);
    color = vec4(upscaled, 1.0);
}
