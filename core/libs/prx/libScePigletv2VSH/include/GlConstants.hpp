#ifndef CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_GLCONSTANTS_HPP
#define CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_GLCONSTANTS_HPP

#include <cstdint>

namespace Piglet {

using GLenum = std::uint32_t;
using GLuint = std::uint32_t;
using GLint = std::int32_t;
using GLsizei = std::int32_t;
using GLbitfield = std::uint32_t;
using GLboolean = std::uint8_t;
using GLfloat = float;
using GLclampf = float;
using EGLint = std::int32_t;
using EGLBoolean = std::uint32_t;
using EGLenum = std::uint32_t;

inline constexpr GLenum GL_NO_ERROR = 0;
inline constexpr GLenum GL_INVALID_ENUM = 0x0500;
inline constexpr GLenum GL_INVALID_VALUE = 0x0501;
inline constexpr GLenum GL_INVALID_OPERATION = 0x0502;
inline constexpr GLenum GL_POINTS = 0x0000;
inline constexpr GLenum GL_LINES = 0x0001;
inline constexpr GLenum GL_LINE_LOOP = 0x0002;
inline constexpr GLenum GL_LINE_STRIP = 0x0003;
inline constexpr GLenum GL_TRIANGLES = 0x0004;
inline constexpr GLenum GL_TRIANGLE_STRIP = 0x0005;
inline constexpr GLenum GL_TRIANGLE_FAN = 0x0006;
inline constexpr GLenum GL_ZERO = 0;
inline constexpr GLenum GL_ONE = 1;
inline constexpr GLenum GL_SRC_COLOR = 0x0300;
inline constexpr GLenum GL_ONE_MINUS_SRC_COLOR = 0x0301;
inline constexpr GLenum GL_SRC_ALPHA = 0x0302;
inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
inline constexpr GLenum GL_DST_ALPHA = 0x0304;
inline constexpr GLenum GL_ONE_MINUS_DST_ALPHA = 0x0305;
inline constexpr GLenum GL_DST_COLOR = 0x0306;
inline constexpr GLenum GL_ONE_MINUS_DST_COLOR = 0x0307;
inline constexpr GLenum GL_SRC_ALPHA_SATURATE = 0x0308;
inline constexpr GLenum GL_CULL_FACE = 0x0B44;
inline constexpr GLenum GL_DEPTH_TEST = 0x0B71;
inline constexpr GLenum GL_STENCIL_TEST = 0x0B90;
inline constexpr GLenum GL_DITHER = 0x0BD0;
inline constexpr GLenum GL_BLEND = 0x0BE2;
inline constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
inline constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
inline constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
inline constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
inline constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
inline constexpr GLenum GL_FLOAT = 0x1406;
inline constexpr GLenum GL_ALPHA = 0x1906;
inline constexpr GLenum GL_RGB = 0x1907;
inline constexpr GLenum GL_RGBA = 0x1908;
inline constexpr GLenum GL_LUMINANCE = 0x1909;
inline constexpr GLenum GL_LUMINANCE_ALPHA = 0x190A;
inline constexpr GLenum GL_VENDOR = 0x1F00;
inline constexpr GLenum GL_RENDERER = 0x1F01;
inline constexpr GLenum GL_VERSION = 0x1F02;
inline constexpr GLenum GL_EXTENSIONS = 0x1F03;
inline constexpr GLenum GL_NEAREST = 0x2600;
inline constexpr GLenum GL_LINEAR = 0x2601;
inline constexpr GLenum GL_NEAREST_MIPMAP_NEAREST = 0x2700;
inline constexpr GLenum GL_LINEAR_MIPMAP_NEAREST = 0x2701;
inline constexpr GLenum GL_NEAREST_MIPMAP_LINEAR = 0x2702;
inline constexpr GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;
inline constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
inline constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
inline constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
inline constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
inline constexpr GLenum GL_REPEAT = 0x2901;
inline constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
inline constexpr GLenum GL_MIRRORED_REPEAT = 0x8370;
inline constexpr GLenum GL_TEXTURE0 = 0x84C0;
inline constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x00000100;
inline constexpr GLbitfield GL_STENCIL_BUFFER_BIT = 0x00000400;
inline constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;
inline constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
inline constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
inline constexpr GLenum GL_DELETE_STATUS = 0x8B80;
inline constexpr GLenum GL_LINK_STATUS = 0x8B82;
inline constexpr GLenum GL_VALIDATE_STATUS = 0x8B83;
inline constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum GL_ATTACHED_SHADERS = 0x8B85;
inline constexpr GLenum GL_ACTIVE_UNIFORMS = 0x8B86;
inline constexpr GLenum GL_ACTIVE_UNIFORM_MAX_LENGTH = 0x8B87;
inline constexpr GLenum GL_SHADING_LANGUAGE_VERSION = 0x8B8C;
inline constexpr GLenum GL_ACTIVE_ATTRIBUTES = 0x8B89;
inline constexpr GLenum GL_ACTIVE_ATTRIBUTE_MAX_LENGTH = 0x8B8A;
inline constexpr GLenum GL_FLOAT_MAT4 = 0x8B5C;
inline constexpr GLenum GL_SAMPLER_2D = 0x8B5E;

inline constexpr EGLint EGL_SUCCESS = 0x3000;
inline constexpr EGLint EGL_NOT_INITIALIZED = 0x3001;
inline constexpr EGLint EGL_BAD_ACCESS = 0x3002;
inline constexpr EGLint EGL_BAD_ALLOC = 0x3003;
inline constexpr EGLint EGL_BAD_ATTRIBUTE = 0x3004;
inline constexpr EGLint EGL_BAD_CONFIG = 0x3005;
inline constexpr EGLint EGL_BAD_CONTEXT = 0x3006;
inline constexpr EGLint EGL_BAD_DISPLAY = 0x3008;
inline constexpr EGLint EGL_BAD_MATCH = 0x3009;
inline constexpr EGLint EGL_BAD_NATIVE_WINDOW = 0x300B;
inline constexpr EGLint EGL_BAD_PARAMETER = 0x300C;
inline constexpr EGLint EGL_BAD_SURFACE = 0x300D;
inline constexpr EGLint EGL_ALPHA_SIZE = 0x3021;
inline constexpr EGLint EGL_BLUE_SIZE = 0x3022;
inline constexpr EGLint EGL_GREEN_SIZE = 0x3023;
inline constexpr EGLint EGL_RED_SIZE = 0x3024;
inline constexpr EGLint EGL_DEPTH_SIZE = 0x3025;
inline constexpr EGLint EGL_STENCIL_SIZE = 0x3026;
inline constexpr EGLint EGL_SAMPLES = 0x3031;
inline constexpr EGLint EGL_SAMPLE_BUFFERS = 0x3032;
inline constexpr EGLint EGL_SURFACE_TYPE = 0x3033;
inline constexpr EGLint EGL_NONE = 0x3038;
inline constexpr EGLint EGL_RENDERABLE_TYPE = 0x3040;
inline constexpr EGLint EGL_BACK_BUFFER = 0x3084;
inline constexpr EGLint EGL_RENDER_BUFFER = 0x3086;
inline constexpr EGLint EGL_CONTEXT_CLIENT_VERSION = 0x3098;
inline constexpr EGLint EGL_WINDOW_BIT = 0x0004;
inline constexpr EGLint EGL_OPENGL_ES2_BIT = 0x0004;
inline constexpr EGLenum EGL_OPENGL_ES_API = 0x30A0;

}

#endif
