#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GLSlim viewport check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static void Error(GLenum expected) {
    Require(glGetError_nid_postfix() == expected);
    Require(glGetError_nid_postfix() == 0);
}

template<std::size_t Count>
static std::array<GLint, Count> Integers(GLenum name) {
    std::array<GLint, Count> values{};
    glGetIntegerv_nid_postfix(name, values.data());
    return values;
}

template<std::size_t Count>
static std::array<GLfloat, Count> Floats(GLenum name) {
    std::array<GLfloat, Count> values{};
    glGetFloatv_nid_postfix(name, values.data());
    return values;
}

static EGLint SurfaceValue(EGLDisplay display, EGLSurface surface, EGLint attribute) {
    EGLint value = -99;
    Require(eglQuerySurface_nid_postfix(display, surface, attribute, &value));
    return value;
}

int main() {
    EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    Require(display);

    EGLint major = 0, minor = 0;
    Require(eglInitialize_nid_postfix(display, &major, &minor));
    Require(major == 1 && minor == 4);

    Require(eglBindAPI_nid_postfix(0x30a0));

    const EGLint criteria[]{0x3033, 1, 0x3040, 4, 0x3038};
    EGLConfig config = nullptr;
    EGLint count = 0;
    Require(eglChooseConfig_nid_postfix(display, criteria, &config, 1, &count));
    Require(count == 1 && config);

    const EGLint surfaceAttributes[]{0x3057, 4, 0x3056, 3, 0x3038};
    EGLSurface surface = eglCreatePbufferSurface_nid_postfix(display, config, surfaceAttributes);
    Require(surface);
    Require(SurfaceValue(display, surface, 0x3057) == 4);
    Require(SurfaceValue(display, surface, 0x3056) == 3);

    const EGLint contextAttributes[]{0x3098, 2, 0x3038};
    EGLContext context = eglCreateContext_nid_postfix(display, config, nullptr, contextAttributes);
    Require(context);

    Require(eglMakeCurrent_nid_postfix(display, surface, surface, context));

    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{0, 0, 4, 3}));
    Require((Floats<4>(0x0ba2) == std::array<GLfloat, 4>{0, 0, 4, 3}));

    glViewport_nid_postfix(-15, -27, 640, 480);
    Error(0);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{-15, -27, 640, 480}));
    Require((Floats<4>(0x0ba2) == std::array<GLfloat, 4>{-15, -27, 640, 480}));

    glViewport_nid_postfix(0, 0, -1, 100);
    Error(0x0501);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{-15, -27, 640, 480}));

    glViewport_nid_postfix(0, 0, 100, -1);
    Error(0x0501);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{-15, -27, 640, 480}));

    glViewport_nid_postfix(0, 0, -5, -10);
    Error(0x0501);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{-15, -27, 640, 480}));

    glViewport_nid_postfix(10, -20, 2049, 4096);
    Error(0);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{10, -20, 2048, 2048}));
    Require((Floats<4>(0x0ba2) == std::array<GLfloat, 4>{10, -20, 2048, 2048}));

    glViewport_nid_postfix(0, 0, 2048, 2048);
    Error(0);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{0, 0, 2048, 2048}));
    Require((Floats<4>(0x0ba2) == std::array<GLfloat, 4>{0, 0, 2048, 2048}));

    glViewport_nid_postfix(0, 0, 5000, 3000);
    Error(0);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{0, 0, 2048, 2048}));
    Require((Floats<4>(0x0ba2) == std::array<GLfloat, 4>{0, 0, 2048, 2048}));

    glViewport_nid_postfix(-500, 600, 0, 0);
    Error(0);
    Require((Integers<4>(0x0ba2) == std::array<GLint, 4>{-500, 600, 0, 0}));
    Require((Floats<4>(0x0ba2) == std::array<GLfloat, 4>{-500, 600, 0, 0}));

    Require(eglMakeCurrent_nid_postfix(display, nullptr, nullptr, nullptr));
    Require(eglDestroyContext_nid_postfix(display, context));
    Require(eglDestroySurface_nid_postfix(display, surface));
    Require(eglTerminate_nid_postfix(display));

    return 0;
}
