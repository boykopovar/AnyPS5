#include "prx/libScePigletv2VSH/include/Egl.hpp"

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
void APS5_VABI sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixelFormat, uint32_t tilingMode, uint32_t width, uint32_t height, uint64_t option, uint32_t dccControl, uint64_t dccCbRegisterClearColor);
int APS5_VABI sceVideoOutRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const VideoOutBuffers* buffers, int bufferNum, const VideoOutBufferAttribute2* attribute, int category, void* option);
int APS5_VABI sceVideoOutSubmitFlip(int handle, int index, int flipMode, int64_t flipArg);
int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status);
}

namespace Piglet {

namespace {

constexpr std::uint64_t PixelFormatR8G8B8A8 = 0x8000000022000000ull;
constexpr int UserServiceUserIdSystem = 255;
constexpr int VideoOutBusTypeMain = 0;
constexpr int VideoOutTilingLinear = 1;
constexpr int VideoOutFlipModeVsync = 1;
constexpr int VideoOutFlipModeHsync = 2;
constexpr std::size_t DisplayAlignment = 65536;

thread_local EGLint lastError = EGL_SUCCESS;
std::mutex objectsMutex;
bool initialized = false;
int displayObject = 0;
int configObject = 0;
std::set<WindowSurface*> surfaces;
std::set<ContextObject*> contexts;
thread_local ContextObject* currentContext = nullptr;
EGLint defaultInterval = 1;

EGLBoolean Fail(EGLint code) {
    lastError = code;
    return 0;
}

void* FailHandle(EGLint code) {
    lastError = code;
    return nullptr;
}

EGLBoolean Succeed() {
    lastError = EGL_SUCCESS;
    return 1;
}

bool ValidDisplay(void* display) {
    if (display != &displayObject) return Fail(EGL_BAD_DISPLAY), false;
    if (!initialized) return Fail(EGL_NOT_INITIALIZED), false;
    return true;
}

void DestroySurfaceNow(WindowSurface* window) {
    auto& device = Device::Get();
    if (window->surface.framebuffer) device.vkDestroyFramebuffer(device.device, window->surface.framebuffer, nullptr);
    device.DestroyImage(window->surface.color);
    device.DestroyBuffer(window->surface.readback);
    if (window->videoOut >= 0) sceVideoOutClose(window->videoOut);
    surfaces.erase(window);
    delete window;
}

}

void* EglGetDisplay(void* native) {
    static_cast<void>(native);
    lastError = EGL_SUCCESS;
    return &displayObject;
}

EGLBoolean EglInitialize(void* display, EGLint* major, EGLint* minor) {
    if (display != &displayObject) return Fail(EGL_BAD_DISPLAY);
    initialized = true;
    if (major) *major = 1;
    if (minor) *minor = 4;
    return Succeed();
}

EGLBoolean EglTerminate(void* display) {
    if (display != &displayObject) return Fail(EGL_BAD_DISPLAY);
    initialized = false;
    return Succeed();
}

EGLBoolean EglBindAPI(EGLenum api) {
    if (api != EGL_OPENGL_ES_API) return Fail(EGL_BAD_PARAMETER);
    return Succeed();
}

EGLBoolean EglChooseConfig(void* display, const EGLint* attributes, void** configs, EGLint size, EGLint* count) {
    if (!ValidDisplay(display)) return 0;
    if (!count) return Fail(EGL_BAD_PARAMETER);
    bool matches = true;
    for (const EGLint* attribute = attributes; attribute && *attribute != EGL_NONE; attribute += 2) {
        const EGLint value = attribute[1];
        switch (attribute[0]) {
            case EGL_RED_SIZE:
            case EGL_GREEN_SIZE:
            case EGL_BLUE_SIZE:
            case EGL_ALPHA_SIZE: matches = matches && value <= 8; break;
            case EGL_DEPTH_SIZE:
            case EGL_STENCIL_SIZE:
            case EGL_SAMPLE_BUFFERS:
            case EGL_SAMPLES: matches = matches && value <= 0; break;
            case EGL_RENDERABLE_TYPE: matches = matches && (value & ~EGL_OPENGL_ES2_BIT) == 0; break;
            case EGL_SURFACE_TYPE: matches = matches && (value & ~EGL_WINDOW_BIT) == 0; break;
            default: throw std::runtime_error("eglChooseConfig: attribute " + std::to_string(attribute[0]) + " is not implemented");
        }
    }
    *count = matches ? 1 : 0;
    if (configs && matches && size > 0) configs[0] = &configObject;
    return Succeed();
}

void* EglCreateWindowSurface(void* display, void* config, const NativeWindow* window, const EGLint* attributes) {
    if (!ValidDisplay(display)) return nullptr;
    if (config != &configObject) return FailHandle(EGL_BAD_CONFIG);
    if (!window || window->width == 0 || window->height == 0 || window->width > 16384 || window->height > 16384) return FailHandle(EGL_BAD_NATIVE_WINDOW);
    for (const EGLint* attribute = attributes; attribute && *attribute != EGL_NONE; attribute += 2) {
        if (attribute[0] != EGL_RENDER_BUFFER) throw std::runtime_error("eglCreateWindowSurface: attribute " + std::to_string(attribute[0]) + " is not implemented");
        if (attribute[1] != EGL_BACK_BUFFER) throw std::runtime_error("eglCreateWindowSurface: single-buffered surfaces are not implemented");
    }
    auto& device = Device::Get();
    auto created = std::make_unique<WindowSurface>();
    created->interval = defaultInterval;
    auto& surface = created->surface;
    surface.width = window->width;
    surface.height = window->height;
    surface.color = device.CreateImage(surface.width, surface.height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(surface.width) * surface.height * 4;
    surface.readback = device.CreateBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer.renderPass = ColorRenderPass();
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &surface.color.view;
    framebuffer.width = surface.width;
    framebuffer.height = surface.height;
    framebuffer.layers = 1;
    Check(device.vkCreateFramebuffer(device.device, &framebuffer, nullptr, &surface.framebuffer), "vkCreateFramebuffer");

    created->videoOut = sceVideoOutOpen(UserServiceUserIdSystem, VideoOutBusTypeMain, 0, nullptr);
    if (created->videoOut < 0) throw std::runtime_error("eglCreateWindowSurface: sceVideoOutOpen failed");
    std::array<VideoOutBuffers, 2> buffers{};
    for (std::size_t index = 0; index < 2; ++index) {
        created->storage[index].resize(static_cast<std::size_t>(bytes) + DisplayAlignment);
        const auto address = reinterpret_cast<std::uintptr_t>(created->storage[index].data());
        created->buffers[index] = reinterpret_cast<std::byte*>((address + DisplayAlignment - 1) / DisplayAlignment * DisplayAlignment);
        buffers[index].data = created->buffers[index];
    }
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, PixelFormatR8G8B8A8, VideoOutTilingLinear, surface.width, surface.height, 0, 0, 0);
    if (sceVideoOutRegisterBuffers2(created->videoOut, 0, 0, buffers.data(), 2, &attribute, 0, nullptr) < 0) throw std::runtime_error("eglCreateWindowSurface: sceVideoOutRegisterBuffers2 failed");
    std::lock_guard lock(objectsMutex);
    auto* result = created.release();
    surfaces.insert(result);
    lastError = EGL_SUCCESS;
    return result;
}

void* EglCreateContext(void* display, void* config, void* share, const EGLint* attributes) {
    if (!ValidDisplay(display)) return nullptr;
    if (config != &configObject) return FailHandle(EGL_BAD_CONFIG);
    if (share) throw std::runtime_error("eglCreateContext: shared contexts are not implemented");
    EGLint version = 1;
    for (const EGLint* attribute = attributes; attribute && *attribute != EGL_NONE; attribute += 2) {
        if (attribute[0] != EGL_CONTEXT_CLIENT_VERSION) return FailHandle(EGL_BAD_ATTRIBUTE);
        version = attribute[1];
    }
    if (version != 2) return FailHandle(EGL_BAD_MATCH);
    auto* created = new ContextObject();
    std::lock_guard lock(objectsMutex);
    contexts.insert(created);
    lastError = EGL_SUCCESS;
    return created;
}

EGLBoolean EglMakeCurrent(void* display, void* draw, void* read, void* context) {
    if (!ValidDisplay(display)) return 0;
    std::lock_guard lock(objectsMutex);
    auto* target = static_cast<ContextObject*>(context);
    auto* surface = static_cast<WindowSurface*>(draw);
    if (target && !contexts.contains(target)) return Fail(EGL_BAD_CONTEXT);
    if ((surface && !surfaces.contains(surface)) || draw != read) return Fail(EGL_BAD_SURFACE);
    if (target && !surface) return Fail(EGL_BAD_MATCH);
    if (target && target->current && target != currentContext) return Fail(EGL_BAD_ACCESS);
    if (currentContext) {
        auto* previous = currentContext;
        previous->current = false;
        auto* previousSurface = previous->draw;
        if (previousSurface) previousSurface->current = false;
        if (previous->destroyPending && previous != target) {
            contexts.erase(previous);
            delete previous;
        }
        if (previousSurface && previousSurface->destroyPending && previousSurface != surface) DestroySurfaceNow(previousSurface);
    }
    currentContext = target;
    if (target) {
        target->current = true;
        target->draw = surface;
        surface->current = true;
        target->context.SetSurface(&surface->surface);
        MakeCurrent(&target->context);
    } else {
        MakeCurrent(nullptr);
    }
    return Succeed();
}

EGLBoolean EglSwapInterval(void* display, EGLint interval) {
    if (!ValidDisplay(display)) return 0;
    const EGLint clamped = std::clamp(interval, 0, 1);
    if (currentContext && currentContext->draw) currentContext->draw->interval = clamped;
    else defaultInterval = clamped;
    return Succeed();
}

EGLBoolean EglSwapBuffers(void* display, void* surface) {
    if (!ValidDisplay(display)) return 0;
    auto* window = static_cast<WindowSurface*>(surface);
    {
        std::lock_guard lock(objectsMutex);
        if (!surfaces.contains(window)) return Fail(EGL_BAD_SURFACE);
    }
    if (!currentContext || currentContext->draw != window) return Fail(EGL_BAD_SURFACE);
    currentContext->context.Finish();
    VideoOutFlipStatus status{};
    while (true) {
        if (sceVideoOutGetFlipStatus(window->videoOut, &status) < 0) throw std::runtime_error("eglSwapBuffers: sceVideoOutGetFlipStatus failed");
        if (status.flipPendingNum == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::memcpy(window->buffers[window->next], window->surface.readback.mapped, static_cast<std::size_t>(window->surface.readback.size));
    if (sceVideoOutSubmitFlip(window->videoOut, window->next, window->interval == 0 ? VideoOutFlipModeHsync : VideoOutFlipModeVsync, ++window->frame) < 0) throw std::runtime_error("eglSwapBuffers: sceVideoOutSubmitFlip failed");
    window->next ^= 1;
    return Succeed();
}

EGLBoolean EglDestroySurface(void* display, void* surface) {
    if (!ValidDisplay(display)) return 0;
    std::lock_guard lock(objectsMutex);
    auto* window = static_cast<WindowSurface*>(surface);
    if (!surfaces.contains(window)) return Fail(EGL_BAD_SURFACE);
    if (window->current) window->destroyPending = true;
    else DestroySurfaceNow(window);
    return Succeed();
}

EGLBoolean EglDestroyContext(void* display, void* context) {
    if (!ValidDisplay(display)) return 0;
    std::lock_guard lock(objectsMutex);
    auto* target = static_cast<ContextObject*>(context);
    if (!contexts.contains(target)) return Fail(EGL_BAD_CONTEXT);
    if (target->current) {
        target->destroyPending = true;
        return Succeed();
    }
    contexts.erase(target);
    delete target;
    return Succeed();
}

EGLint EglGetError() {
    const EGLint result = lastError;
    lastError = EGL_SUCCESS;
    return result;
}

bool SetConfiguration(const PigletConfiguration* configuration) {
    return configuration && configuration->size == 0x88;
}

}
