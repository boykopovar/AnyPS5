#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GLSlim swap interval check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static void EglError(EGLint expected) {
    Require(eglGetError_nid_postfix() == expected);
    Require(eglGetError_nid_postfix() == 0x3000);
}

static void SwapIntervalErrors() {
    EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    Require(display);

    Require(!eglSwapInterval_nid_postfix(nullptr, 0));
    EglError(0x3008);

    int token = 0;
    Require(!eglSwapInterval_nid_postfix(&token, 0));
    EglError(0x3008);

    Require(!eglSwapInterval_nid_postfix(display, 0));
    EglError(0x3001);

    Require(eglInitialize_nid_postfix(display, nullptr, nullptr));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(!eglSwapInterval_nid_postfix(display, 0));
    EglError(0x3006);

    const EGLint configAttribs[]{0x3033, 1, 0x3040, 4, 0x3038};
    EGLint count = 0;
    EGLConfig config = nullptr;
    Require(eglChooseConfig_nid_postfix(display, configAttribs, &config, 1, &count));
    Require(config);

    const EGLint contextAttribs[]{0x3098, 2, 0x3038};
    EGLContext context = eglCreateContext_nid_postfix(display, config, nullptr, contextAttribs);
    Require(context);

    const EGLint surfaceAttribs[]{0x3057, 4, 0x3056, 3, 0x3038};
    EGLSurface surface = eglCreatePbufferSurface_nid_postfix(display, config, surfaceAttribs);
    Require(surface);

    Require(!eglSwapInterval_nid_postfix(display, 0));
    EglError(0x3006);

    Require(!eglMakeCurrent_nid_postfix(display, nullptr, nullptr, context));
    EglError(0x3009);

    Require(eglMakeCurrent_nid_postfix(display, surface, surface, context));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglSwapInterval_nid_postfix(display, 0));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglSwapInterval_nid_postfix(display, 1));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglSwapInterval_nid_postfix(display, 2));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglSwapInterval_nid_postfix(display, -1));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglMakeCurrent_nid_postfix(display, nullptr, nullptr, nullptr));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglDestroySurface_nid_postfix(display, surface));
    Require(eglDestroyContext_nid_postfix(display, context));
    Require(eglTerminate_nid_postfix(display));
    Require(eglGetError_nid_postfix() == 0x3000);
}

static void BindApiQueryApiGetError() {
    EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    Require(display);

    Require(eglInitialize_nid_postfix(display, nullptr, nullptr));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglBindAPI_nid_postfix(0x30a0));
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(eglQueryAPI_nid_postfix() == 0x30a0);
    Require(eglGetError_nid_postfix() == 0x3000);

    Require(!eglBindAPI_nid_postfix(0x30a1));
    EglError(0x300c);
    Require(eglQueryAPI_nid_postfix() == 0x30a0);

    Require(!eglBindAPI_nid_postfix(0x30a2));
    EglError(0x300c);
    Require(eglQueryAPI_nid_postfix() == 0x30a0);

    Require(!eglBindAPI_nid_postfix(0));
    EglError(0x300c);
    Require(eglQueryAPI_nid_postfix() == 0x30a0);

    Require(eglTerminate_nid_postfix(display));
    Require(eglGetError_nid_postfix() == 0x3000);
}

int main() {
    SwapIntervalErrors();
    BindApiQueryApiGetError();
    return 0;
}
