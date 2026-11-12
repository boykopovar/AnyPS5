#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GLSlim state check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static void Error(GLenum expected) {
    Require(glGetError_nid_postfix() == expected);
    Require(glGetError_nid_postfix() == 0);
}

static GLint Integer(GLenum name) {
    GLint value = -99;
    glGetIntegerv_nid_postfix(name, &value);
    return value;
}

static void Defaults() {
    Require(Integer(0x80c9) == 1);
    Require(Integer(0x80c8) == 0);
    Require(Integer(0x80cb) == 1);
    Require(Integer(0x80ca) == 0);
    Require(Integer(0x0b74) == 0x0201);
    Require(Integer(0x0d05) == 4);
    Require(Integer(0x0cf5) == 4);
    Require(Integer(0x0b98) == -1);
    Require(glIsEnabled_nid_postfix(0x0bd0));
    Require(!glIsEnabled_nid_postfix(0x0be2));
    std::array<GLint, 4> box{};
    glGetIntegerv_nid_postfix(0x0ba2, box.data());
    Require((box == std::array<GLint, 4>{0, 0, 4, 3}));
    std::array<GLfloat, 4> attr{};
    glGetVertexAttribfv_nid_postfix(3, 0x8626, attr.data());
    Require((attr == std::array<GLfloat, 4>{0, 0, 0, 1}));
    glVertexAttrib2f_nid_postfix(3, 2, -3);
    glGetVertexAttribfv_nid_postfix(3, 0x8626, attr.data());
    Require((attr == std::array<GLfloat, 4>{2, -3, 0, 1}));
    glVertexAttrib1f_nid_postfix(8, 1);
    Error(0x0501);
    glEnable_nid_postfix(0xffffffff);
    glDepthFunc_nid_postfix(0);
    Error(0x0500);
    glScissor_nid_postfix(0, 0, -1, 1);
    Error(0x0501);
    glBlendFuncSeparate_nid_postfix(1, 0x0308, 1, 0);
    Error(0x0500);
    glBlendFuncSeparate_nid_postfix(0x8001, 0x8003, 1, 0);
    Error(0x0502);
    Require(Integer(0x80c9) == 1);
    glClearColor_nid_postfix(-1, 2, 0.5f, 1);
    glGetFloatv_nid_postfix(0x0c22, attr.data());
    Require((attr == std::array<GLfloat, 4>{0, 1, 0.5f, 1}));
    glGetIntegerv_nid_postfix(0x0c22, box.data());
    Require(box[0] == 0 && box[1] == std::numeric_limits<GLint>::max());
    GLint untouched = 55;
    glGetIntegerv_nid_postfix(0xffffffff, &untouched);
    Require(untouched == 55);
    Error(0x0500);
    glGetIntegerv_nid_postfix(0x80c9, nullptr);
    glGetIntegerv_nid_postfix(0x0b74, nullptr);
    glGetIntegerv_nid_postfix(0xffffffff, nullptr);
    Error(0);
}

static void Buffers() {
    GLuint names[2]{};
    glGenBuffers_nid_postfix(2, names);
    Require(names[0] && names[1] && names[0] != names[1]);
    Require(!glIsBuffer_nid_postfix(names[0]));
    glBindBuffer_nid_postfix(0x8892, names[0]);
    Require(glIsBuffer_nid_postfix(names[0]));
    const std::array<unsigned char, 5> initial{1, 2, 3, 4, 5};
    glBufferData_nid_postfix(0x8892, initial.size(), initial.data(), 0x88e8);
    const unsigned char patch[2]{9, 8};
    glBufferSubData_nid_postfix(0x8892, 2, 2, patch);
    Error(0);
    GLint size = 0;
    glGetBufferParameteriv_nid_postfix(0x8892, 0x8764, &size);
    Require(size == 5);
    glBufferSubData_nid_postfix(0x8892, std::numeric_limits<GLintptr>::max(), 2, patch);
    Error(0x0501);
    glBufferData_nid_postfix(0x8892, 8, nullptr, 0);
    Error(0x0500);
    glGetBufferParameteriv_nid_postfix(0x8892, 0x8764, &size);
    Require(size == 5);
    glVertexAttribPointer_nid_postfix(1, 2, 0x1406, 0, 8, nullptr);
    glDeleteBuffers_nid_postfix(2, names);
    Require(!glIsBuffer_nid_postfix(names[0]));
    Require(Integer(0x8894) == 0);
    glGetVertexAttribiv_nid_postfix(1, 0x889f, &size);
    Require(size == 0);
    glBufferData_nid_postfix(0x8892, 4, nullptr, 0x88e4);
    Error(0x0502);
}

static void Pixels() {
    glClearColor_nid_postfix(1, 0, 0, 1);
    glClearDepthf_nid_postfix(0.5f);
    glClearStencil_nid_postfix(0xab);
    GLfloat clearDepth = 0;
    glGetFloatv_nid_postfix(0x0b73, &clearDepth);
    Require(clearDepth == 0.5f);
    Require(Integer(0x0b91) == 0xab);
    glClear_nid_postfix(0x4500);
    std::array<unsigned char, 48> data{};
    glReadPixels_nid_postfix(0, 0, 4, 3, 0x1908, 0x1401, data.data());
    for (unsigned i = 0; i < 12; ++i) Require(data[i * 4] == 255 && data[i * 4 + 1] == 0 && data[i * 4 + 3] == 255);
    Error(0);
    std::array<unsigned char, 12> edge{};
    glReadPixels_nid_postfix(-2, 0, 3, 1, 0x1908, 0x1401, edge.data());
    Require((edge == std::array<unsigned char, 12>{0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 255}));
    std::array<unsigned char, 4> outside{7, 7, 7, 7};
    glReadPixels_nid_postfix(-536870912, 0, 1, 1, 0x1908, 0x1401, outside.data());
    Require((outside == std::array<unsigned char, 4>{7, 7, 7, 7}));
    Error(0);
    glEnable_nid_postfix(0x0c11);
    glScissor_nid_postfix(1, 1, 2, 1);
    glColorMask_nid_postfix(0, 1, 1, 0);
    glStencilMask_nid_postfix(0x0f);
    glClearColor_nid_postfix(0, 1, 0, 0);
    glClearStencil_nid_postfix(0x05);
    glClear_nid_postfix(0x4400);
    glReadPixels_nid_postfix(0, 0, 4, 3, 0x1908, 0x1401, data.data());
    Require(data[5 * 4] == 255 && data[5 * 4 + 1] == 255 && data[5 * 4 + 3] == 255);
    Require(data[1] == 0);
    Error(0);
    glDisable_nid_postfix(0x0c11);
    glColorMask_nid_postfix(1, 1, 1, 1);
    glStencilMask_nid_postfix(0xffffffff);

    GLuint texture, framebuffer;
    glGenTextures_nid_postfix(1, &texture);
    Require(!glIsTexture_nid_postfix(texture));
    glBindTexture_nid_postfix(0x0de1, texture);
    Require(glIsTexture_nid_postfix(texture));
    GLint parameter;
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2702);
    glCopyTexImage2D_nid_postfix(0x0de1, 0, 0x1908, 0, 0, 4, 3, 0);
    glGenFramebuffers_nid_postfix(1, &framebuffer);
    Require(!glIsFramebuffer_nid_postfix(framebuffer));
    glBindFramebuffer_nid_postfix(0x8d40, framebuffer);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd7);
    glClear_nid_postfix(0x4000);
    Error(0x0506);
    glFramebufferTexture2D_nid_postfix(0x8d40, 0x8ce0, 0x0de1, texture, 0);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd5);
    std::array<unsigned char, 48> copied{};
    glReadPixels_nid_postfix(0, 0, 4, 3, 0x1908, 0x1401, copied.data());
    Require(copied == data);
    const unsigned char patch[]{12, 34, 56, 78};
    glTexSubImage2D_nid_postfix(0x0de1, 0, 2, 1, 1, 1, 0x1908, 0x1401, patch);
    std::array<unsigned char, 4> pixel{};
    glReadPixels_nid_postfix(2, 1, 1, 1, 0x1908, 0x1401, pixel.data());
    Require((pixel == std::array<unsigned char, 4>{12, 34, 56, 78}));
    glTexSubImage2D_nid_postfix(0x0de1, 0, 4, 0, 1, 1, 0x1908, 0x1401, patch);
    Error(0x0501);
    glBindTexture_nid_postfix(0x8513, texture);
    Error(0x0502);

    GLuint renderbuffer;
    glGenRenderbuffers_nid_postfix(1, &renderbuffer);
    glBindRenderbuffer_nid_postfix(0x8d41, renderbuffer);
    glRenderbufferStorage_nid_postfix(0x8d41, 0x81a5, 2, 2);
    glFramebufferRenderbuffer_nid_postfix(0x8d40, 0x8d00, 0x8d41, renderbuffer);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd9);
    glRenderbufferStorage_nid_postfix(0x8d41, 0x81a5, 4, 3);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd5);
    glDeleteRenderbuffers_nid_postfix(1, &renderbuffer);
    glDeleteTextures_nid_postfix(1, &texture);
    Require(Integer(0x8069) == 0);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd7);
    glDeleteFramebuffers_nid_postfix(1, &framebuffer);
    Require(Integer(0x8ca6) == 0);
    Error(0);
}

