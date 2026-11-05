#version 450
out gl_PerVertex { vec4 gl_Position; };
void main() {
    vec2 position = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(position * 2.0 - 1.0, 0.5, 1.0);
}
