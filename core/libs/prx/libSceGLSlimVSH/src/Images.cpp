#include "State.hpp"

using namespace Slim;

namespace {

int Target(GLenum target) {
    if (target == 0x0de1) return 0;
    if (target == 0x8513) return 1;
    Error_nid_no_patch(0x0500);
    return -1;
}

int ImageTarget(GLenum target) {
    if (target == 0x0de1) return 0;
    if (target >= 0x8515 && target <= 0x851a) return 1;
    Error_nid_no_patch(0x0500);
    return -1;
}

std::shared_ptr<Texture> BoundTexture(int target) {
    auto& c = Current_nid_no_patch();
    auto texture = c.textures[c.activeTexture][target];
    return texture ? texture : c.defaultTextures[target];
}

bool Dimensions(int target, GLint level, GLsizei width, GLsizei height, GLint border) {
    if (level < 0 || level > 11 || width < 0 || height < 0 || border != 0 ||
        width > (2048 >> level) || height > (2048 >> level) || (target == 1 && width != height) ||
        (level > 0 && ((width & (width - 1)) || (height & (height - 1))))) {
        Error_nid_no_patch(0x0501); return false;
    }
    return true;
}

bool Format(GLenum format) { return format == 0x1906 || format == 0x1907 || format == 0x1908 || format == 0x1909 || format == 0x190a; }

int PixelSize(GLenum format, GLenum type) {
    if (!Format(format) || (type != 0x1401 && type != 0x8033 && type != 0x8034 && type != 0x8363)) { Error_nid_no_patch(0x0500); return 0; }
    if (type != 0x1401) {
        if ((type == 0x8363 && format != 0x1907) || (type != 0x8363 && format != 0x1908)) { Error_nid_no_patch(0x0502); return 0; }
        return 2;
    }
    return format == 0x1908 ? 4 : format == 0x1907 ? 3 : format == 0x190a ? 2 : 1;
}

unsigned char Expand(unsigned value, unsigned maximum) { return static_cast<unsigned char>((value * 255 + maximum / 2) / maximum); }

std::array<unsigned char, 4> Decode(const unsigned char* data, GLenum format, GLenum type) {
    if (type != 0x1401) {
        std::uint16_t value;
        std::memcpy(&value, data, 2);
        if (type == 0x8363) return {Expand(value >> 11, 31), Expand((value >> 5) & 63, 63), Expand(value & 31, 31), 255};
        if (type == 0x8033) return {Expand(value >> 12, 15), Expand((value >> 8) & 15, 15), Expand((value >> 4) & 15, 15), Expand(value & 15, 15)};
        return {Expand(value >> 11, 31), Expand((value >> 6) & 31, 31), Expand((value >> 1) & 31, 31), static_cast<unsigned char>((value & 1) * 255)};
    }
    switch (format) {
    case 0x1906: return {0, 0, 0, data[0]};
    case 0x1907: return {data[0], data[1], data[2], 255};
    case 0x1908: return {data[0], data[1], data[2], data[3]};
    case 0x1909: return {data[0], data[0], data[0], 255};
    default: return {data[0], data[0], data[0], data[1]};
    }
}

std::array<unsigned char, 4> Convert(std::array<unsigned char, 4> color, GLenum format) {
    if (format == 0x1906) color = {0, 0, 0, color[3]};
    if (format == 0x1909 || format == 0x190a) color = {color[0], color[0], color[0], color[3]};
    if (format == 0x1907 || format == 0x1909 || format == 0x8d62) color[3] = 255;
    if (format == 0x8056 || format == 0x8057 || format == 0x8d62) for (unsigned i = 0; i < 4; ++i) {
        unsigned maximum = format == 0x8056 ? 15 : i == 3 ? (format == 0x8057 ? 1 : 255) : (format == 0x8d62 && i == 1 ? 63 : 31);
        color[i] = Expand((unsigned(color[i]) * maximum + 127) / 255, maximum);
    }
    return color;
}

void Upload(Image& image, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels, int size) {
    if (!pixels || !width || !height) return;
    auto& c = Current_nid_no_patch();
    std::size_t alignment = static_cast<std::size_t>(c.state[0x0cf5][0]);
    std::size_t stride = (std::size_t(width) * size + alignment - 1) & ~(alignment - 1);
    auto* bytes = static_cast<const unsigned char*>(pixels);
    for (GLsizei row = 0; row < height; ++row) for (GLsizei col = 0; col < width; ++col)
        image.color[std::size_t(y + row) * image.width + x + col] = Decode(bytes + std::size_t(row) * stride + std::size_t(col) * size, format, type);
}

bool Parameter(GLenum name, GLint value) {
    switch (name) {
    case 0x2800: if (value == 0x2600 || value == 0x2601) return true; break;
    case 0x2801: if (value == 0x2600 || value == 0x2601 || (value >= 0x2700 && value <= 0x2703)) return true; break;
    case 0x2802: case 0x2803: if (value == 0x2901 || value == 0x812f || value == 0x8370) return true; break;
    default: break;
    }
    Error_nid_no_patch(0x0500);
    return false;
}

int AttachmentIndex(GLenum attachment) {
    if (attachment == 0x8ce0) return 0;
    if (attachment == 0x8d00) return 1;
    if (attachment == 0x8d20) return 2;
    Error_nid_no_patch(0x0500);
    return -1;
}

Attachment* BoundAttachment(GLenum target, GLenum attachment) {
    if (target != 0x8d40) { Error_nid_no_patch(0x0500); return nullptr; }
    int index = AttachmentIndex(attachment);
    if (index < 0) return nullptr;
    auto& c = Current_nid_no_patch();
    if (!c.framebuffer) { Error_nid_no_patch(0x0502); return nullptr; }
    return &c.framebuffer->attachments[index];
}

bool Readable(Context& c) {
    if (Complete_nid_no_patch(c) != 0x8cd5) { Error_nid_no_patch(0x0506); return false; }
    return true;
}

bool Region(Image& image, GLint level, GLint x, GLint y, GLsizei width, GLsizei height) {
    if (level < 0 || level > 11 || x < 0 || y < 0 || width < 0 || height < 0 ||
        x > image.width || y > image.height || width > image.width - x || height > image.height - y) {
        Error_nid_no_patch(0x0501); return false;
    }
    return true;
}

bool CopyFormat(GLenum destination, GLenum source) {
    bool sourceAlpha = source == 0x1908 || source == 0x8056 || source == 0x8057;
    if ((destination == 0x1908 || destination == 0x1906 || destination == 0x190a) && !sourceAlpha) {
        Error_nid_no_patch(0x0502); return false;
    }
    return true;
}

void Copy(Image& destination, GLint dx, GLint dy, Image& source, GLint sx, GLint sy, GLsizei width, GLsizei height) {
    for (GLsizei y = 0; y < height; ++y) for (GLsizei x = 0; x < width; ++x) {
        auto xx = std::int64_t(sx) + x, yy = std::int64_t(sy) + y;
        if (xx >= 0 && yy >= 0 && xx < source.width && yy < source.height)
            destination.color[std::size_t(dy + y) * destination.width + dx + x] = Convert(source.color[std::size_t(yy) * source.width + xx], destination.format);
    }
}

}