static void Shaders() {
    GLuint shader = glCreateShader_nid_postfix(0x8b31);
    GLuint program = glCreateProgram_nid_postfix();
    Require(shader && program && shader != program);
    const char first[]{'a', 0, 'b'};
    const char* strings[]{first, "tail"};
    GLint lengths[]{3, -1};
    glShaderSource_nid_postfix(shader, 2, strings, lengths);
    GLint sourceLength = 0;
    glGetShaderiv_nid_postfix(shader, 0x8b88, &sourceLength);
    Require(sourceLength == 8);
    GLint status = 1;
    glGetShaderiv_nid_postfix(shader, 0x8b81, &status);
    Require(status == 0);
    glAttachShader_nid_postfix(program, shader);
    glAttachShader_nid_postfix(program, shader);
    Error(0x0502);
    GLuint duplicateStage = glCreateShader_nid_postfix(0x8b31);
    glAttachShader_nid_postfix(program, duplicateStage);
    Error(0x0502);
    glDeleteShader_nid_postfix(duplicateStage);
    glDeleteShader_nid_postfix(shader);
    Require(glIsShader_nid_postfix(shader));
    glGetShaderiv_nid_postfix(shader, 0x8b80, &status);
    Require(status == 1);
    glDetachShader_nid_postfix(program, shader);
    Require(!glIsShader_nid_postfix(shader));
    glUseProgram_nid_postfix(program);
    Error(0x0502);
    glDeleteProgram_nid_postfix(program);
    Require(!glIsProgram_nid_postfix(program));
}

static void TextureFormats() {
    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);
    glPixelStorei_nid_postfix(0x0cf5, 8);
    const unsigned char pixels[]{255, 0, 0, 0, 255, 0, 99, 99, 0, 0, 255, 255, 255, 255, 99, 99};
    glTexImage2D_nid_postfix(0x0de1, 0, 0x1907, 2, 2, 0, 0x1907, 0x1401, pixels);
    glGenerateMipmap_nid_postfix(0x0de1);
    Error(0);
    GLuint framebuffer;
    glGenFramebuffers_nid_postfix(1, &framebuffer);
    glBindFramebuffer_nid_postfix(0x8d40, framebuffer);
    glFramebufferTexture2D_nid_postfix(0x8d40, 0x8ce0, 0x0de1, texture, 0);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd5);
    glPixelStorei_nid_postfix(0x0d05, 1);
    std::array<unsigned char, 4> pixel{};
    glReadPixels_nid_postfix(0, 1, 1, 1, 0x1908, 0x1401, pixel.data());
    Require((pixel == std::array<unsigned char, 4>{0, 0, 255, 255}));
    glReadPixels_nid_postfix(1, 1, 1, 1, 0x1908, 0x1401, pixel.data());
    Require((pixel == std::array<unsigned char, 4>{255, 255, 255, 255}));
    const std::uint16_t packed = 0xf800;
    glTexImage2D_nid_postfix(0x0de1, 0, 0x1907, 1, 1, 0, 0x1907, 0x8363, &packed);
    glReadPixels_nid_postfix(0, 0, 1, 1, 0x1908, 0x1401, pixel.data());
    Require((pixel == std::array<unsigned char, 4>{255, 0, 0, 255}));
    const unsigned char luminance[]{17, 42};
    glTexImage2D_nid_postfix(0x0de1, 0, 0x190a, 1, 1, 0, 0x190a, 0x1401, luminance);
    Require(glCheckFramebufferStatus_nid_postfix(0x8d40) == 0x8cd6);
    glBindFramebuffer_nid_postfix(0x8d40, 0);
    glDeleteFramebuffers_nid_postfix(1, &framebuffer);
    glPixelStorei_nid_postfix(0x0d05, 4);
    Error(0);
    glTexImage2D_nid_postfix(0x0de1, 0, 0x1908, 3, 2, 0, 0x1908, 0x1401, nullptr);
    glGenerateMipmap_nid_postfix(0x0de1);
    Error(0x0502);
    glTexImage2D_nid_postfix(0x0de1, 0, 0x1908, 1, 1, 0, 0x1908, 0x8363, &packed);
    Error(0x0502);
    glTexImage2D_nid_postfix(0x0de1, 0, 0, 1, 1, 0, 0x1908, 0x1401, nullptr);
    Error(0x0501);
    glTexImage2D_nid_postfix(0x0de1, 1, 0x1908, 3, 2, 0, 0x1908, 0x1401, nullptr);
    Error(0x0501);
    glDeleteTextures_nid_postfix(1, &texture);
    glPixelStorei_nid_postfix(0x0cf5, 4);
    Error(0);
}

static GLfloat Float(GLenum name) {
    GLfloat value = -99;
    glGetFloatv_nid_postfix(name, &value);
    return value;
}

