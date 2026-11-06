#include "State.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

namespace Slim {

std::recursive_mutex mutex;
struct ThreadTag;

ThreadState& Thread() {
    return HostThreadLocal<ThreadState, ThreadTag>();
}
std::map<EGLContext, std::shared_ptr<Context>> contexts;
std::map<EGLSurface, std::shared_ptr<Surface>> surfaces;
bool initialized = false;
int displayToken;
int configToken;

ThreadState::~ThreadState() {
    std::lock_guard lock(mutex);
    if (context) context->bound = false;
    context.reset(); draw.reset(); read.reset();
    std::erase_if(contexts, [](const auto& item) { return item.second->deleted && item.second.use_count() == 1; });
    std::erase_if(surfaces, [](const auto& item) { return item.second->deleted && item.second.use_count() == 1; });
}

float Clamp_nid_no_patch(float value) { return std::clamp(value, 0.0f, 1.0f); }

void Image::Allocate(GLint w, GLint h, GLenum f) {
    Image next;
    next.width = w;
    next.height = h;
    next.format = f;
    auto size = static_cast<std::size_t>(w) * h;
    if (f == 0x81a5) next.depth.resize(size);
    else if (f == 0x8d48) next.stencil.resize(size);
    else next.color.resize(size);
    *this = std::move(next);
}

Image* Attachment::Get() {
    if (renderbuffer) return &renderbuffer->image;
    if (texture) {
        auto it = texture->images.find({face, level});
        if (it != texture->images.end()) return &it->second;
    }
    return nullptr;
}

Context::Context() : state{
    {0x0b21, {1}}, {0x0b44, {0}}, {0x0b45, {0x0405}}, {0x0b46, {0x0901}},
    {0x0b70, {0, 1}}, {0x0b72, {1}}, {0x0b73, {1}}, {0x0b74, {0x0201}},
    {0x0b91, {0}}, {0x0b92, {0x0207}}, {0x0b93, {4294967295.0}},
    {0x0b94, {0x1e00}}, {0x0b95, {0x1e00}}, {0x0b96, {0x1e00}},
    {0x0b97, {0}}, {0x0b98, {4294967295.0}},
    {0x8800, {0x0207}}, {0x8801, {0x1e00}}, {0x8802, {0x1e00}}, {0x8803, {0x1e00}},
    {0x8ca3, {0}}, {0x8ca4, {4294967295.0}}, {0x8ca5, {4294967295.0}},
    {0x0ba2, {0, 0, 0, 0}}, {0x0c10, {0, 0, 0, 0}},
    {0x0c22, {0, 0, 0, 0}}, {0x0c23, {1, 1, 1, 1}},
    {0x8005, {0, 0, 0, 0}}, {0x8009, {0x8006}}, {0x883d, {0x8006}},
    {0x80c8, {0}}, {0x80c9, {1}}, {0x80ca, {0}}, {0x80cb, {1}},
    {0x8038, {0}}, {0x2a00, {0}}, {0x80aa, {1}}, {0x80ab, {0}},
    {0x0d05, {4}}, {0x0cf5, {4}}, {0x8192, {0x1100}}
} {
    for (unsigned i = 0; i < 2; ++i) {
        defaultTextures[i] = std::make_shared<Texture>();
        defaultTextures[i]->target = i ? 0x8513 : 0x0de1;
    }
}

Context& Current_nid_no_patch() {
    if (!Thread().context) NotImplemented_nid_no_patch(__func__);
    return *Thread().context;
}

void Error_nid_no_patch(GLenum error) {
    auto& context = Current_nid_no_patch();
    if (!context.error) context.error = error;
}

EGLBoolean EglError_nid_no_patch(EGLint error) { Thread().error = error; return 0; }

bool Display_nid_no_patch(EGLDisplay display) {
    if (display != &displayToken) return EglError_nid_no_patch(0x3008);
    if (!initialized) return EglError_nid_no_patch(0x3001);
    return true;
}

bool Config_nid_no_patch(EGLConfig config) {
    if (config != &configToken) return EglError_nid_no_patch(0x3005);
    return true;
}

bool Capability_nid_no_patch(GLenum cap) {
    switch (cap) {
    case 0x0b44: case 0x0be2: case 0x0bd0: case 0x0b71: case 0x0b90:
    case 0x0c11: case 0x8037: case 0x809e: case 0x80a0: return true;
    default: return false;
    }
}

GLenum Complete_nid_no_patch(Context& context) {
    if (!context.framebuffer) return Thread().draw ? 0x8cd5 : 0x8cd7;
    GLint width = -1, height = -1;
    bool any = false;
    for (unsigned i = 0; i < 3; ++i) {
        auto& attachment = context.framebuffer->attachments[i];
        if (!attachment.texture && !attachment.renderbuffer) continue;
        any = true;
        auto* image = attachment.Get();
        if (!image || !image->width || !image->height) return 0x8cd6;
        if (i == 0 && image->format != 0x1907 && image->format != 0x1908 && image->format != 0x8056 && image->format != 0x8057 && image->format != 0x8d62) return 0x8cd6;
        if (i == 1 && image->format != 0x81a5) return 0x8cd6;
        if (i == 2 && image->format != 0x8d48) return 0x8cd6;
        if (width >= 0 && (width != image->width || height != image->height)) return 0x8cd9;
        width = image->width;
        height = image->height;
    }
    return any ? 0x8cd5 : 0x8cd7;
}

Image* Color_nid_no_patch(Context& context, bool read) {
    if (context.framebuffer) return context.framebuffer->attachments[0].Get();
    auto surface = read ? Thread().read : Thread().draw;
    return surface ? &surface->image : nullptr;
}

bool Query_nid_no_patch(GLenum name, std::vector<double>& result) {
    auto& c = Current_nid_no_patch();
    if (Capability_nid_no_patch(name)) { result = {double(c.enabled.contains(name))}; return true; }
    if (name == 0x0b97 || name == 0x8ca3) {
        Image* image = c.framebuffer ? c.framebuffer->attachments[2].Get() : Thread().draw ? &Thread().draw->image : nullptr;
        double maximum = image && (!c.framebuffer || image->format == 0x8d48) ? 255 : 0;
        result = {std::clamp(c.state.at(name)[0], 0.0, maximum)};
        return true;
    }
    if (auto it = c.state.find(name); it != c.state.end()) { result = it->second; return true; }
    switch (name) {
    case 0x8894: result = {double(c.arrayBuffer ? c.arrayBuffer->name : 0)}; break;
    case 0x8895: result = {double(c.elementBuffer ? c.elementBuffer->name : 0)}; break;
    case 0x8ca6: result = {double(c.framebuffer ? c.framebuffer->name : 0)}; break;
    case 0x8ca7: result = {double(c.renderbuffer ? c.renderbuffer->name : 0)}; break;
    case 0x84e0: result = {double(0x84c0 + c.activeTexture)}; break;
    case 0x8069: case 0x8514: {
        auto t = c.textures[c.activeTexture][name == 0x8514];
        result = {double(t ? t->name : 0)}; break;
    }
    case 0x8b8d: result = {0}; break;
    case 0x0d33: case 0x851c: case 0x84e8: result = {2048}; break;
    case 0x0d3a: result = {2048, 2048}; break;
    case 0x8869: case 0x8872: case 0x8b4d: result = {8}; break;
    case 0x8b4c: result = {0}; break;
    case 0x8dfb: result = {128}; break;
    case 0x8dfc: result = {8}; break;
    case 0x8dfd: result = {16}; break;
    case 0x846d: case 0x846e: result = {1, 1}; break;
    case 0x0d50: result = {4}; break;
    case 0x80a8: case 0x80a9: case 0x86a2: case 0x8df9: result = {0}; break;
    case 0x86a3: case 0x8df8: result.clear(); break;
    case 0x8dfa: NotImplemented_nid_no_patch(__func__);
    case 0x8b9a: result = {0x1401}; break;
    case 0x8b9b: result = {0x1908}; break;
    case 0x0d52: case 0x0d53: case 0x0d54: case 0x0d55: case 0x0d56: case 0x0d57: {
        Image* image = nullptr;
        unsigned index = name < 0x0d56 ? 0 : name - 0x0d55;
        if (c.framebuffer) image = c.framebuffer->attachments[index].Get();
        else if (Thread().draw) image = &Thread().draw->image;
        GLint bits = 0;
        if (image) {
            if (index == 1) bits = !c.framebuffer || image->format == 0x81a5 ? 16 : 0;
            else if (index == 2) bits = !c.framebuffer || image->format == 0x8d48 ? 8 : 0;
            else if (image->format && image->format != 0x81a5 && image->format != 0x8d48) {
                bits = 8;
                if (image->format == 0x8056) bits = 4;
                if (image->format == 0x8057) bits = name == 0x0d55 ? 1 : 5;
                if (image->format == 0x8d62) bits = name == 0x0d55 ? 0 : name == 0x0d53 ? 6 : 5;
                if (image->format == 0x1907 && name == 0x0d55) bits = 0;
            }
        }
        result = {double(bits)}; break;
    }
    default: Error_nid_no_patch(0x0500); return false;
    }
    return true;
}

}
