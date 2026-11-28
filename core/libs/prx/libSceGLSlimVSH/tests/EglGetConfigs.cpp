#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <cstdint>
#include <cstdlib>

int main() {
    const EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    if (eglInitialize_nid_postfix(display, nullptr, nullptr) != 1) std::abort();

    EGLConfig* configs = new EGLConfig[4];
    EGLint count = -1;
    if (eglGetConfigs_nid_postfix(display, configs, 4, &count) != 1 || count != 1 || configs[0] == nullptr) std::abort();
    delete[] configs;

    EGLConfig* single = new EGLConfig[1];
    if (eglGetConfigs_nid_postfix(display, single, 1, &count) != 1 || count != 1 || single[0] == nullptr) std::abort();
    delete[] single;

    EGLint hostCount = -7;
    if (eglGetConfigs_nid_postfix(display, nullptr, 0, &hostCount) != 1 || hostCount != 1) std::abort();

    if (eglGetConfigs_nid_postfix(display, nullptr, 0, nullptr) != 0 || eglGetError_nid_postfix() != 0x300c) std::abort();
    EGLConfig* negative = new EGLConfig[1];
    if (eglGetConfigs_nid_postfix(display, negative, -1, &hostCount) != 0 || eglGetError_nid_postfix() != 0x300c) std::abort();
    delete[] negative;

    const EGLint* badCount = reinterpret_cast<EGLint*>(0x10);
    if (eglGetConfigs_nid_postfix(display, nullptr, 0, const_cast<EGLint*>(badCount)) != 0 || eglGetError_nid_postfix() != 0x300c) std::abort();

    EGLConfig* badConfigs = reinterpret_cast<EGLConfig*>(0x10);
    if (eglGetConfigs_nid_postfix(display, badConfigs, 4, &hostCount) != 0 || eglGetError_nid_postfix() != 0x300c) std::abort();

    const EGLint* highCount = reinterpret_cast<EGLint*>(~std::uintptr_t(0xfff));
    if (eglGetConfigs_nid_postfix(display, nullptr, 0, const_cast<EGLint*>(highCount)) == 1 || eglGetError_nid_postfix() != 0x300c) std::abort();

    return 0;
}
