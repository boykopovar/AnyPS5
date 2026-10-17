#version 450

layout(push_constant) uniform Parameters {
    vec4 palette[3];
    vec2 extent;
} parameters;
layout(location = 0) flat in uint paletteIndex;
layout(location = 0) out vec4 color;

void main() {
    color = paletteIndex == 1u ? parameters.palette[1] : paletteIndex == 2u ? parameters.palette[2] : parameters.palette[0];
}