template<std::size_t Count>
static std::array<GLfloat, Count> Floats(GLenum name) {
    std::array<GLfloat, Count> values{};
    glGetFloatv_nid_postfix(name, values.data());
    return values;
}

static GLboolean Boolean(GLenum name) {
    GLboolean value = 9;
    glGetBooleanv_nid_postfix(name, &value);
    return value;
}

static void EglError(EGLint expected) {
    Require(eglGetError_nid_postfix() == expected);
    Require(eglGetError_nid_postfix() == 0x3000);
}

static EGLint SurfaceValue(EGLDisplay display, EGLSurface surface, EGLint attribute) {
    EGLint value = -99;
    Require(eglQuerySurface_nid_postfix(display, surface, attribute, &value));
    return value;
}

static __eglMustCastToProperFunctionPointerType Expected(const char* name, __eglMustCastToProperFunctionPointerType linked) {
#ifdef _WIN32
    const std::string symbol = std::string(name) + "_nid_postfix";
    const auto resolved = reinterpret_cast<__eglMustCastToProperFunctionPointerType>(GetProcAddress(GetModuleHandleA("libSceGLSlimVSH.prx"), symbol.c_str()));
    if (!resolved) {
        std::fprintf(stderr, "%s is not exported by libSceGLSlimVSH\n", symbol.c_str());
        std::abort();
    }
    return resolved;
#else
    (void)name;
    return linked;
#endif
}

static void ProcAddress(const char* name, __eglMustCastToProperFunctionPointerType linked) {
    Require(eglGetProcAddress_nid_postfix(name) == Expected(name, linked));
}

#define ProcEqual(name) ProcAddress(#name, reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&name##_nid_postfix))

static void CurrentObjects(EGLDisplay display, EGLSurface surface) {
    Require(eglGetCurrentDisplay_nid_postfix() == display);
    Require(eglGetCurrentSurface_nid_postfix(0x3059) == surface);
    Require(eglGetCurrentSurface_nid_postfix(0x305a) == surface);
    EglError(0x3000);
    Require(eglGetCurrentSurface_nid_postfix(0x3000) == nullptr);
    EglError(0x300c);
}

static void SurfaceQueries(EGLDisplay display, EGLConfig config, EGLSurface surface) {
    const std::array<std::array<EGLint, 2>, 16> defaults{{
        {0x3057, 4}, {0x3056, 3}, {0x3058, 0}, {0x3080, 0x305c}, {0x3081, 0x305c}, {0x3082, 0},
        {0x3083, 0}, {0x3086, 0x3084}, {0x3087, 0x3089}, {0x3088, 0x308b}, {0x3090, -1}, {0x3091, -1},
        {0x3092, -1}, {0x3093, 0x3094}, {0x3099, 0x309a}, {0x3028, 1}
    }};
    for (auto [attribute, value] : defaults) Require(SurfaceValue(display, surface, attribute) == value);

    const EGLint attributes[]{0x3057, 8, 0x3056, 8, 0x3058, 1, 0x3082, 1, 0x3087, 0x308a, 0x3088, 0x308c, 0x3038};
    EGLSurface large = eglCreatePbufferSurface_nid_postfix(display, config, attributes);
    Require(large);
    Require(SurfaceValue(display, large, 0x3057) == 8);
    Require(SurfaceValue(display, large, 0x3056) == 8);
    Require(SurfaceValue(display, large, 0x3058) == 1);
    Require(SurfaceValue(display, large, 0x3082) == 1);
    Require(SurfaceValue(display, large, 0x3087) == 0x308a);
    Require(SurfaceValue(display, large, 0x3088) == 0x308c);
    Require(SurfaceValue(display, surface, 0x3057) == 4);
    Require(eglDestroySurface_nid_postfix(display, large));

    int token;
    EGLint value = -99;
    Require(!eglQuerySurface_nid_postfix(display, large, 0x3057, &value));
    EglError(0x300d);
    Require(!eglQuerySurface_nid_postfix(display, surface, 0x1234, &value));
    EglError(0x3004);
    Require(!eglQuerySurface_nid_postfix(display, surface, 0x3057, nullptr));
    EglError(0x300c);
    Require(!eglQuerySurface_nid_postfix(display, &token, 0x3057, &value));
    EglError(0x300d);
    Require(!eglQuerySurface_nid_postfix(&token, surface, 0x3057, &value));
    EglError(0x3008);
    Require(value == -99);
}

static void SurfaceAttributes(EGLDisplay display, EGLSurface surface) {
    Require(eglSurfaceAttrib_nid_postfix(display, surface, 0x3083, 2));
    Require(SurfaceValue(display, surface, 0x3083) == 2);
    Require(eglSurfaceAttrib_nid_postfix(display, surface, 0x3083, 0));
    Require(SurfaceValue(display, surface, 0x3083) == 0);
    Require(eglSurfaceAttrib_nid_postfix(display, surface, 0x3099, 0x309a));
    Require(eglSurfaceAttrib_nid_postfix(display, surface, 0x3093, 0x3094));
    Require(!eglSurfaceAttrib_nid_postfix(display, surface, 0x3099, 0x309b));
    EglError(0x3009);
    Require(!eglSurfaceAttrib_nid_postfix(display, surface, 0x3093, 0x3095));
    EglError(0x3009);
    Require(!eglSurfaceAttrib_nid_postfix(display, surface, 0x3099, 0x1234));
    EglError(0x3004);
    Require(!eglSurfaceAttrib_nid_postfix(display, surface, 0x3093, 0));
    EglError(0x3004);
    Require(!eglSurfaceAttrib_nid_postfix(display, surface, 0x1234, 0));
    EglError(0x3004);
    int token;
    Require(!eglSurfaceAttrib_nid_postfix(display, &token, 0x3083, 1));
    EglError(0x300d);
    Require(SurfaceValue(display, surface, 0x3083) == 0);
}

static void TextureImageBinding(EGLDisplay display, EGLSurface surface) {
    int token;
    Require(!eglBindTexImage_nid_postfix(display, surface, 0x3084));
    EglError(0x3009);
    Require(!eglReleaseTexImage_nid_postfix(display, surface, 0x3084));
    EglError(0x3009);
    Require(!eglBindTexImage_nid_postfix(display, surface, 0x3085));
    EglError(0x300c);
    Require(!eglReleaseTexImage_nid_postfix(display, surface, 0x3085));
    EglError(0x300c);
    Require(!eglBindTexImage_nid_postfix(display, &token, 0x3084));
    EglError(0x300d);
    Require(!eglReleaseTexImage_nid_postfix(display, &token, 0x3084));
    EglError(0x300d);
}

static void NativeWait() {
    Require(eglWaitNative_nid_postfix(0x305b));
    Require(eglGetError_nid_postfix() == 0x3000);
    Require(!eglWaitNative_nid_postfix(0x3000));
    EglError(0x300c);
}

