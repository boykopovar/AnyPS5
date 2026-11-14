#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GLSlim eglQuerySurface check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

int main() {
    auto display = eglGetDisplay_nid_postfix(nullptr);
    Require(display != nullptr);
    Require(eglInitialize_nid_postfix(display, nullptr, nullptr) == 1);
    EGLConfig config;
    EGLint count = -1;
    Require(eglGetConfigs_nid_postfix(display, &config, 1, &count) == 1);
    const EGLint attributes[] = {0x3057, 4, 0x3056, 2, 0x3038};
    auto surface = eglCreatePbufferSurface_nid_postfix(display, config, attributes);
    Require(surface != nullptr);

    EGLint value = -1;
    Require(eglQuerySurface_nid_postfix(display, surface, 0x3057, &value) == 1);
    Require(value == 4);
    Require(eglGetError_nid_postfix() == 0x3000);

    value = -1;
    Require(eglQuerySurface_nid_postfix(display, surface, 0x3028, nullptr) == 0);
    Require(eglGetError_nid_postfix() == 0x300c);

#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
#else
    void* page = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    Require(page != nullptr);
    value = -1;
    Require(eglQuerySurface_nid_postfix(display, surface, 0x3057, static_cast<EGLint*>(page)) == 0);
    Require(eglGetError_nid_postfix() == 0x300c);

    auto unmapped = reinterpret_cast<EGLint*>(1);
    value = -1;
    Require(eglQuerySurface_nid_postfix(display, surface, 0x3057, unmapped) == 0);
    Require(eglGetError_nid_postfix() == 0x300c);

#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, 4096);
#endif
    return 0;
}
