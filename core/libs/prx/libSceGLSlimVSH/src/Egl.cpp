#include "State.hpp"

using namespace Slim;

namespace {

const std::map<EGLint, EGLint> configAttributes{
    {0x3020, 32}, {0x3021, 8}, {0x3022, 8}, {0x3023, 8}, {0x3024, 8},
    {0x3025, 16}, {0x3026, 8}, {0x3027, 0x3051}, {0x3028, 1},
    {0x3029, 0}, {0x302a, 2048}, {0x302b, 4194304}, {0x302c, 2048},
    {0x302d, 0}, {0x302e, 0}, {0x302f, 0}, {0x3031, 0}, {0x3032, 0},
    {0x3033, 1}, {0x3034, 0x3038}, {0x3035, 0}, {0x3036, 0}, {0x3037, 0},
    {0x3039, 0}, {0x303a, 0}, {0x303b, 0}, {0x303c, 0},
    {0x303d, 0}, {0x303e, 0}, {0x303f, 0x308e}, {0x3040, 4}, {0x3042, 0}
};

std::shared_ptr<Surface> SurfaceFor(EGLSurface handle) {
    auto it = surfaces.find(handle);
    if (it == surfaces.end() || it->second->deleted) { EglError_nid_no_patch(0x300d); return {}; }
    return it->second;
}

std::shared_ptr<Context> ContextFor(EGLContext handle) {
    auto it = contexts.find(handle);
    if (it == contexts.end() || it->second->deleted) { EglError_nid_no_patch(0x3006); return {}; }
    return it->second;
}

void Collect() {
    std::erase_if(contexts, [](const auto& item) { return item.second->deleted && item.second.use_count() == 1; });
    std::erase_if(surfaces, [](const auto& item) { return item.second->deleted && item.second.use_count() == 1; });
}

}

extern "C" {

EGLDisplay APS5_VABI eglGetDisplay_nid_postfix(void* nativeDisplay) {
    if (nativeDisplay) return nullptr;
    return &displayToken;
}

EGLint APS5_VABI eglGetError_nid_postfix() {
    EGLint result = Thread().error;
    Thread().error = 0x3000;
    return result;
}

EGLBoolean APS5_VABI eglInitialize_nid_postfix(EGLDisplay display, EGLint* major, EGLint* minor) {
    std::lock_guard lock(mutex);
    if (display != &displayToken) return EglError_nid_no_patch(0x3008);
    initialized = true;
    if (major) *major = 1;
    if (minor) *minor = 4;
    return 1;
}

EGLBoolean APS5_VABI eglTerminate_nid_postfix(EGLDisplay display) {
    std::lock_guard lock(mutex);
    if (display != &displayToken) return EglError_nid_no_patch(0x3008);
    initialized = false;
    for (auto& item : contexts) item.second->deleted = true;
    for (auto& item : surfaces) item.second->deleted = true;
    Collect();
    return 1;
}

EGLBoolean APS5_VABI eglBindAPI_nid_postfix(EGLenum api) {
    if (api != 0x30a0) return EglError_nid_no_patch(0x300c);
    return 1;
}

EGLenum APS5_VABI eglQueryAPI_nid_postfix() { return 0x30a0; }

EGLBoolean APS5_VABI eglGetConfigs_nid_postfix(EGLDisplay display, EGLConfig* configs, EGLint size, EGLint* count) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    if (!count || (configs && size < 0)) return EglError_nid_no_patch(0x300c);
    *count = configs ? std::min(size, 1) : 1;
    if (configs && size) configs[0] = &configToken;
    return 1;
}

EGLBoolean APS5_VABI eglGetConfigAttrib_nid_postfix(EGLDisplay display, EGLConfig config, EGLint attribute, EGLint* value) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display) || !Config_nid_no_patch(config)) return 0;
    auto it = configAttributes.find(attribute);
    if (it == configAttributes.end()) return EglError_nid_no_patch(0x3004);
    if (!value) return EglError_nid_no_patch(0x300c);
    *value = it->second;
    return 1;
}