static void QueryString(EGLDisplay display) {
    Require(std::strcmp(eglQueryString_nid_postfix(display, 0x3055), "") == 0);
    Require(std::strcmp(eglQueryString_nid_postfix(display, 0x308d), "OpenGL_ES") == 0);
    Require(std::strncmp(eglQueryString_nid_postfix(display, 0x3054), "1.4", 3) == 0);
    Require(eglGetError_nid_postfix() == 0x3000);
    Require(eglQueryString_nid_postfix(display, 0x3000) == nullptr);
    EglError(0x300c);
    int token;
    Require(eglQueryString_nid_postfix(&token, 0x3055) == nullptr);
    EglError(0x3008);
}

static void ProcAddresses() {
    Require(eglGetProcAddress_nid_postfix(nullptr) == nullptr);
    Require(eglGetProcAddress_nid_postfix("") == nullptr);
    Require(eglGetProcAddress_nid_postfix("glNotAFunction") == nullptr);
    Require(eglGetProcAddress_nid_postfix("GLCLEAR") == nullptr);
    Require(eglGetProcAddress_nid_postfix("glClear_nid_postfix") == nullptr);
    ProcEqual(glClear);
    ProcEqual(glStencilFunc);
    ProcEqual(glGetBooleanv);
    ProcEqual(glGetAttachedShaders);
    ProcEqual(glCopyTexSubImage2D);
    ProcEqual(eglGetProcAddress);
    ProcEqual(eglQuerySurface);
    ProcEqual(eglGetCurrentDisplay);
    ProcEqual(eglQueryString);
    Require(eglGetError_nid_postfix() == 0x3000);
}

static void Rasterizer() {
    Require(Integer(0x0b45) == 0x0405);
    Require(Integer(0x0b46) == 0x0901);
    Require(Float(0x0b21) == 1);
    glCullFace_nid_postfix(0x0404);
    Require(Integer(0x0b45) == 0x0404);
    glCullFace_nid_postfix(0x0408);
    Require(Integer(0x0b45) == 0x0408);
    glCullFace_nid_postfix(0x0900);
    Error(0x0500);
    Require(Integer(0x0b45) == 0x0408);
    glCullFace_nid_postfix(0x0405);
    glFrontFace_nid_postfix(0x0900);
    Require(Integer(0x0b46) == 0x0900);
    glFrontFace_nid_postfix(0x0902);
    Error(0x0500);
    Require(Integer(0x0b46) == 0x0900);
    glFrontFace_nid_postfix(0x0901);
    glLineWidth_nid_postfix(2.5f);
    Require(Float(0x0b21) == 2.5f);
    glLineWidth_nid_postfix(0);
    Error(0x0501);
    glLineWidth_nid_postfix(-1);
    Error(0x0501);
    Require(Float(0x0b21) == 2.5f);
    glLineWidth_nid_postfix(1);

    glDepthMask_nid_postfix(0);
    Require(Boolean(0x0b72) == 0);
    glDepthMask_nid_postfix(7);
    Require(Boolean(0x0b72) == 1);
    Require((Floats<2>(0x0b70) == std::array<GLfloat, 2>{0, 1}));
    glDepthRangef_nid_postfix(0.25f, 0.75f);
    Require((Floats<2>(0x0b70) == std::array<GLfloat, 2>{0.25f, 0.75f}));
    glDepthRangef_nid_postfix(1, 0);
    Require((Floats<2>(0x0b70) == std::array<GLfloat, 2>{1, 0}));
    glDepthRangef_nid_postfix(-1, 2);
    Require((Floats<2>(0x0b70) == std::array<GLfloat, 2>{0, 1}));

    glPolygonOffset_nid_postfix(1.5f, -2);
    Require(Float(0x8038) == 1.5f);
    Require(Float(0x2a00) == -2);
    glPolygonOffset_nid_postfix(0, 0);
    Require(Float(0x8038) == 0 && Float(0x2a00) == 0);

    Require(Float(0x80aa) == 1 && Boolean(0x80ab) == 0);
    glSampleCoverage_nid_postfix(0.5f, 7);
    Require(Float(0x80aa) == 0.5f && Boolean(0x80ab) == 1);
    glSampleCoverage_nid_postfix(2, 1);
    Require(Float(0x80aa) == 1 && Boolean(0x80ab) == 1);
    glSampleCoverage_nid_postfix(-1, 0);
    Require(Float(0x80aa) == 0 && Boolean(0x80ab) == 0);
    glSampleCoverage_nid_postfix(1, 0);

    Require(Integer(0x8192) == 0x1100);
    glHint_nid_postfix(0x8192, 0x1102);
    Require(Integer(0x8192) == 0x1102);
    glHint_nid_postfix(0x8192, 0x1103);
    Error(0x0500);
    glHint_nid_postfix(0x0c50, 0x1100);
    Error(0x0500);
    Require(Integer(0x8192) == 0x1102);
    glHint_nid_postfix(0x8192, 0x1100);
    Error(0);
}

static void Blending() {
    Require((Floats<4>(0x8005) == std::array<GLfloat, 4>{0, 0, 0, 0}));
    glBlendColor_nid_postfix(-1, 2, 0.5f, 0.25f);
    Require((Floats<4>(0x8005) == std::array<GLfloat, 4>{0, 1, 0.5f, 0.25f}));
    glBlendColor_nid_postfix(0, 0, 0, 0);

    glBlendEquation_nid_postfix(0x800b);
    Require(Integer(0x8009) == 0x800b && Integer(0x883d) == 0x800b);
    glBlendEquationSeparate_nid_postfix(0x800a, 0x8006);
    Require(Integer(0x8009) == 0x800a && Integer(0x883d) == 0x8006);
    glBlendEquationSeparate_nid_postfix(0x800a, 0x8007);
    Error(0x0500);
    glBlendEquationSeparate_nid_postfix(0x8007, 0x800b);
    Error(0x0500);
    glBlendEquation_nid_postfix(0x8007);
    Error(0x0500);
    Require(Integer(0x8009) == 0x800a && Integer(0x883d) == 0x8006);
    glBlendEquation_nid_postfix(0x8006);
    Require(Integer(0x8009) == 0x8006 && Integer(0x883d) == 0x8006);
}

