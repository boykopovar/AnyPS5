#version 450
layout(location = 0) out vec4 bytes;
void main() {
    uint index = uint(gl_FragCoord.x) + uint(gl_FragCoord.y) * 64u;
    uint word = (index & 1023u) | (((index * 17u + 511u) & 1023u) << 10u)
              | (((index * 73u + 512u) & 1023u) << 20u) | ((index >> 10u) << 30u);
    bytes = unpackUnorm4x8(word);
}
