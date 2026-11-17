#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static char* MapPageBeforeHole() {
#ifdef _WIN32
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD old = 0;
    Require(pages != nullptr && VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &old));
#else
    void* mapped = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<char*>(mapped);
    Require(mprotect(pages + 4096, 4096, PROT_NONE) == 0);
#endif
    return pages;
}

template <std::size_t Count>
static const EGLint* AtPageEnd(char* pages, const EGLint (&words)[Count]) {
    auto* list = reinterpret_cast<EGLint*>(pages + 4096) - Count;
    std::memcpy(list, words, sizeof(words));
    return list;
}

int main() {
    const EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    Require(eglInitialize_nid_postfix(display, nullptr, nullptr) == 1);
    char* pages = MapPageBeforeHole();
    const auto* unmapped = reinterpret_cast<const EGLint*>(0x10);

    EGLConfig config = nullptr;
    EGLint count = -1;
    const EGLint criteria[]{0x3033, 1, 0x3040, 4, 0x3038};
    Require(eglChooseConfig_nid_postfix(display, AtPageEnd(pages, criteria), &config, 1, &count) == 1);
    Require(count == 1 && config != nullptr);
    Require(eglGetError_nid_postfix() == 0x3000);
    Require(eglChooseConfig_nid_postfix(display, unmapped, &config, 1, &count) == 0);
    Require(eglGetError_nid_postfix() == 0x300c);
    const EGLint keyOnly[]{0x3033};
    Require(eglChooseConfig_nid_postfix(display, AtPageEnd(pages, keyOnly), &config, 1, &count) == 0);
    Require(eglGetError_nid_postfix() == 0x300c);

    const EGLint contextAttributes[]{0x3098, 2, 0x3038};
    const EGLContext context = eglCreateContext_nid_postfix(display, config, nullptr, AtPageEnd(pages, contextAttributes));
    Require(context != nullptr);
    Require(eglGetError_nid_postfix() == 0x3000);
    Require(eglCreateContext_nid_postfix(display, config, nullptr, unmapped) == nullptr);
    Require(eglGetError_nid_postfix() == 0x300c);
    const EGLint versionKey[]{0x3098};
    Require(eglCreateContext_nid_postfix(display, config, nullptr, AtPageEnd(pages, versionKey)) == nullptr);
    Require(eglGetError_nid_postfix() == 0x300c);

    const EGLint surfaceAttributes[]{0x3057, 4, 0x3056, 2, 0x3038};
    const EGLSurface surface = eglCreatePbufferSurface_nid_postfix(display, config, AtPageEnd(pages, surfaceAttributes));
    Require(surface != nullptr);
    Require(eglGetError_nid_postfix() == 0x3000);
    EGLint width = -1;
    Require(eglQuerySurface_nid_postfix(display, surface, 0x3057, &width) == 1 && width == 4);
    Require(eglCreatePbufferSurface_nid_postfix(display, config, unmapped) == nullptr);
    Require(eglGetError_nid_postfix() == 0x300c);
    const EGLint widthKey[]{0x3057};
    Require(eglCreatePbufferSurface_nid_postfix(display, config, AtPageEnd(pages, widthKey)) == nullptr);
    Require(eglGetError_nid_postfix() == 0x300c);

    Require(eglDestroySurface_nid_postfix(display, surface) == 1);
    Require(eglDestroyContext_nid_postfix(display, context) == 1);
#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, 8192);
#endif
    return 0;
}
