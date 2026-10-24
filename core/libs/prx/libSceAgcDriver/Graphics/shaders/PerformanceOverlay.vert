#version 450

layout(location = 0) in ivec4 area;
layout(location = 1) in uint color;
layout(push_constant) uniform Parameters {
    vec4 palette[3];
    vec2 extent;
} parameters;
layout(location = 0) flat out uint paletteIndex;

void main() {
    vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
    vec2 pixel = vec2(area.xy) + corner * vec2(area.zw);
    gl_Position = vec4(pixel / parameters.extent * 2.0 - 1.0, 0.0, 1.0);
    paletteIndex = color;
}