EGLBoolean APS5_VABI eglChooseConfig_nid_postfix(EGLDisplay display, const EGLint* attributes, EGLConfig* configs, EGLint size, EGLint* count) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    if (!count || (configs && size < 0)) return EglError_nid_no_patch(0x300c);
    std::map<EGLint, EGLint> criteria{{0x3033, 4}, {0x3040, 1}, {0x303f, 0x308e}, {0x3034, 0x3038}, {0x3029, 0}};
    if (attributes) {
        for (auto p = attributes; *p != 0x3038; p += 2) {
            if (!configAttributes.contains(p[0]) && p[0] != 0x3041) return EglError_nid_no_patch(0x3004);
            if (p[1] < 0 && p[1] != -1 && p[0] != 0x3029 && p[0] != 0x3041) return EglError_nid_no_patch(0x3004);
            if (p[0] == 0x3027 && p[1] != -1 && p[1] != 0x3038 && p[1] != 0x3050 && p[1] != 0x3051) return EglError_nid_no_patch(0x3004);
            if (p[0] == 0x3034 && p[1] != -1 && p[1] != 0x3038 && p[1] != 0x3052) return EglError_nid_no_patch(0x3004);
            if (p[0] == 0x303f && p[1] != -1 && p[1] != 0x308e && p[1] != 0x308f) return EglError_nid_no_patch(0x3004);
            if ((p[0] == 0x3039 || p[0] == 0x303a || p[0] == 0x302d) && p[1] != -1 && p[1] > 1) return EglError_nid_no_patch(0x3004);
            criteria[p[0]] = p[1];
        }
    }
    bool match = true;
    if (criteria.contains(0x3028) && criteria[0x3028] != -1) match = criteria[0x3028] == 1;
    else for (auto [key, value] : criteria) {
        if ((value == -1 && key != 0x3029 && key != 0x3041) || key == 0x302a || key == 0x302b || key == 0x302c || key == 0x302e || key == 0x302f) continue;
        if (key == 0x3041) { if (value != 0x3038) match = false; continue; }
        EGLint actual = configAttributes.at(key);
        if (key == 0x3033 || key == 0x3040 || key == 0x3042) match &= (actual & value) == value;
        else if ((key >= 0x3020 && key <= 0x3026) || key == 0x3031 || key == 0x3032 || key == 0x303d || key == 0x303e) match &= actual >= value;
        else if (key != 0x3035 && key != 0x3036 && key != 0x3037) match &= actual == value;
    }
    *count = match && (!configs || size > 0) ? 1 : 0;
    if (configs && *count) configs[0] = &configToken;
    return 1;
}

EGLContext APS5_VABI eglCreateContext_nid_postfix(EGLDisplay display, EGLConfig config, EGLContext shared, const EGLint* attributes) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display) || !Config_nid_no_patch(config)) return nullptr;
    EGLint version = 1;
    if (attributes) for (auto p = attributes; *p != 0x3038; p += 2) {
        if (*p != 0x3098) { EglError_nid_no_patch(0x3004); return nullptr; }
        version = p[1];
    }
    if (version != 2) { EglError_nid_no_patch(0x3005); return nullptr; }
    std::shared_ptr<Context> other;
    if (shared && !(other = ContextFor(shared))) return nullptr;
    try {
        auto context = std::make_shared<Context>();
        if (other) context->share = other->share;
        auto handle = context.get();
        contexts.emplace(handle, std::move(context));
        return handle;
    } catch (const std::bad_alloc&) { EglError_nid_no_patch(0x3003); return nullptr; }
}

EGLSurface APS5_VABI eglCreatePbufferSurface_nid_postfix(EGLDisplay display, EGLConfig config, const EGLint* attributes) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display) || !Config_nid_no_patch(config)) return nullptr;
    EGLint width = 0, height = 0, largest = 0, textureFormat = 0x305c, textureTarget = 0x305c, mipmap = 0;
    EGLint colorspace = 0x3089, alphaFormat = 0x308b;
    if (attributes) for (auto p = attributes; *p != 0x3038; p += 2) {
        switch (*p) {
        case 0x3057: width = p[1]; break;
        case 0x3056: height = p[1]; break;
        case 0x3058: largest = p[1]; break;
        case 0x3080: textureFormat = p[1]; break;
        case 0x3081: textureTarget = p[1]; break;
        case 0x3082: mipmap = p[1]; break;
        case 0x3087: colorspace = p[1]; break;
        case 0x3088: alphaFormat = p[1]; break;
        default: EglError_nid_no_patch(0x3004); return nullptr;
        }
    }
    if (width < 0 || height < 0) { EglError_nid_no_patch(0x300c); return nullptr; }
    if ((colorspace != 0x3089 && colorspace != 0x308a) || (alphaFormat != 0x308b && alphaFormat != 0x308c) ||
        (largest != 0 && largest != 1) || (mipmap != 0 && mipmap != 1) ||
        (textureFormat != 0x305c && textureFormat != 0x305d && textureFormat != 0x305e) ||
        (textureTarget != 0x305c && textureTarget != 0x305f)) { EglError_nid_no_patch(0x3004); return nullptr; }
    if (textureFormat != 0x305c || textureTarget != 0x305c) { EglError_nid_no_patch(0x3009); return nullptr; }
    if (!largest && (width > 2048 || height > 2048)) { EglError_nid_no_patch(0x3003); return nullptr; }
    try {
        auto surface = std::make_shared<Surface>();
        surface->largest = largest;
        surface->mipmap = mipmap;
        surface->colorspace = colorspace;
        surface->alphaFormat = alphaFormat;
        surface->image.Allocate(std::min(width, 2048), std::min(height, 2048), 0x1908);
        surface->image.depth.resize(surface->image.color.size());
        surface->image.stencil.resize(surface->image.color.size());
        auto handle = surface.get();
        surfaces.emplace(handle, std::move(surface));
        return handle;
    } catch (const std::bad_alloc&) { EglError_nid_no_patch(0x3003); return nullptr; }
}

