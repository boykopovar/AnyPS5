#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GLSlim texture units check failed at line %d\n", line);
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

static void ActiveTextureTests() {
    Require(Integer(0x84e0) == 0x84c0);

    glActiveTexture_nid_postfix(0x84c0);
    Error(0);
    Require(Integer(0x84e0) == 0x84c0);

    glActiveTexture_nid_postfix(0x84c1);
    Error(0);
    Require(Integer(0x84e0) == 0x84c1);

    glActiveTexture_nid_postfix(0x84c7);
    Error(0);
    Require(Integer(0x84e0) == 0x84c7);

    glActiveTexture_nid_postfix(0x84c8);
    Error(0x0500);
    Require(Integer(0x84e0) == 0x84c7);

    glActiveTexture_nid_postfix(0x84bf);
    Error(0x0500);
    Require(Integer(0x84e0) == 0x84c7);

    glActiveTexture_nid_postfix(0);
    Error(0x0500);
    Require(Integer(0x84e0) == 0x84c7);

    glActiveTexture_nid_postfix(0x84c3);
    Error(0);
    Require(Integer(0x84e0) == 0x84c3);

    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);
    glTexParameteri_nid_postfix(0x0de1, 0x2801, 0x2600);

    glActiveTexture_nid_postfix(0x84c0);
    Error(0);
    Require(Integer(0x84e0) == 0x84c0);

    GLint parameter = -99;
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2702);

    glActiveTexture_nid_postfix(0x84c3);
    Error(0);
    Require(Integer(0x84e0) == 0x84c3);

    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2600);

    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(1, &texture);
}

static void TextureUnitIsolationTests() {
    GLuint textures[3]{};
    glGenTextures_nid_postfix(3, textures);
    Require(textures[0] && textures[1] && textures[2] && textures[0] != textures[1] && textures[1] != textures[2]);

    glActiveTexture_nid_postfix(0x84c0);
    glBindTexture_nid_postfix(0x0de1, textures[0]);
    glTexParameteri_nid_postfix(0x0de1, 0x2801, 0x2600);
    glTexParameteri_nid_postfix(0x0de1, 0x2802, 0x2901);
    glTexParameteri_nid_postfix(0x0de1, 0x2803, 0x812f);

    glActiveTexture_nid_postfix(0x84c3);
    glBindTexture_nid_postfix(0x0de1, textures[1]);
    glTexParameteri_nid_postfix(0x0de1, 0x2801, 0x2601);
    glTexParameteri_nid_postfix(0x0de1, 0x2802, 0x812f);
    glTexParameteri_nid_postfix(0x0de1, 0x2803, 0x2901);

    glActiveTexture_nid_postfix(0x84c7);
    glBindTexture_nid_postfix(0x0de1, textures[2]);
    glTexParameteri_nid_postfix(0x0de1, 0x2801, 0x2703);
    glTexParameteri_nid_postfix(0x0de1, 0x2802, 0x8370);
    glTexParameteri_nid_postfix(0x0de1, 0x2803, 0x8370);

    GLint parameter = -99;

    glActiveTexture_nid_postfix(0x84c0);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2600);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x2901);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2803, &parameter);
    Require(parameter == 0x812f);

    glActiveTexture_nid_postfix(0x84c3);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2601);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x812f);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2803, &parameter);
    Require(parameter == 0x2901);

    glActiveTexture_nid_postfix(0x84c7);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2703);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x8370);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2803, &parameter);
    Require(parameter == 0x8370);

    glActiveTexture_nid_postfix(0x84c0);
    glBindTexture_nid_postfix(0x0de1, textures[1]);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2601);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x812f);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2803, &parameter);
    Require(parameter == 0x2901);

    glActiveTexture_nid_postfix(0x84c3);
    glBindTexture_nid_postfix(0x0de1, 0);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2702);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x2901);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2803, &parameter);
    Require(parameter == 0x2901);

    glBindTexture_nid_postfix(0x0de1, 0);
    glActiveTexture_nid_postfix(0x84c7);
    glBindTexture_nid_postfix(0x0de1, 0);
    glActiveTexture_nid_postfix(0x84c0);
    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(3, textures);
}