static void Stencil() {
    glStencilFunc_nid_postfix(0x0200, 300, 0x0f);
    Require(Integer(0x0b92) == 0x0200 && Integer(0x8800) == 0x0200);
    Require(Integer(0x0b97) == 255 && Integer(0x8ca3) == 255);
    Require(Integer(0x0b93) == 15 && Integer(0x8ca4) == 15);
    glStencilFunc_nid_postfix(0x0201, -5, 1);
    Require(Integer(0x0b97) == 0 && Integer(0x8ca3) == 0);
    glStencilFuncSeparate_nid_postfix(0x0404, 0x0202, 3, 0xff);
    Require(Integer(0x0b92) == 0x0202 && Integer(0x0b97) == 3 && Integer(0x0b93) == 255);
    Require(Integer(0x8800) == 0x0201 && Integer(0x8ca3) == 0 && Integer(0x8ca4) == 1);
    glStencilFuncSeparate_nid_postfix(0x0405, 0x0204, 4, 7);
    Require(Integer(0x0b92) == 0x0202 && Integer(0x0b97) == 3 && Integer(0x0b93) == 255);
    Require(Integer(0x8800) == 0x0204 && Integer(0x8ca3) == 4 && Integer(0x8ca4) == 7);
    glStencilFuncSeparate_nid_postfix(0x0900, 0x0200, 0, 0);
    Error(0x0500);
    glStencilFuncSeparate_nid_postfix(0x0404, 0x0208, 0, 0);
    Error(0x0500);
    Require(Integer(0x0b92) == 0x0202 && Integer(0x0b97) == 3 && Integer(0x0b93) == 255);
    Require(Integer(0x8800) == 0x0204 && Integer(0x8ca3) == 4 && Integer(0x8ca4) == 7);
    glStencilFunc_nid_postfix(0x0207, 0, 0xffffffff);

    glStencilMaskSeparate_nid_postfix(0x0404, 0x0f);
    Require(Integer(0x0b98) == 15 && Integer(0x8ca5) == -1);
    glStencilMaskSeparate_nid_postfix(0x0405, 0xf0);
    Require(Integer(0x0b98) == 15 && Integer(0x8ca5) == 240);
    glStencilMaskSeparate_nid_postfix(0x0900, 0);
    Error(0x0500);
    Require(Integer(0x0b98) == 15 && Integer(0x8ca5) == 240);
    glStencilMask_nid_postfix(0xffffffff);
    Require(Integer(0x0b98) == -1 && Integer(0x8ca5) == -1);

    Require(Integer(0x0b94) == 0x1e00 && Integer(0x0b95) == 0x1e00 && Integer(0x0b96) == 0x1e00);
    glStencilOp_nid_postfix(0x1e01, 0x1e02, 0x150a);
    Require(Integer(0x0b94) == 0x1e01 && Integer(0x0b95) == 0x1e02 && Integer(0x0b96) == 0x150a);
    Require(Integer(0x8801) == 0x1e01 && Integer(0x8802) == 0x1e02 && Integer(0x8803) == 0x150a);
    glStencilOpSeparate_nid_postfix(0x0405, 0x1e03, 0x8507, 0x8508);
    Require(Integer(0x0b94) == 0x1e01 && Integer(0x0b95) == 0x1e02 && Integer(0x0b96) == 0x150a);
    Require(Integer(0x8801) == 0x1e03 && Integer(0x8802) == 0x8507 && Integer(0x8803) == 0x8508);
    glStencilOp_nid_postfix(0, 0x1e04, 0);
    Error(0x0500);
    glStencilOpSeparate_nid_postfix(0x0900, 0, 0, 0);
    Error(0x0500);
    Require(Integer(0x0b94) == 0x1e01 && Integer(0x0b95) == 0x1e02 && Integer(0x0b96) == 0x150a);
    Require(Integer(0x8801) == 0x1e03 && Integer(0x8802) == 0x8507 && Integer(0x8803) == 0x8508);
    glStencilOp_nid_postfix(0x1e00, 0x1e00, 0x1e00);
    Require(Integer(0x8801) == 0x1e00 && Integer(0x8802) == 0x1e00 && Integer(0x8803) == 0x1e00);
}

static void Booleans() {
    Require(Boolean(0x0be2) == 0);
    glEnable_nid_postfix(0x0be2);
    Require(Boolean(0x0be2) == 1);
    glDisable_nid_postfix(0x0be2);
    Require(Boolean(0x0be2) == 0);
    glColorMask_nid_postfix(1, 0, 1, 0);
    GLboolean mask[4]{9, 9, 9, 9};
    glGetBooleanv_nid_postfix(0x0c23, mask);
    Require(mask[0] == 1 && mask[1] == 0 && mask[2] == 1 && mask[3] == 0);
    glColorMask_nid_postfix(1, 1, 1, 1);
    GLboolean untouched = 7;
    glGetBooleanv_nid_postfix(0xffffffff, &untouched);
    Require(untouched == 7);
    Error(0x0500);
    GLboolean flag = 0;
    glGetBooleanv_nid_postfix(0x0bd0, nullptr);
    glGetBooleanv_nid_postfix(0xffffffff, nullptr);
    glGetBooleanv_nid_postfix(0x0bd0, &flag);
    Require(flag == 1);
}

static void VertexAttributes() {
    const GLfloat values[4]{1, 2, 3, 4};
    const auto current = [] {
        std::array<GLfloat, 4> value{};
        glGetVertexAttribfv_nid_postfix(5, 0x8626, value.data());
        return value;
    };
    const auto reset = [] { glVertexAttrib4f_nid_postfix(5, 9, 9, 9, 9); };
    reset();
    glVertexAttrib1fv_nid_postfix(5, values);
    Require((current() == std::array<GLfloat, 4>{1, 0, 0, 1}));
    reset();
    glVertexAttrib2fv_nid_postfix(5, values);
    Require((current() == std::array<GLfloat, 4>{1, 2, 0, 1}));
    reset();
    glVertexAttrib3f_nid_postfix(5, 1, 2, 3);
    Require((current() == std::array<GLfloat, 4>{1, 2, 3, 1}));
    reset();
    glVertexAttrib3fv_nid_postfix(5, values);
    Require((current() == std::array<GLfloat, 4>{1, 2, 3, 1}));
    reset();
    glVertexAttrib4f_nid_postfix(5, 1, 2, 3, 4);
    Require((current() == std::array<GLfloat, 4>{1, 2, 3, 4}));
    reset();
    glVertexAttrib4fv_nid_postfix(5, values);
    Require((current() == std::array<GLfloat, 4>{1, 2, 3, 4}));
    glVertexAttrib4f_nid_postfix(5, 0, 0, 0, 1);
    glVertexAttrib1fv_nid_postfix(8, values);
    Error(0x0501);
    glVertexAttrib2fv_nid_postfix(8, values);
    Error(0x0501);
    glVertexAttrib3f_nid_postfix(8, 1, 2, 3);
    Error(0x0501);
    glVertexAttrib3fv_nid_postfix(8, values);
    Error(0x0501);
    glVertexAttrib4f_nid_postfix(8, 1, 2, 3, 4);
    Error(0x0501);
    glVertexAttrib4fv_nid_postfix(8, values);
    Error(0x0501);

    GLint enabled = -99;
    glEnableVertexAttribArray_nid_postfix(2);
    glGetVertexAttribiv_nid_postfix(2, 0x8622, &enabled);
    Require(enabled == 1);
    glDisableVertexAttribArray_nid_postfix(2);
    glGetVertexAttribiv_nid_postfix(2, 0x8622, &enabled);
    Require(enabled == 0);
    glDisableVertexAttribArray_nid_postfix(8);
    Error(0x0501);

    float local[3]{};
    void* pointer = nullptr;
    glVertexAttribPointer_nid_postfix(2, 3, 0x1406, 0, 12, local);
    glGetVertexAttribPointerv_nid_postfix(2, 0x8645, &pointer);
    Require(pointer == local);
    glGetVertexAttribPointerv_nid_postfix(2, 0x8644, &pointer);
    Error(0x0500);
    Require(pointer == local);
    glGetVertexAttribPointerv_nid_postfix(8, 0x8645, &pointer);
    Error(0x0501);
    Require(pointer == local);
    glVertexAttribPointer_nid_postfix(2, 4, 0x1406, 0, 0, nullptr);
    glGetVertexAttribPointerv_nid_postfix(2, 0x8645, &pointer);
    Require(pointer == nullptr);
}

