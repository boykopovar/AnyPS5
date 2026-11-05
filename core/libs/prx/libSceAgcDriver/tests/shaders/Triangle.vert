#version 450
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out vec3 color;
void main() {
    vec2 positions[3] = vec2[](vec2(-0.8, -0.7), vec2(0.8, -0.7), vec2(0.0, 0.8));
    vec3 colors[3] = vec3[](vec3(1.0, 0.15, 0.1), vec3(0.1, 1.0, 0.2), vec3(0.15, 0.3, 1.0));
    gl_Position = vec4(positions[gl_VertexIndex], 0.5, 1.0);
    color = colors[gl_VertexIndex];
}
