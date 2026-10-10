#version 450
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) flat out vec4 color;
void main() {
    vec2 positions[6] = vec2[](
        vec2(-0.75, 0.75), vec2(-0.75, -0.75), vec2(0.75, 0.75),
        vec2(0.75, -0.75), vec2(0.75, 0.75), vec2(-0.75, -0.75));
    vec4 colors[6] = vec4[](
        vec4(1, 0, 0, 1), vec4(0, 1, 0, 1), vec4(0, 0, 1, 1),
        vec4(1, 1, 0, 1), vec4(0, 1, 1, 1), vec4(1, 0, 1, 1));
    gl_Position = vec4(positions[gl_VertexIndex], 0.5, 1.0);
    color = colors[gl_VertexIndex];
}
