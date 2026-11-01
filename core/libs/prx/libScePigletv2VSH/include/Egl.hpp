#ifndef CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_EGL_HPP
#define CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_EGL_HPP

#include "prx/libScePigletv2VSH/include/Context.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Piglet {

struct NativeWindow {
    std::uint32_t uniqueId;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t reserved;
};

struct PigletConfiguration {
    std::uint32_t size;
    std::uint32_t flags;
};

struct WindowSurface {
    WindowSurface() = default;
    WindowSurface(const WindowSurface&) = delete;
    WindowSurface& operator=(const WindowSurface&) = delete;
    ~WindowSurface();

    Surface surface;
    int videoOut = -1;
    std::array<std::vector<std::byte>, 2> storage;
    std::array<std::byte*, 2> buffers{};
    int next = 0;
    std::int64_t frame = 0;
    EGLint interval = 1;
    bool current = false;
    bool destroyPending = false;
};

struct ContextObject {
    Context context;
    WindowSurface* draw = nullptr;
    bool current = false;
    bool destroyPending = false;
};

void* EglGetDisplay(void* native);
EGLBoolean EglInitialize(void* display, EGLint* major, EGLint* minor);
EGLBoolean EglTerminate(void* display);
EGLBoolean EglBindAPI(EGLenum api);
EGLBoolean EglChooseConfig(void* display, const EGLint* attributes, void** configs, EGLint size, EGLint* count);
void* EglCreateWindowSurface(void* display, void* config, const NativeWindow* window, const EGLint* attributes);
void* EglCreateContext(void* display, void* config, void* share, const EGLint* attributes);
EGLBoolean EglMakeCurrent(void* display, void* draw, void* read, void* context);
EGLBoolean EglSwapInterval(void* display, EGLint interval);
EGLBoolean EglSwapBuffers(void* display, void* surface);
EGLBoolean EglDestroySurface(void* display, void* surface);
EGLBoolean EglDestroyContext(void* display, void* context);
EGLint EglGetError();
bool SetConfiguration(const PigletConfiguration* configuration);

}

#endif
