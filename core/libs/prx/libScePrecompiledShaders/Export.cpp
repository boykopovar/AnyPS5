#include "prx/libScePrecompiledShaders/include/PrecompiledShaders.hpp"

namespace {

constexpr char TexmapVertex[] = R"(APS5GLSL
#version 310 es
in vec4 a_vertex;
uniform mat4 u_projectionMatrix;
uniform mat4 u_modelViewMatrix;
uniform mat4 u_textureSpaceMatrix;
out vec2 v_texcoord;
void main()
{
    gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(a_vertex.xy, 0.0, 1.0);
    v_texcoord = (u_textureSpaceMatrix * vec4(a_vertex.zw, 0.0, 1.0)).xy;
}
)";

constexpr char TexmapFragment[] = R"(APS5GLSL
#version 310 es
precision mediump float;
uniform sampler2D s_sampler;
uniform float u_opacity;
in vec2 v_texcoord;
out vec4 fragColor;
void main()
{
    vec4 color = texture(s_sampler, v_texcoord);
    fragColor = vec4(color.rgb, color.a * u_opacity);
}
)";

}

extern "C" const PrecompiledShaders::Entry scePrecompiledShaderEntries_nid_postfix[] = {
    {"texmap/v_2.vert", 0, 0, sizeof(TexmapVertex) - 1, TexmapVertex},
    {"texmap/f_2.frag", 0, 0, sizeof(TexmapFragment) - 1, TexmapFragment},
    {nullptr, 0, 0, 0, nullptr},
};