EGLBoolean APS5_VABI eglDestroyContext_nid_postfix(EGLDisplay display, EGLContext handle) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    auto context = ContextFor(handle);
    if (!context) return 0;
    context->deleted = true;
    context.reset();
    Collect();
    return 1;
}

EGLBoolean APS5_VABI eglDestroySurface_nid_postfix(EGLDisplay display, EGLSurface handle) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    auto surface = SurfaceFor(handle);
    if (!surface) return 0;
    surface->deleted = true;
    surface.reset();
    Collect();
    return 1;
}

EGLBoolean APS5_VABI eglMakeCurrent_nid_postfix(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext handle) {
    std::lock_guard lock(mutex);
    if (!handle && !draw && !read) {
        if (display != &displayToken) return EglError_nid_no_patch(0x3008);
        if (Thread().context) Thread().context->bound = false;
        Thread().context.reset(); Thread().draw.reset(); Thread().read.reset();
        Collect();
        return 1;
    }
    if (!Display_nid_no_patch(display)) return 0;
    if (!handle || !draw || !read) return EglError_nid_no_patch(0x3009);
    auto context = ContextFor(handle);
    if (!context) return 0;
    if (context != Thread().context && context->bound) return EglError_nid_no_patch(0x3002);
    auto drawSurface = SurfaceFor(draw);
    if (!drawSurface) return 0;
    if (drawSurface != Thread().draw && drawSurface != Thread().read && drawSurface.use_count() > 2) return EglError_nid_no_patch(0x3002);
    auto readSurface = SurfaceFor(read);
    if (!readSurface) return 0;
    if (readSurface != drawSurface && readSurface != Thread().draw && readSurface != Thread().read && readSurface.use_count() > 2) return EglError_nid_no_patch(0x3002);
    if (!context->initializedViewport) {
        context->state[0x0ba2] = {0, 0, double(drawSurface->image.width), double(drawSurface->image.height)};
        context->state[0x0c10] = context->state[0x0ba2];
        context->initializedViewport = true;
    }
    if (Thread().context) Thread().context->bound = false;
    context->bound = true;
    Thread().context = std::move(context);
    Thread().draw = std::move(drawSurface);
    Thread().read = std::move(readSurface);
    Collect();
    return 1;
}

EGLContext APS5_VABI eglGetCurrentContext_nid_postfix() { return Thread().context.get(); }
EGLDisplay APS5_VABI eglGetCurrentDisplay_nid_postfix() { return Thread().context ? &displayToken : nullptr; }

EGLSurface APS5_VABI eglGetCurrentSurface_nid_postfix(EGLint which) {
    if (which == 0x3059) return Thread().draw.get();
    if (which == 0x305a) return Thread().read.get();
    EglError_nid_no_patch(0x300c);
    return nullptr;
}

EGLBoolean APS5_VABI eglReleaseThread_nid_postfix() {
    std::lock_guard lock(mutex);
    if (Thread().context) Thread().context->bound = false;
    Thread().context.reset(); Thread().draw.reset(); Thread().read.reset();
    Thread().error = 0x3000;
    Collect();
    return 1;
}