static void ParameterValidationTests() {
    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);

    glTexParameteri_nid_postfix(0x0de0, 0x2801, 0x2601);
    Error(0x0500);
    glTexParameteri_nid_postfix(0x8514, 0x2801, 0x2601);
    Error(0x0500);

    GLint parameter = -99;
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2702);

    glTexParameteri_nid_postfix(0x0de1, 0x2800, 0x2602);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameteri_nid_postfix(0x0de1, 0x2801, 0x2704);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2702);

    glTexParameteri_nid_postfix(0x0de1, 0x2802, 0x2902);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x2901);

    glTexParameteri_nid_postfix(0x0de1, 0x1234, 0x2601);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2801, &parameter);
    Require(parameter == 0x2702);

    const GLint wrapS[]{0x2902};
    glTexParameteriv_nid_postfix(0x0de1, 0x2802, wrapS);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2802, &parameter);
    Require(parameter == 0x2901);

    const GLfloat wrapT[]{33649.0f};
    glTexParameterfv_nid_postfix(0x0de1, 0x2803, wrapT);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2803, &parameter);
    Require(parameter == 0x2901);

    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(1, &texture);
}

static void TexParameterfValidationTests() {
    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);

    GLint parameter = -99;

    glTexParameterf_nid_postfix(0x0de1, 0x2800, 9728.5f);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, std::numeric_limits<GLfloat>::quiet_NaN());
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, std::numeric_limits<GLfloat>::infinity());
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, -std::numeric_limits<GLfloat>::infinity());
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, -1.0f);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, 65536.0f);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    const GLfloat fractional[]{9728.5f};
    glTexParameterfv_nid_postfix(0x0de1, 0x2800, fractional);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    const GLint minification[]{0x2700};
    glTexParameteriv_nid_postfix(0x0de1, 0x2800, minification);
    Error(0x0500);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, 9728.0f);
    Error(0);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2600);

    glTexParameterf_nid_postfix(0x0de1, 0x2800, 9729.0f);
    Error(0);
    glGetTexParameteriv_nid_postfix(0x0de1, 0x2800, &parameter);
    Require(parameter == 0x2601);

    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(1, &texture);
}

static void GetTexParameterUnknownTests() {
    GLuint texture;
    glGenTextures_nid_postfix(1, &texture);
    glBindTexture_nid_postfix(0x0de1, texture);

    GLint untouched = 55;
    glGetTexParameteriv_nid_postfix(0x0de1, 0x0500, &untouched);
    Error(0x0500);
    Require(untouched == 55);

    GLfloat untouchedFloat = 99.0f;
    glGetTexParameterfv_nid_postfix(0x0de1, 0x0500, &untouchedFloat);
    Error(0x0500);
    Require(untouchedFloat == 99.0f);

    glBindTexture_nid_postfix(0x0de1, 0);
    glDeleteTextures_nid_postfix(1, &texture);
}

int main() {
    EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    EGLint major, minor;
    Require(eglInitialize_nid_postfix(display, &major, &minor));
    EGLConfig config{};
    EGLint count = 0;
    const EGLint criteria[]{0x3033, 1, 0x3040, 4, 0x3038};
    Require(eglChooseConfig_nid_postfix(display, criteria, &config, 1, &count) && count == 1);
    const EGLint contextAttributes[]{0x3098, 2, 0x3038};
    const EGLint surfaceAttributes[]{0x3057, 4, 0x3056, 3, 0x3038};
    EGLContext context = eglCreateContext_nid_postfix(display, config, nullptr, contextAttributes);
    EGLSurface surface = eglCreatePbufferSurface_nid_postfix(display, config, surfaceAttributes);
    Require(context && surface);
    Require(eglMakeCurrent_nid_postfix(display, surface, surface, context));

    ActiveTextureTests();
    TextureUnitIsolationTests();
    ParameterValidationTests();
    TexParameterfValidationTests();
    GetTexParameterUnknownTests();

    Require(eglMakeCurrent_nid_postfix(display, nullptr, nullptr, nullptr));
    Require(eglDestroyContext_nid_postfix(display, context));
    Require(eglDestroySurface_nid_postfix(display, surface));
    Require(eglTerminate_nid_postfix(display));
    std::puts("GLSlim texture units tests passed");
}
