#version 450
layout(location = 0) in vec4 offset;
out gl_PerVertex { vec4 gl_Position; };
void main() {
    vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexIndex % 3] + vec2(offset.x, 0.0), 0.5, 1.0);
}