EGLBoolean APS5_VABI eglQueryContext_nid_postfix(EGLDisplay display, EGLContext handle, EGLint attribute, EGLint* value) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    auto context = ContextFor(handle);
    if (!context) return 0;
    if (!value) return EglError_nid_no_patch(0x300c);
    switch (attribute) {
    case 0x3028: *value = 1; break;
    case 0x3097: *value = 0x30a0; break;
    case 0x3098: *value = 2; break;
    case 0x3086: *value = context->bound ? 0x3084 : 0x3038; break;
    default: return EglError_nid_no_patch(0x3004);
    }
    return 1;
}

EGLBoolean APS5_VABI eglQuerySurface_nid_postfix(EGLDisplay display, EGLSurface handle, EGLint attribute, EGLint* value) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    auto surface = SurfaceFor(handle);
    if (!surface) return 0;
    if (!value) return EglError_nid_no_patch(0x300c);
    switch (attribute) {
    case 0x3028: *value = 1; break;
    case 0x3057: *value = surface->image.width; break;
    case 0x3056: *value = surface->image.height; break;
    case 0x3058: *value = surface->largest; break;
    case 0x3080: case 0x3081: *value = 0x305c; break;
    case 0x3082: *value = surface->mipmap; break;
    case 0x3083: *value = surface->mipmapLevel; break;
    case 0x3086: *value = 0x3084; break;
    case 0x3087: *value = surface->colorspace; break;
    case 0x3088: *value = surface->alphaFormat; break;
    case 0x3090: case 0x3091: case 0x3092: *value = -1; break;
    case 0x3093: *value = 0x3094; break;
    case 0x3099: *value = 0x309a; break;
    default: return EglError_nid_no_patch(0x3004);
    }
    return 1;
}

EGLBoolean APS5_VABI eglSurfaceAttrib_nid_postfix(EGLDisplay display, EGLSurface handle, EGLint attribute, EGLint value) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    auto surface = SurfaceFor(handle);
    if (!surface) return 0;
    switch (attribute) {
    case 0x3083: surface->mipmapLevel = value; return 1;
    case 0x3099:
        if (value == 0x309a) return 1;
        if (value == 0x309b) return EglError_nid_no_patch(0x3009);
        return EglError_nid_no_patch(0x3004);
    case 0x3093:
        if (value == 0x3094) return 1;
        if (value == 0x3095) return EglError_nid_no_patch(0x3009);
        return EglError_nid_no_patch(0x3004);
    default: return EglError_nid_no_patch(0x3004);
    }
}

EGLBoolean APS5_VABI eglSwapBuffers_nid_postfix(EGLDisplay display, EGLSurface surface) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    auto object = SurfaceFor(surface);
    if (!object) return 0;
    if (object != Thread().draw) return EglError_nid_no_patch(0x300d);
    return 1;
}

EGLBoolean APS5_VABI eglSwapInterval_nid_postfix(EGLDisplay display, EGLint interval) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return 0;
    if (!Thread().context) return EglError_nid_no_patch(0x3006);
    if (!Thread().draw) return EglError_nid_no_patch(0x300d);
    return 1;
}

EGLBoolean APS5_VABI eglWaitClient_nid_postfix() { return 1; }
EGLBoolean APS5_VABI eglWaitGL_nid_postfix() { return 1; }
EGLBoolean APS5_VABI eglWaitNative_nid_postfix(EGLint engine) {
    if (engine != 0x305b) return EglError_nid_no_patch(0x300c);
    return 1;
}

EGLBoolean APS5_VABI eglBindTexImage_nid_postfix(EGLDisplay display, EGLSurface handle, EGLint buffer) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display) || !SurfaceFor(handle)) return 0;
    if (buffer != 0x3084) return EglError_nid_no_patch(0x300c);
    return EglError_nid_no_patch(0x3009);
}

EGLBoolean APS5_VABI eglReleaseTexImage_nid_postfix(EGLDisplay display, EGLSurface handle, EGLint buffer) {
    return eglBindTexImage_nid_postfix(display, handle, buffer);
}

const char* APS5_VABI eglQueryString_nid_postfix(EGLDisplay display, EGLint name) {
    std::lock_guard lock(mutex);
    if (!Display_nid_no_patch(display)) return nullptr;
    switch (name) {
    case 0x3053: return "AnyPS5";
    case 0x3054: return "1.4 AnyPS5";
    case 0x3055: return "";
    case 0x308d: return "OpenGL_ES";
    default: EglError_nid_no_patch(0x300c); return nullptr;
    }
}


