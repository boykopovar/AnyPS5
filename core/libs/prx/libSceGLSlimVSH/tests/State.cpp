#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>

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
    Defaults();
    Buffers();
    Pixels();
    TextureFormats();
    Shaders();
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
        Require(!eglMakeCurrent_nid_postfix(display, surface, surface, context));
        Require(eglGetError_nid_postfix() == 0x3002);
        Require(eglMakeCurrent_nid_postfix(display, otherSurface, otherSurface, shared));
        Require(glIsBuffer_nid_postfix(sharedBuffer));
        Require(Integer(0x8894) == 0);
        glEnable_nid_postfix(0x0be2);
        Require(eglReleaseThread_nid_postfix());
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
    std::puts("GLSlim state tests passed");
}
