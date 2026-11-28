#version 450

const vec3 corners[3] = vec3[3](vec3(-0.8, -0.8, 1.0), vec3(0.9, -0.6, 2.5), vec3(-0.5, 0.85, 0.5));

void main() {
    const vec3 corner = corners[gl_VertexIndex];
    gl_Position = vec4(corner.xy * corner.z, 0.5 * corner.z, corner.z);
}
