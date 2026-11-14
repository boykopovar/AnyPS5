#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(set = 0, binding = 10) uniform utexture2D images[1];
layout(location = 0) out vec4 outputColor;
void main() {
    outputColor = unpackUnorm4x8(texelFetch(images[0], ivec2(0, 0), 0).r);
}