static void TextureParameters() {
    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);
    const auto parameter = [](GLenum name) {
        GLfloat value = -99;
        glGetTexParameterfv_nid_postfix(0x0de1, name, &value);
        return value;
    };
    Require(parameter(0x2800) == 9729 && parameter(0x2801) == 9986);
    Require(parameter(0x2802) == 10497 && parameter(0x2803) == 10497);
    glTexParameterf_nid_postfix(0x0de1, 0x2801, 9729);
    Require(parameter(0x2801) == 9729);
    const GLint wrapS[]{0x812f};
    glTexParameteriv_nid_postfix(0x0de1, 0x2802, wrapS);
    Require(parameter(0x2802) == 0x812f);
    const GLfloat wrapT[]{33648};
    glTexParameterfv_nid_postfix(0x0de1, 0x2803, wrapT);
    Require(parameter(0x2803) == 0x8370);
    Error(0);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, 9728.5f);
    Error(0x0500);
    glTexParameterf_nid_postfix(0x0de1, 0x2800, std::numeric_limits<GLfloat>::quiet_NaN());
    Error(0x0500);
    const GLfloat fractional[]{9728.5f};
    glTexParameterfv_nid_postfix(0x0de1, 0x2800, fractional);
    Error(0x0500);
    const GLint minification[]{0x2700};
    glTexParameteriv_nid_postfix(0x0de1, 0x2800, minification);
    Error(0x0500);
    Require(parameter(0x2800) == 9729);
    GLfloat unknown = -99;
    glGetTexParameterfv_nid_postfix(0x0de1, 0x84fe, &unknown);
    Error(0x0500);
    Require(unknown == -99);
    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(1, &texture);
    Error(0);
}

static void Renderbuffers() {
    GLuint renderbuffer;
    glGenRenderbuffers_nid_postfix(1, &renderbuffer);
    Require(!glIsRenderbuffer_nid_postfix(renderbuffer));
    Require(!glIsRenderbuffer_nid_postfix(0));
    glBindRenderbuffer_nid_postfix(0x8d41, renderbuffer);
    Require(glIsRenderbuffer_nid_postfix(renderbuffer));
    const auto parameters = [] {
        std::array<GLint, 9> values{};
        const GLenum names[]{0x8d42, 0x8d43, 0x8d44, 0x8d50, 0x8d51, 0x8d52, 0x8d53, 0x8d54, 0x8d55};
        for (unsigned i = 0; i < values.size(); ++i) glGetRenderbufferParameteriv_nid_postfix(0x8d41, names[i], &values[i]);
        return values;
    };
    Require((parameters() == std::array<GLint, 9>{0, 0, 0x8056, 0, 0, 0, 0, 0, 0}));
    glRenderbufferStorage_nid_postfix(0x8d41, 0x8057, 6, 5);
    Require((parameters() == std::array<GLint, 9>{6, 5, 0x8057, 5, 5, 5, 1, 0, 0}));
    glRenderbufferStorage_nid_postfix(0x8d41, 0x8d62, 6, 5);
    Require((parameters() == std::array<GLint, 9>{6, 5, 0x8d62, 5, 6, 5, 0, 0, 0}));
    glRenderbufferStorage_nid_postfix(0x8d41, 0x8056, 6, 5);
    Require((parameters() == std::array<GLint, 9>{6, 5, 0x8056, 4, 4, 4, 4, 0, 0}));
    glRenderbufferStorage_nid_postfix(0x8d41, 0x81a5, 6, 5);
    Require((parameters() == std::array<GLint, 9>{6, 5, 0x81a5, 0, 0, 0, 0, 16, 0}));
    glRenderbufferStorage_nid_postfix(0x8d41, 0x8d48, 6, 5);
    Require((parameters() == std::array<GLint, 9>{6, 5, 0x8d48, 0, 0, 0, 0, 0, 8}));
    Error(0);

    GLint value = -99;
    glGetRenderbufferParameteriv_nid_postfix(0x8d40, 0x8d42, &value);
    Error(0x0500);
    glGetRenderbufferParameteriv_nid_postfix(0x8d41, 0x8d56, &value);
    Error(0x0500);
    glBindRenderbuffer_nid_postfix(0x8d41, 0);
    glGetRenderbufferParameteriv_nid_postfix(0x8d41, 0x8d42, &value);
    Error(0x0502);
    Require(value == -99);
    glDeleteRenderbuffers_nid_postfix(1, &renderbuffer);
    Require(!glIsRenderbuffer_nid_postfix(renderbuffer));
    Error(0);
}

static void FramebufferAttachments() {
    GLuint texture, cube, renderbuffer, framebuffer;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);
    glGenTextures_nid_postfix(1, &cube);
    glBindTexture_nid_postfix(0x8513, cube);
    glGenRenderbuffers_nid_postfix(1, &renderbuffer);
    glBindRenderbuffer_nid_postfix(0x8d41, renderbuffer);
    glGenFramebuffers_nid_postfix(1, &framebuffer);
    GLint value = -99;
    glGetFramebufferAttachmentParameteriv_nid_postfix(0x8d40, 0x8ce0, 0x8cd0, &value);
    Error(0x0502);
    glBindFramebuffer_nid_postfix(0x8d40, framebuffer);
    glGetFramebufferAttachmentParameteriv_nid_postfix(0x8d41, 0x8ce0, 0x8cd0, &value);
    Error(0x0500);
    glGetFramebufferAttachmentParameteriv_nid_postfix(0x8d40, 0x8ce1, 0x8cd0, &value);
    Error(0x0500);
    Require(value == -99);

    const auto query = [&value](GLenum attachment, GLenum name) {
        value = -99;
        glGetFramebufferAttachmentParameteriv_nid_postfix(0x8d40, attachment, name, &value);
        return value;
    };
    Require(query(0x8d20, 0x8cd0) == 0);
    Require(query(0x8d20, 0x8cd1) == -99);
    Error(0x0500);

    glFramebufferTexture2D_nid_postfix(0x8d40, 0x8ce0, 0x0de1, texture, 0);
    Require(query(0x8ce0, 0x8cd0) == 0x1702);
    Require(query(0x8ce0, 0x8cd1) == static_cast<GLint>(texture));
    Require(query(0x8ce0, 0x8cd2) == 0);
    Require(query(0x8ce0, 0x8cd3) == 0);
    glFramebufferTexture2D_nid_postfix(0x8d40, 0x8ce0, 0x8516, cube, 0);
    Require(query(0x8ce0, 0x8cd0) == 0x1702);
    Require(query(0x8ce0, 0x8cd1) == static_cast<GLint>(cube));
    Require(query(0x8ce0, 0x8cd3) == 0x8516);

    glFramebufferRenderbuffer_nid_postfix(0x8d40, 0x8d00, 0x8d41, renderbuffer);
    Require(query(0x8d00, 0x8cd0) == 0x8d41);
    Require(query(0x8d00, 0x8cd1) == static_cast<GLint>(renderbuffer));
    Require(query(0x8d00, 0x8cd2) == -99);
    Error(0x0500);

    glBindFramebuffer_nid_postfix(0x8d40, 0);
    glDeleteFramebuffers_nid_postfix(1, &framebuffer);
    glDeleteRenderbuffers_nid_postfix(1, &renderbuffer);
    glBindTexture_nid_postfix(0x8513, 0);
    glBindTexture_nid_postfix(0x0de1, 0);
    GLuint textures[]{texture, cube};
    glDeleteTextures_nid_postfix(2, textures);
    Error(0);
}