__eglMustCastToProperFunctionPointerType APS5_VABI eglGetProcAddress_nid_postfix(const char* name) {
    if (!name) return nullptr;
    static const std::pair<const char*, __eglMustCastToProperFunctionPointerType> functions[]{
        {"eglBindAPI", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglBindAPI_nid_postfix)},
        {"eglBindTexImage", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglBindTexImage_nid_postfix)},
        {"eglChooseConfig", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglChooseConfig_nid_postfix)},
        {"eglCreateContext", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglCreateContext_nid_postfix)},
        {"eglCreatePbufferFromClientBuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglCreatePbufferFromClientBuffer_nid_postfix)},
        {"eglCreatePbufferSurface", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglCreatePbufferSurface_nid_postfix)},
        {"eglCreateWindowSurface", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglCreateWindowSurface_nid_postfix)},
        {"eglDestroyContext", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglDestroyContext_nid_postfix)},
        {"eglDestroySurface", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglDestroySurface_nid_postfix)},
        {"eglGetConfigAttrib", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetConfigAttrib_nid_postfix)},
        {"eglGetConfigs", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetConfigs_nid_postfix)},
        {"eglGetCurrentContext", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetCurrentContext_nid_postfix)},
        {"eglGetCurrentDisplay", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetCurrentDisplay_nid_postfix)},
        {"eglGetCurrentSurface", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetCurrentSurface_nid_postfix)},
        {"eglGetDisplay", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetDisplay_nid_postfix)},
        {"eglGetError", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetError_nid_postfix)},
        {"eglGetProcAddress", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglGetProcAddress_nid_postfix)},
        {"eglInitialize", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglInitialize_nid_postfix)},
        {"eglMakeCurrent", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglMakeCurrent_nid_postfix)},
        {"eglQueryAPI", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglQueryAPI_nid_postfix)},
        {"eglQueryContext", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglQueryContext_nid_postfix)},
        {"eglQueryString", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglQueryString_nid_postfix)},
        {"eglQuerySurface", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglQuerySurface_nid_postfix)},
        {"eglReleaseTexImage", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglReleaseTexImage_nid_postfix)},
        {"eglReleaseThread", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglReleaseThread_nid_postfix)},
        {"eglSurfaceAttrib", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglSurfaceAttrib_nid_postfix)},
        {"eglSwapBuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglSwapBuffers_nid_postfix)},
        {"eglSwapInterval", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglSwapInterval_nid_postfix)},
        {"eglTerminate", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglTerminate_nid_postfix)},
        {"eglWaitClient", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglWaitClient_nid_postfix)},
        {"eglWaitGL", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglWaitGL_nid_postfix)},
        {"eglWaitNative", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&eglWaitNative_nid_postfix)},
        {"glActiveTexture", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glActiveTexture_nid_postfix)},
        {"glAttachShader", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glAttachShader_nid_postfix)},
        {"glBindAttribLocation", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBindAttribLocation_nid_postfix)},
        {"glBindBuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBindBuffer_nid_postfix)},
        {"glBindFramebuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBindFramebuffer_nid_postfix)},
        {"glBindRenderbuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBindRenderbuffer_nid_postfix)},
        {"glBindTexture", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBindTexture_nid_postfix)},
        {"glBlendColor", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBlendColor_nid_postfix)},
        {"glBlendEquation", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBlendEquation_nid_postfix)},
        {"glBlendEquationSeparate", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBlendEquationSeparate_nid_postfix)},
        {"glBlendFunc", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBlendFunc_nid_postfix)},
        {"glBlendFuncSeparate", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBlendFuncSeparate_nid_postfix)},
        {"glBufferData", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBufferData_nid_postfix)},
        {"glBufferSubData", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glBufferSubData_nid_postfix)},
        {"glCheckFramebufferStatus", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCheckFramebufferStatus_nid_postfix)},
        {"glClear", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glClear_nid_postfix)},
        {"glClearColor", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glClearColor_nid_postfix)},
        {"glClearDepthf", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glClearDepthf_nid_postfix)},
        {"glClearStencil", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glClearStencil_nid_postfix)},
        {"glColorMask", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glColorMask_nid_postfix)},
        {"glCompileShader", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCompileShader_nid_postfix)},
        {"glCompressedTexImage2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCompressedTexImage2D_nid_postfix)},
        {"glCompressedTexSubImage2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCompressedTexSubImage2D_nid_postfix)},
        {"glCopyTexImage2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCopyTexImage2D_nid_postfix)},
        {"glCopyTexSubImage2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCopyTexSubImage2D_nid_postfix)},
        {"glCreateProgram", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCreateProgram_nid_postfix)},
        {"glCreateShader", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCreateShader_nid_postfix)},
        {"glCullFace", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glCullFace_nid_postfix)},
        {"glDeleteBuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDeleteBuffers_nid_postfix)},
        {"glDeleteFramebuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDeleteFramebuffers_nid_postfix)},
        {"glDeleteProgram", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDeleteProgram_nid_postfix)},
        {"glDeleteRenderbuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDeleteRenderbuffers_nid_postfix)},
        {"glDeleteShader", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDeleteShader_nid_postfix)},
        {"glDeleteTextures", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDeleteTextures_nid_postfix)},
        {"glDepthFunc", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDepthFunc_nid_postfix)},
        {"glDepthMask", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDepthMask_nid_postfix)},
        {"glDepthRangef", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDepthRangef_nid_postfix)},
        {"glDetachShader", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDetachShader_nid_postfix)},
        {"glDisable", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDisable_nid_postfix)},
        {"glDisableVertexAttribArray", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDisableVertexAttribArray_nid_postfix)},
        {"glDrawArrays", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDrawArrays_nid_postfix)},
        {"glDrawElements", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glDrawElements_nid_postfix)},
        {"glEnable", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glEnable_nid_postfix)},
        {"glEnableVertexAttribArray", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glEnableVertexAttribArray_nid_postfix)},
        {"glFinish", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glFinish_nid_postfix)},
        {"glFlush", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glFlush_nid_postfix)},
        {"glFramebufferRenderbuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glFramebufferRenderbuffer_nid_postfix)},
        {"glFramebufferTexture2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glFramebufferTexture2D_nid_postfix)},
        {"glFrontFace", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glFrontFace_nid_postfix)},
        {"glGenBuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGenBuffers_nid_postfix)},
        {"glGenFramebuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGenFramebuffers_nid_postfix)},
        {"glGenRenderbuffers", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGenRenderbuffers_nid_postfix)},
        {"glGenTextures", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGenTextures_nid_postfix)},
        {"glGenerateMipmap", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGenerateMipmap_nid_postfix)},
        {"glGetActiveAttrib", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetActiveAttrib_nid_postfix)},
        {"glGetActiveUniform", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetActiveUniform_nid_postfix)},
        {"glGetAttachedShaders", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetAttachedShaders_nid_postfix)},
        {"glGetAttribLocation", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetAttribLocation_nid_postfix)},
        {"glGetBooleanv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetBooleanv_nid_postfix)},
        {"glGetBufferParameteriv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetBufferParameteriv_nid_postfix)},
        {"glGetError", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetError_nid_postfix)},
        {"glGetFloatv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetFloatv_nid_postfix)},
        {"glGetFramebufferAttachmentParameteriv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetFramebufferAttachmentParameteriv_nid_postfix)},
        {"glGetIntegerv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetIntegerv_nid_postfix)},
        {"glGetProgramInfoLog", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetProgramInfoLog_nid_postfix)},
        {"glGetProgramiv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetProgramiv_nid_postfix)},
        {"glGetRenderbufferParameteriv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetRenderbufferParameteriv_nid_postfix)},
        {"glGetShaderInfoLog", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetShaderInfoLog_nid_postfix)},
        {"glGetShaderPrecisionFormat", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetShaderPrecisionFormat_nid_postfix)},
        {"glGetShaderiv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetShaderiv_nid_postfix)},
        {"glGetString", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetString_nid_postfix)},
        {"glGetTexParameterfv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetTexParameterfv_nid_postfix)},
        {"glGetTexParameteriv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetTexParameteriv_nid_postfix)},
        {"glGetUniformLocation", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetUniformLocation_nid_postfix)},
        {"glGetUniformfv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetUniformfv_nid_postfix)},
        {"glGetUniformiv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetUniformiv_nid_postfix)},
        {"glGetVertexAttribPointerv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetVertexAttribPointerv_nid_postfix)},
        {"glGetVertexAttribfv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetVertexAttribfv_nid_postfix)},
        {"glGetVertexAttribiv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glGetVertexAttribiv_nid_postfix)},
        {"glHint", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glHint_nid_postfix)},
        {"glIsBuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsBuffer_nid_postfix)},
        {"glIsEnabled", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsEnabled_nid_postfix)},
        {"glIsFramebuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsFramebuffer_nid_postfix)},
        {"glIsProgram", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsProgram_nid_postfix)},
        {"glIsRenderbuffer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsRenderbuffer_nid_postfix)},
        {"glIsShader", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsShader_nid_postfix)},
        {"glIsTexture", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glIsTexture_nid_postfix)},
        {"glLineWidth", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glLineWidth_nid_postfix)},
        {"glLinkProgram", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glLinkProgram_nid_postfix)},
        {"glPixelStorei", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glPixelStorei_nid_postfix)},
        {"glPolygonOffset", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glPolygonOffset_nid_postfix)},
        {"glReadPixels", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glReadPixels_nid_postfix)},
        {"glReleaseShaderCompiler", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glReleaseShaderCompiler_nid_postfix)},
        {"glRenderbufferStorage", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glRenderbufferStorage_nid_postfix)},
        {"glSampleCoverage", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glSampleCoverage_nid_postfix)},
        {"glScissor", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glScissor_nid_postfix)},
        {"glShaderBinary", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glShaderBinary_nid_postfix)},
        {"glShaderSource", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glShaderSource_nid_postfix)},
        {"glStencilFunc", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glStencilFunc_nid_postfix)},
        {"glStencilFuncSeparate", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glStencilFuncSeparate_nid_postfix)},
        {"glStencilMask", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glStencilMask_nid_postfix)},
        {"glStencilMaskSeparate", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glStencilMaskSeparate_nid_postfix)},
        {"glStencilOp", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glStencilOp_nid_postfix)},
        {"glStencilOpSeparate", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glStencilOpSeparate_nid_postfix)},
        {"glTexImage2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glTexImage2D_nid_postfix)},
        {"glTexParameterf", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glTexParameterf_nid_postfix)},
        {"glTexParameterfv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glTexParameterfv_nid_postfix)},
        {"glTexParameteri", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glTexParameteri_nid_postfix)},
        {"glTexParameteriv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glTexParameteriv_nid_postfix)},
        {"glTexSubImage2D", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glTexSubImage2D_nid_postfix)},
        {"glUniform1f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform1f_nid_postfix)},
        {"glUniform1fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform1fv_nid_postfix)},
        {"glUniform1i", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform1i_nid_postfix)},
        {"glUniform1iv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform1iv_nid_postfix)},
        {"glUniform2f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform2f_nid_postfix)},
        {"glUniform2fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform2fv_nid_postfix)},
        {"glUniform2i", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform2i_nid_postfix)},
        {"glUniform2iv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform2iv_nid_postfix)},
        {"glUniform3f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform3f_nid_postfix)},
        {"glUniform3fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform3fv_nid_postfix)},
        {"glUniform3i", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform3i_nid_postfix)},
        {"glUniform3iv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform3iv_nid_postfix)},
        {"glUniform4f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform4f_nid_postfix)},
        {"glUniform4fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform4fv_nid_postfix)},
        {"glUniform4i", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform4i_nid_postfix)},
        {"glUniform4iv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniform4iv_nid_postfix)},
        {"glUniformMatrix2fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniformMatrix2fv_nid_postfix)},
        {"glUniformMatrix3fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniformMatrix3fv_nid_postfix)},
        {"glUniformMatrix4fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUniformMatrix4fv_nid_postfix)},
        {"glUseProgram", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glUseProgram_nid_postfix)},
        {"glValidateProgram", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glValidateProgram_nid_postfix)},
        {"glVertexAttrib1f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib1f_nid_postfix)},
        {"glVertexAttrib1fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib1fv_nid_postfix)},
        {"glVertexAttrib2f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib2f_nid_postfix)},
        {"glVertexAttrib2fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib2fv_nid_postfix)},
        {"glVertexAttrib3f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib3f_nid_postfix)},
        {"glVertexAttrib3fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib3fv_nid_postfix)},
        {"glVertexAttrib4f", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib4f_nid_postfix)},
        {"glVertexAttrib4fv", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttrib4fv_nid_postfix)},
        {"glVertexAttribPointer", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glVertexAttribPointer_nid_postfix)},
        {"glViewport", reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&glViewport_nid_postfix)},
    };
    for (auto& entry : functions) if (std::strcmp(entry.first, name) == 0) return entry.second;
    return nullptr;
}

}
