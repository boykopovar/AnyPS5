#version 450

layout(push_constant) uniform Parameters {
    vec4 palette[7];
    vec2 extent;
} parameters;
layout(location = 0) flat in uint paletteIndex;
layout(location = 0) out vec4 color;

void main() {
    color = paletteIndex < 7u ? parameters.palette[paletteIndex] : parameters.palette[0];
}