static void ShaderObjects() {
    GLuint vertex = glCreateShader_nid_postfix(0x8b31);
    GLuint fragment = glCreateShader_nid_postfix(0x8b30);
    GLuint program = glCreateProgram_nid_postfix();
    Require(vertex && fragment && program && vertex < fragment);
    glAttachShader_nid_postfix(program, vertex);
    glAttachShader_nid_postfix(program, fragment);
    Error(0);

    GLsizei count = -99;
    GLuint attached[8]{};
    glGetAttachedShaders_nid_postfix(program, 8, &count, attached);
    Require(count == 2 && attached[0] == vertex && attached[1] == fragment);
    attached[1] = 0;
    glGetAttachedShaders_nid_postfix(program, 1, &count, attached);
    Require(count == 1 && attached[0] == vertex && attached[1] == 0);
    attached[0] = 0;
    count = -99;
    glGetAttachedShaders_nid_postfix(program, 0, &count, attached);
    Require(count == 0 && attached[0] == 0);
    count = -99;
    glGetAttachedShaders_nid_postfix(program, -1, &count, attached);
    Error(0x0501);
    Require(count == -99);
    glGetAttachedShaders_nid_postfix(program, 8, nullptr, attached);
    Require(attached[0] == vertex && attached[1] == fragment);
    glGetAttachedShaders_nid_postfix(vertex, 8, &count, attached);
    Error(0x0502);
    glGetAttachedShaders_nid_postfix(9999, 8, &count, attached);
    Error(0x0501);

    char log[8];
    GLsizei length = -99;
    std::memset(log, 'x', sizeof(log));
    glGetShaderInfoLog_nid_postfix(vertex, 8, &length, log);
    Require(length == 0 && log[0] == 0 && log[1] == 'x');
    std::memset(log, 'x', sizeof(log));
    glGetShaderInfoLog_nid_postfix(vertex, 1, &length, log);
    Require(length == 0 && log[0] == 0 && log[1] == 'x');
    std::memset(log, 'x', sizeof(log));
    glGetShaderInfoLog_nid_postfix(vertex, 0, nullptr, log);
    Require(log[0] == 'x');
    glGetShaderInfoLog_nid_postfix(vertex, 8, nullptr, log);
    Require(log[0] == 0);
    std::memset(log, 'x', sizeof(log));
    glGetShaderInfoLog_nid_postfix(vertex, -1, &length, log);
    Error(0x0501);
    Require(log[0] == 'x');
    glGetShaderInfoLog_nid_postfix(program, 8, &length, log);
    Error(0x0502);
    Require(log[0] == 'x');
    glGetShaderInfoLog_nid_postfix(9999, 8, &length, log);
    Error(0x0501);

    length = -99;
    glGetProgramInfoLog_nid_postfix(program, 8, &length, log);
    Require(length == 0 && log[0] == 0 && log[1] == 'x');
    std::memset(log, 'x', sizeof(log));
    glGetProgramInfoLog_nid_postfix(program, 1, &length, log);
    Require(length == 0 && log[0] == 0 && log[1] == 'x');
    std::memset(log, 'x', sizeof(log));
    glGetProgramInfoLog_nid_postfix(program, 0, nullptr, log);
    Require(log[0] == 'x');
    glGetProgramInfoLog_nid_postfix(program, 8, nullptr, log);
    Require(log[0] == 0);
    std::memset(log, 'x', sizeof(log));
    glGetProgramInfoLog_nid_postfix(program, -1, &length, log);
    Error(0x0501);
    Require(log[0] == 'x');
    glGetProgramInfoLog_nid_postfix(vertex, 8, &length, log);
    Error(0x0502);
    Require(log[0] == 'x');
    glGetProgramInfoLog_nid_postfix(9999, 8, &length, log);
    Error(0x0501);

    glBindAttribLocation_nid_postfix(program, 3, "aPos");
    Error(0);
    glBindAttribLocation_nid_postfix(program, 8, "a");
    Error(0x0501);
    glBindAttribLocation_nid_postfix(program, 0, "gl_Position");
    Error(0x0502);
    glBindAttribLocation_nid_postfix(vertex, 0, "a");
    Error(0x0502);
    glBindAttribLocation_nid_postfix(9999, 0, "a");
    Error(0x0501);

    glDetachShader_nid_postfix(program, vertex);
    glGetAttachedShaders_nid_postfix(program, 8, &count, attached);
    Require(count == 1 && attached[0] == fragment);
    glDeleteProgram_nid_postfix(program);
    glDeleteShader_nid_postfix(vertex);
    glDeleteShader_nid_postfix(fragment);
    Require(!glIsShader_nid_postfix(vertex) && !glIsShader_nid_postfix(fragment) && !glIsProgram_nid_postfix(program));
    Error(0);
}

static void CopySubImage() {
    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);
    glCopyTexSubImage2D_nid_postfix(0x0de1, 0, 0, 0, 0, 0, 1, 1);
    Error(0x0502);
    glClearColor_nid_postfix(1, 0, 0, 1);
    glClear_nid_postfix(0x4000);
    glCopyTexImage2D_nid_postfix(0x0de1, 0, 0x1908, 0, 0, 4, 3, 0);
    glClearColor_nid_postfix(0, 1, 0, 1);
    glClear_nid_postfix(0x4000);
    glCopyTexSubImage2D_nid_postfix(0x0de1, 0, 1, 1, 2, 0, 2, 1);
    Error(0);
    glCopyTexSubImage2D_nid_postfix(0x0de1, 0, 3, 1, 0, 0, 2, 1);
    Error(0x0501);
    glCopyTexSubImage2D_nid_postfix(0x0de1, 12, 0, 0, 0, 0, 1, 1);
    Error(0x0501);
    glCopyTexSubImage2D_nid_postfix(0x0de1, 1, 0, 0, 0, 0, 1, 1);
    Error(0x0502);
    glCopyTexSubImage2D_nid_postfix(0x0de2, 0, 0, 0, 0, 0, 1, 1);
    Error(0x0500);
    GLuint framebuffer;
    glGenFramebuffers_nid_postfix(1, &framebuffer);
    glBindFramebuffer_nid_postfix(0x8d40, framebuffer);
    glCopyTexSubImage2D_nid_postfix(0x0de1, 0, 0, 0, 0, 0, 1, 1);
    Error(0x0506);
    glFramebufferTexture2D_nid_postfix(0x8d40, 0x8ce0, 0x0de1, texture, 0);
    std::array<unsigned char, 48> data{};
    glReadPixels_nid_postfix(0, 0, 4, 3, 0x1908, 0x1401, data.data());
    for (unsigned y = 0; y < 3; ++y) for (unsigned x = 0; x < 4; ++x) {
        const bool green = y == 1 && (x == 1 || x == 2);
        const unsigned char* pixel = &data[(y * 4 + x) * 4];
        Require(pixel[0] == (green ? 0 : 255) && pixel[1] == (green ? 255 : 0) && pixel[2] == 0 && pixel[3] == 255);
    }
    glBindFramebuffer_nid_postfix(0x8d40, 0);
    glDeleteFramebuffers_nid_postfix(1, &framebuffer);
    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(1, &texture);
    Error(0);
}