extern "C" {

void APS5_VABI glGenerateMipmap_nid_postfix(GLenum target) {
    std::lock_guard lock(mutex);
    int index = Target(target);
    if (index < 0) return;
    auto texture = BoundTexture(index);
    GLenum firstFace = index ? 0x8515 : 0x0de1;
    unsigned faces = index ? 6 : 1;
    GLint width = 0, height = 0;
    GLenum format = 0;
    for (unsigned face = 0; face < faces; ++face) {
        auto it = texture->images.find({firstFace + face, 0});
        if (it == texture->images.end()) { Error_nid_no_patch(0x0502); return; }
        auto& base = it->second;
        if (base.width <= 0 || base.height <= 0 || (base.width & (base.width - 1)) || (base.height & (base.height - 1)) ||
            (face && (base.width != width || base.height != height || base.format != format))) { Error_nid_no_patch(0x0502); return; }
        width = base.width; height = base.height; format = base.format;
    }
    try {
        auto images = texture->images;
        for (unsigned face = 0; face < faces; ++face) {
            GLint level = 0;
            while (images.at({firstFace + face, level}).width > 1 || images.at({firstFace + face, level}).height > 1) {
                const auto& source = images.at({firstFace + face, level});
                Image next;
                next.Allocate(std::max(source.width / 2, 1), std::max(source.height / 2, 1), format);
                unsigned nx = source.width > 1 ? 2 : 1, ny = source.height > 1 ? 2 : 1;
                for (GLint y = 0; y < next.height; ++y) for (GLint x = 0; x < next.width; ++x) {
                    std::array<unsigned char, 4> color{};
                    for (unsigned component = 0; component < 4; ++component) {
                        unsigned sum = 0;
                        for (unsigned dy = 0; dy < ny; ++dy) for (unsigned dx = 0; dx < nx; ++dx)
                            sum += source.color[std::size_t(y * ny + dy) * source.width + x * nx + dx][component];
                        color[component] = static_cast<unsigned char>((sum + nx * ny / 2) / (nx * ny));
                    }
                    next.color[std::size_t(y) * next.width + x] = Convert(color, format);
                }
                images[{firstFace + face, ++level}] = std::move(next);
            }
        }
        texture->images.swap(images);
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glCompressedTexImage2D_nid_postfix(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) {
    std::lock_guard lock(mutex);
    int index = ImageTarget(target);
    if (index < 0 || !Dimensions(index, level, width, height, border)) return;
    Error_nid_no_patch(0x0500);
}

void APS5_VABI glCompressedTexSubImage2D_nid_postfix(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    std::lock_guard lock(mutex);
    if (ImageTarget(target) < 0) return;
    Error_nid_no_patch(0x0500);
}

void APS5_VABI glActiveTexture_nid_postfix(GLenum texture) {
    std::lock_guard lock(mutex);
    if (texture < 0x84c0 || texture >= 0x84c8) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().activeTexture = texture - 0x84c0;
}

void APS5_VABI glGenTextures_nid_postfix(GLsizei count, GLuint* textures) {
    std::lock_guard lock(mutex);
    Generate_nid_no_patch(count, textures, Current_nid_no_patch().share->textures);
}

void APS5_VABI glBindTexture_nid_postfix(GLenum target, GLuint name) {
    std::lock_guard lock(mutex);
    int index = Target(target);
    if (index < 0) return;
    auto& c = Current_nid_no_patch();
    if (!name) { c.textures[c.activeTexture][index].reset(); return; }
    try {
        auto& object = c.share->textures[name];
        if (object && object->target != target) { Error_nid_no_patch(0x0502); return; }
        if (!object) { object = std::make_shared<Texture>(); object->name = name; object->target = target; }
        c.textures[c.activeTexture][index] = object;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

GLboolean APS5_VABI glIsTexture_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    auto& objects = Current_nid_no_patch().share->textures;
    auto it = objects.find(name);
    return it != objects.end() && bool(it->second);
}

void APS5_VABI glDeleteTextures_nid_postfix(GLsizei count, const GLuint* textures) {
    std::lock_guard lock(mutex);
    if (count < 0) { Error_nid_no_patch(0x0501); return; }
    auto& c = Current_nid_no_patch();
    for (GLsizei i = 0; i < count; ++i) {
        auto it = c.share->textures.find(textures[i]);
        if (it == c.share->textures.end()) continue;
        auto object = it->second;
        if (object) {
            for (auto& unit : c.textures) for (auto& binding : unit) if (binding == object) binding.reset();
            if (c.framebuffer) for (auto& attachment : c.framebuffer->attachments) if (attachment.texture == object) attachment = {};
        }
        c.share->textures.erase(it);
    }
}

void APS5_VABI glTexParameteri_nid_postfix(GLenum target, GLenum name, GLint value) {
    std::lock_guard lock(mutex);
    int index = Target(target);
    if (index < 0 || !Parameter(name, value)) return;
    BoundTexture(index)->parameters[name] = value;
}

void APS5_VABI glTexParameterf_nid_postfix(GLenum target, GLenum name, GLfloat value) {
    if (!std::isfinite(value) || value < 0 || value > 65535 || value != std::round(value)) {
        std::lock_guard lock(mutex); Error_nid_no_patch(0x0500); return;
    }
    glTexParameteri_nid_postfix(target, name, static_cast<GLint>(std::round(value)));
}

void APS5_VABI glTexParameteriv_nid_postfix(GLenum target, GLenum name, const GLint* values) { glTexParameteri_nid_postfix(target, name, *values); }
void APS5_VABI glTexParameterfv_nid_postfix(GLenum target, GLenum name, const GLfloat* values) { glTexParameterf_nid_postfix(target, name, *values); }

void APS5_VABI glGetTexParameteriv_nid_postfix(GLenum target, GLenum name, GLint* values) {
    std::lock_guard lock(mutex);
    int index = Target(target);
    if (index < 0) return;
    auto object = BoundTexture(index);
    auto it = object->parameters.find(name);
    if (it == object->parameters.end()) { Error_nid_no_patch(0x0500); return; }
    *values = it->second;
}

void APS5_VABI glGetTexParameterfv_nid_postfix(GLenum target, GLenum name, GLfloat* values) {
    std::lock_guard lock(mutex);
    int index = Target(target);
    if (index < 0) return;
    auto object = BoundTexture(index);
    auto it = object->parameters.find(name);
    if (it == object->parameters.end()) { Error_nid_no_patch(0x0500); return; }
    *values = static_cast<GLfloat>(it->second);
}

void APS5_VABI glTexImage2D_nid_postfix(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    std::lock_guard lock(mutex);
    int index = ImageTarget(target);
    if (index < 0 || !Dimensions(index, level, width, height, border)) return;
    int size = PixelSize(format, type);
    if (!size) return;
    if (!Format(internalFormat)) { Error_nid_no_patch(0x0501); return; }
    if (internalFormat != static_cast<GLint>(format)) { Error_nid_no_patch(0x0502); return; }
    try {
        Image image;
        image.Allocate(width, height, format);
        Upload(image, 0, 0, width, height, format, type, pixels, size);
        BoundTexture(index)->images[{target, level}] = std::move(image);
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glTexSubImage2D_nid_postfix(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    std::lock_guard lock(mutex);
    int index = ImageTarget(target);
    if (index < 0) return;
    if (level < 0 || level > 11) { Error_nid_no_patch(0x0501); return; }
    int size = PixelSize(format, type);
    if (!size) return;
    auto texture = BoundTexture(index);
    auto it = texture->images.find({target, level});
    if (it == texture->images.end()) { Error_nid_no_patch(0x0502); return; }
    if (!Region(it->second, level, x, y, width, height)) return;
    if (format != it->second.format) { Error_nid_no_patch(0x0502); return; }
    Upload(it->second, x, y, width, height, format, type, pixels, size);
}

void APS5_VABI glGenRenderbuffers_nid_postfix(GLsizei count, GLuint* names) {
    std::lock_guard lock(mutex);
    Generate_nid_no_patch(count, names, Current_nid_no_patch().share->renderbuffers);
}

void APS5_VABI glBindRenderbuffer_nid_postfix(GLenum target, GLuint name) {
    std::lock_guard lock(mutex);
    if (target != 0x8d41) { Error_nid_no_patch(0x0500); return; }
    auto& c = Current_nid_no_patch();
    if (!name) { c.renderbuffer.reset(); return; }
    try {
        auto& object = c.share->renderbuffers[name];
        if (!object) { object = std::make_shared<Renderbuffer>(); object->name = name; }
        c.renderbuffer = object;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

GLboolean APS5_VABI glIsRenderbuffer_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    auto& objects = Current_nid_no_patch().share->renderbuffers;
    auto it = objects.find(name);
    return it != objects.end() && bool(it->second);
}

void APS5_VABI glDeleteRenderbuffers_nid_postfix(GLsizei count, const GLuint* names) {
    std::lock_guard lock(mutex);
    if (count < 0) { Error_nid_no_patch(0x0501); return; }
    auto& c = Current_nid_no_patch();
    for (GLsizei i = 0; i < count; ++i) {
        auto it = c.share->renderbuffers.find(names[i]);
        if (it == c.share->renderbuffers.end()) continue;
        auto object = it->second;
        if (object) {
            if (c.renderbuffer == object) c.renderbuffer.reset();
            if (c.framebuffer) for (auto& a : c.framebuffer->attachments) if (a.renderbuffer == object) a = {};
        }
        c.share->renderbuffers.erase(it);
    }
}

void APS5_VABI glRenderbufferStorage_nid_postfix(GLenum target, GLenum format, GLsizei width, GLsizei height) {
    std::lock_guard lock(mutex);
    if (target != 0x8d41) { Error_nid_no_patch(0x0500); return; }
    if (format != 0x8056 && format != 0x8057 && format != 0x8d62 && format != 0x81a5 && format != 0x8d48) { Error_nid_no_patch(0x0500); return; }
    if (width < 0 || height < 0 || width > 2048 || height > 2048) { Error_nid_no_patch(0x0501); return; }
    auto object = Current_nid_no_patch().renderbuffer;
    if (!object) { Error_nid_no_patch(0x0502); return; }
    try { object->image.Allocate(width, height, format); }
    catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glGetRenderbufferParameteriv_nid_postfix(GLenum target, GLenum name, GLint* value) {
    std::lock_guard lock(mutex);
    if (target != 0x8d41) { Error_nid_no_patch(0x0500); return; }
    auto object = Current_nid_no_patch().renderbuffer;
    if (!object) { Error_nid_no_patch(0x0502); return; }
    const auto& image = object->image;
    if (name == 0x8d42) *value = image.width;
    else if (name == 0x8d43) *value = image.height;
    else if (name == 0x8d44) *value = image.format ? image.format : 0x8056;
    else if (name >= 0x8d50 && name <= 0x8d55) {
        GLint bits = 0;
        if (name <= 0x8d53) {
            if (image.format == 0x8056) bits = 4;
            if (image.format == 0x8057) bits = name == 0x8d53 ? 1 : 5;
            if (image.format == 0x8d62) bits = name == 0x8d53 ? 0 : name == 0x8d51 ? 6 : 5;
        } else if (name == 0x8d54 && image.format == 0x81a5) bits = 16;
        else if (name == 0x8d55 && image.format == 0x8d48) bits = 8;
        *value = bits;
    } else Error_nid_no_patch(0x0500);
}

void APS5_VABI glGenFramebuffers_nid_postfix(GLsizei count, GLuint* names) {
    std::lock_guard lock(mutex);
    Generate_nid_no_patch(count, names, Current_nid_no_patch().framebuffers);
}

void APS5_VABI glBindFramebuffer_nid_postfix(GLenum target, GLuint name) {
    std::lock_guard lock(mutex);
    if (target != 0x8d40) { Error_nid_no_patch(0x0500); return; }
    auto& c = Current_nid_no_patch();
    if (!name) { c.framebuffer.reset(); return; }
    try {
        auto& object = c.framebuffers[name];
        if (!object) { object = std::make_shared<Framebuffer>(); object->name = name; }
        c.framebuffer = object;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

GLboolean APS5_VABI glIsFramebuffer_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    auto& objects = Current_nid_no_patch().framebuffers;
    auto it = objects.find(name);
    return it != objects.end() && bool(it->second);
}

void APS5_VABI glDeleteFramebuffers_nid_postfix(GLsizei count, const GLuint* names) {
    std::lock_guard lock(mutex);
    if (count < 0) { Error_nid_no_patch(0x0501); return; }
    auto& c = Current_nid_no_patch();
    for (GLsizei i = 0; i < count; ++i) {
        auto it = c.framebuffers.find(names[i]);
        if (it == c.framebuffers.end()) continue;
        if (c.framebuffer == it->second) c.framebuffer.reset();
        c.framebuffers.erase(it);
    }
}

void APS5_VABI glFramebufferRenderbuffer_nid_postfix(GLenum target, GLenum attachment, GLenum renderbufferTarget, GLuint name) {
    std::lock_guard lock(mutex);
    auto* a = BoundAttachment(target, attachment);
    if (!a) return;
    if (renderbufferTarget != 0x8d41) { Error_nid_no_patch(0x0500); return; }
    if (!name) { *a = {}; return; }
    auto& objects = Current_nid_no_patch().share->renderbuffers;
    auto it = objects.find(name);
    if (it == objects.end() || !it->second) { Error_nid_no_patch(0x0502); return; }
    *a = {}; a->renderbuffer = it->second;
}

void APS5_VABI glFramebufferTexture2D_nid_postfix(GLenum target, GLenum attachment, GLenum textureTarget, GLuint name, GLint level) {
    std::lock_guard lock(mutex);
    auto* a = BoundAttachment(target, attachment);
    if (!a) return;
    if (!name) { *a = {}; return; }
    int index = ImageTarget(textureTarget);
    if (index < 0) return;
    if (level != 0) { Error_nid_no_patch(0x0501); return; }
    auto& objects = Current_nid_no_patch().share->textures;
    auto it = objects.find(name);
    if (it == objects.end() || !it->second || it->second->target != (index ? 0x8513u : 0x0de1u)) { Error_nid_no_patch(0x0502); return; }
    *a = {}; a->texture = it->second; a->face = textureTarget; a->level = level;
}

GLenum APS5_VABI glCheckFramebufferStatus_nid_postfix(GLenum target) {
    std::lock_guard lock(mutex);
    if (target != 0x8d40) { Error_nid_no_patch(0x0500); return 0; }
    return Complete_nid_no_patch(Current_nid_no_patch());
}

void APS5_VABI glGetFramebufferAttachmentParameteriv_nid_postfix(GLenum target, GLenum attachment, GLenum name, GLint* value) {
    std::lock_guard lock(mutex);
    auto* a = BoundAttachment(target, attachment);
    if (!a) return;
    if (name == 0x8cd0) *value = a->texture ? 0x1702 : a->renderbuffer ? 0x8d41 : 0;
    else if (name == 0x8cd1 && (a->texture || a->renderbuffer)) *value = a->texture ? a->texture->name : a->renderbuffer->name;
    else if (name == 0x8cd2 && a->texture) *value = a->level;
    else if (name == 0x8cd3 && a->texture) *value = a->face == 0x0de1 ? 0 : a->face;
    else Error_nid_no_patch(0x0500);
}

void APS5_VABI glClear_nid_postfix(GLenum mask) {
    std::lock_guard lock(mutex);
    auto& c = Current_nid_no_patch();
    if (mask & ~(0x4000u | 0x0100u | 0x0400u)) { Error_nid_no_patch(0x0501); return; }
    if (!Readable(c)) return;
    for (unsigned component = 0; component < 3; ++component) {
        if (!(mask & (component == 0 ? 0x4000 : component == 1 ? 0x0100 : 0x0400))) continue;
        Image* image = c.framebuffer ? c.framebuffer->attachments[component].Get() : &Thread().draw->image;
        if (!image) continue;
        std::int64_t x0 = 0, y0 = 0, x1 = image->width, y1 = image->height;
        if (c.enabled.contains(0x0c11)) {
            auto& box = c.state[0x0c10];
            x0 = std::max<std::int64_t>(0, box[0]); y0 = std::max<std::int64_t>(0, box[1]);
            x1 = std::min<std::int64_t>(x1, std::int64_t(box[0]) + std::int64_t(box[2]));
            y1 = std::min<std::int64_t>(y1, std::int64_t(box[1]) + std::int64_t(box[3]));
        }
        std::array<unsigned char, 4> color{};
        for (unsigned i = 0; i < 4; ++i) {
            double value = c.state[0x0c22][i];
            unsigned maximum = 255;
            if (image->format == 0x8056) maximum = 15;
            if (image->format == 0x8057) maximum = i == 3 ? 1 : 31;
            if (image->format == 0x8d62 && i != 3) maximum = i == 1 ? 63 : 31;
            color[i] = std::isnan(value) ? 0 : Expand(static_cast<unsigned>(std::round(value * maximum)), maximum);
        }
        color = Convert(color, image->format);
        auto stencilMask = static_cast<unsigned char>(static_cast<GLuint>(c.state[0x0b98][0]));
        auto stencil = static_cast<unsigned char>(static_cast<GLint>(c.state[0x0b91][0]));
        for (auto y = y0; y < y1; ++y) for (auto x = x0; x < x1; ++x) {
            auto index = std::size_t(y) * image->width + x;
            if (component == 0 && !image->color.empty()) for (unsigned i = 0; i < 4; ++i) {
                if (c.state[0x0c23][i]) image->color[index][i] = color[i];
            }
            if (component == 1 && !image->depth.empty() && c.state[0x0b72][0]) {
                double value = c.state[0x0b73][0];
                image->depth[index] = std::isnan(value) ? 0 : static_cast<float>(std::round(value * 65535) / 65535);
            }
            if (component == 2 && !image->stencil.empty()) image->stencil[index] = (image->stencil[index] & ~stencilMask) | (stencil & stencilMask);
        }
    }
}

void APS5_VABI glReadPixels_nid_postfix(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    std::lock_guard lock(mutex);
    auto& c = Current_nid_no_patch();
    if (width < 0 || height < 0) { Error_nid_no_patch(0x0501); return; }
    if (!Readable(c)) return;
    if (format != 0x1908 || type != 0x1401) {
        if (!Format(format) || (type != 0x1401 && type != 0x8033 && type != 0x8034 && type != 0x8363)) Error_nid_no_patch(0x0500);
        else Error_nid_no_patch(0x0502);
        return;
    }
    auto* image = Color_nid_no_patch(c, true);
    if (!image) { Error_nid_no_patch(0x0502); return; }
    std::size_t alignment = static_cast<std::size_t>(c.state[0x0d05][0]);
    std::size_t stride = (std::size_t(width) * 4 + alignment - 1) & ~(alignment - 1);
    auto* bytes = static_cast<unsigned char*>(pixels);
    std::int64_t firstColumn = std::max<std::int64_t>(0, -std::int64_t(x));
    std::int64_t lastColumn = std::min<std::int64_t>(width, std::int64_t(image->width) - x);
    std::int64_t firstRow = std::max<std::int64_t>(0, -std::int64_t(y));
    std::int64_t lastRow = std::min<std::int64_t>(height, std::int64_t(image->height) - y);
    for (auto row = firstRow; row < lastRow; ++row) for (auto col = firstColumn; col < lastColumn; ++col) {
        auto color = Convert(image->color[std::size_t(y + row) * image->width + std::size_t(x + col)], image->format);
        std::memcpy(bytes + std::size_t(row) * stride + std::size_t(col) * 4, color.data(), 4);
    }
}

void APS5_VABI glCopyTexImage2D_nid_postfix(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    std::lock_guard lock(mutex);
    int index = ImageTarget(target);
    if (index < 0 || !Dimensions(index, level, width, height, border)) return;
    if (!Format(format)) { Error_nid_no_patch(0x0501); return; }
    auto& c = Current_nid_no_patch();
    if (!Readable(c)) return;
    auto* source = Color_nid_no_patch(c, true);
    if (!source) { Error_nid_no_patch(0x0502); return; }
    if (!CopyFormat(format, source->format)) return;
    try {
        Image image;
        image.Allocate(width, height, format);
        Copy(image, 0, 0, *source, x, y, width, height);
        BoundTexture(index)->images[{target, level}] = std::move(image);
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glCopyTexSubImage2D_nid_postfix(GLenum target, GLint level, GLint dx, GLint dy, GLint x, GLint y, GLsizei width, GLsizei height) {
    std::lock_guard lock(mutex);
    int index = ImageTarget(target);
    if (index < 0) return;
    if (level < 0 || level > 11) { Error_nid_no_patch(0x0501); return; }
    auto texture = BoundTexture(index);
    auto it = texture->images.find({target, level});
    if (it == texture->images.end()) { Error_nid_no_patch(0x0502); return; }
    if (!Region(it->second, level, dx, dy, width, height)) return;
    auto& c = Current_nid_no_patch();
    if (!Readable(c)) return;
    auto* source = Color_nid_no_patch(c, true);
    if (!source) { Error_nid_no_patch(0x0502); return; }
    if (!CopyFormat(it->second.format, source->format)) return;
    Copy(it->second, dx, dy, *source, x, y, width, height);
}

}