int main() {
    Require(eglGetCurrentContext_nid_postfix() == nullptr);
    Require(eglQueryAPI_nid_postfix() == 0x30a0);
    Require(eglGetError_nid_postfix() == 0x3000);
    EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    EGLConfig config{};
    EGLint count;
    Require(!eglGetConfigs_nid_postfix(display, &config, 1, &count));
    Require(eglGetError_nid_postfix() == 0x3001);
    EGLint major, minor;
    Require(eglInitialize_nid_postfix(display, &major, &minor));
    Require(major == 1 && minor == 4);
    Require(eglGetConfigs_nid_postfix(display, &config, 1, &count) && count == 1);
    EGLint attribute;
    Require(eglGetConfigAttrib_nid_postfix(display, config, 0x3029, &attribute) && attribute == 0);
    Require(eglGetConfigAttrib_nid_postfix(display, config, 0x302c, &attribute) && attribute == 2048);
    Require(eglGetConfigAttrib_nid_postfix(display, config, 0x3042, &attribute) && attribute == 0);
    const EGLint criteria[]{0x3033, 1, 0x3040, 4, 0x3038};
    Require(eglChooseConfig_nid_postfix(display, criteria, &config, 1, &count) && count == 1);
    Require(eglChooseConfig_nid_postfix(display, nullptr, nullptr, 0, &count) && count == 0);
    Require(!eglCreateContext_nid_postfix(display, config, nullptr, nullptr));
    Require(eglGetError_nid_postfix() == 0x3005);
    const EGLint contextAttributes[]{0x3098, 2, 0x3038};
    const EGLint surfaceAttributes[]{0x3057, 4, 0x3056, 3, 0x3038};
    EGLContext context = eglCreateContext_nid_postfix(display, config, nullptr, contextAttributes);
    EGLSurface surface = eglCreatePbufferSurface_nid_postfix(display, config, surfaceAttributes);
    Require(context && surface);
    Require(eglQueryContext_nid_postfix(display, context, 0x3086, &attribute) && attribute == 0x3038);
    const EGLint invalidSurface[]{0x3057, -1, 0x3038};
    Require(!eglCreatePbufferSurface_nid_postfix(display, config, invalidSurface));
    Require(eglGetError_nid_postfix() == 0x300c);
    Require(eglMakeCurrent_nid_postfix(display, surface, surface, context));
    CurrentObjects(display, surface);
    SurfaceQueries(display, config, surface);
    SurfaceAttributes(display, surface);
    TextureImageBinding(display, surface);
    NativeWait();
    QueryString(display);
    ProcAddresses();
    Defaults();
    Buffers();
    Pixels();
    TextureFormats();
    Shaders();
    Rasterizer();
    Blending();
    Stencil();
    Booleans();
    VertexAttributes();
    TextureParameters();
    Renderbuffers();
    FramebufferAttachments();
    ShaderObjects();
    CopySubImage();
    GLuint sharedBuffer;
    glGenBuffers_nid_postfix(1, &sharedBuffer);
    glBindBuffer_nid_postfix(0x8892, sharedBuffer);
    EGLContext shared = eglCreateContext_nid_postfix(display, config, context, contextAttributes);
    EGLSurface otherSurface = eglCreatePbufferSurface_nid_postfix(display, config, surfaceAttributes);
    Require(eglSwapBuffers_nid_postfix(display, surface));
    Require(!eglSwapBuffers_nid_postfix(display, otherSurface));
    Require(eglGetError_nid_postfix() == 0x300d);
    std::thread worker([&] {
        Require(!eglSwapBuffers_nid_postfix(display, otherSurface));
        Require(eglGetError_nid_postfix() == 0x300d);
        Require(!eglGetCurrentContext_nid_postfix());
        Require(!eglGetCurrentDisplay_nid_postfix());
        Require(!eglGetCurrentSurface_nid_postfix(0x3059));
        Require(!eglGetCurrentSurface_nid_postfix(0x305a));
        Require(eglGetError_nid_postfix() == 0x3000);
        Require(!eglMakeCurrent_nid_postfix(display, surface, surface, context));
        Require(eglGetError_nid_postfix() == 0x3002);
        Require(eglMakeCurrent_nid_postfix(display, otherSurface, otherSurface, shared));
        Require(eglGetCurrentDisplay_nid_postfix() == display);
        Require(eglGetCurrentSurface_nid_postfix(0x3059) == otherSurface);
        Require(eglGetCurrentSurface_nid_postfix(0x305a) == otherSurface);
        Require(glIsBuffer_nid_postfix(sharedBuffer));
        Require(Integer(0x8894) == 0);
        glEnable_nid_postfix(0x0be2);
        Require(eglReleaseThread_nid_postfix());
        Require(!eglGetCurrentDisplay_nid_postfix());
        Require(!eglGetCurrentSurface_nid_postfix(0x3059));
        Require(!eglGetCurrentSurface_nid_postfix(0x305a));
    });
    worker.join();
    Require(!glIsEnabled_nid_postfix(0x0be2));
    Require(eglGetError_nid_postfix() == 0x3000);
    Require(eglDestroyContext_nid_postfix(display, shared));
    Require(eglDestroySurface_nid_postfix(display, otherSurface));
    Require(eglDestroyContext_nid_postfix(display, context));
    Require(eglDestroySurface_nid_postfix(display, surface));
    Require(eglGetCurrentContext_nid_postfix() == context);
    glClear_nid_postfix(0x4000);
    Error(0);
    Require(eglReleaseThread_nid_postfix());
    Require(!eglGetCurrentContext_nid_postfix());
    Require(!eglDestroyContext_nid_postfix(display, context));
    Require(eglGetError_nid_postfix() == 0x3006);
    Require(eglTerminate_nid_postfix(display));
    Require(!eglQueryString_nid_postfix(display, 0x3055));
    Require(eglGetError_nid_postfix() == 0x3001);
    std::puts("GLSlim state tests passed");
}
