#pragma once

#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace Slim {

struct Buffer {
    GLuint name = 0;
    GLenum usage = 0x88e4;
    std::vector<unsigned char> data;
};

struct Image {
    GLint width = 0;
    GLint height = 0;
    GLenum format = 0;
    std::vector<std::array<unsigned char, 4>> color;
    std::vector<float> depth;
    std::vector<unsigned char> stencil;
    void Allocate(GLint w, GLint h, GLenum f);
};

struct Texture {
    GLuint name = 0;
    GLenum target = 0;
    std::map<std::pair<GLenum, GLint>, Image> images;
    std::map<GLenum, GLint> parameters{{0x2800, 0x2601}, {0x2801, 0x2702}, {0x2802, 0x2901}, {0x2803, 0x2901}};
};

struct Renderbuffer {
    GLuint name = 0;
    Image image;
};

struct Attachment {
    std::shared_ptr<Texture> texture;
    std::shared_ptr<Renderbuffer> renderbuffer;
    GLenum face = 0;
    GLint level = 0;
    Image* Get();
};

struct Framebuffer {
    GLuint name = 0;
    std::array<Attachment, 3> attachments;
};

struct Shader {
    GLenum type = 0;
    bool deleted = false;
    std::string source;
};

struct Program {
    bool deleted = false;
    std::map<GLuint, std::shared_ptr<Shader>> shaders;
    std::map<std::string, GLuint> attributes;
};

struct ShareGroup {
    std::map<GLuint, std::shared_ptr<Buffer>> buffers;
    std::map<GLuint, std::shared_ptr<Texture>> textures;
    std::map<GLuint, std::shared_ptr<Renderbuffer>> renderbuffers;
    std::map<GLuint, std::shared_ptr<Shader>> shaders;
    std::map<GLuint, std::shared_ptr<Program>> programs;
};

struct Attribute {
    std::array<float, 4> value{0, 0, 0, 1};
    bool enabled = false;
    GLint size = 4;
    GLenum type = 0x1406;
    bool normalized = false;
    GLsizei stride = 0;
    const void* pointer = nullptr;
    std::shared_ptr<Buffer> buffer;
};

struct Surface {
    Image image;
    bool deleted = false;
    bool largest = false;
    bool mipmap = false;
    EGLint colorspace = 0x3089;
    EGLint alphaFormat = 0x308b;
    GLint mipmapLevel = 0;
};

struct Context {
    bool deleted = false;
    bool bound = false;
    bool initializedViewport = false;
    std::shared_ptr<ShareGroup> share = std::make_shared<ShareGroup>();
    GLenum error = 0;
    std::map<GLenum, std::vector<double>> state;
    std::set<GLenum> enabled{0x0bd0};
    std::array<Attribute, 8> attributes;
    std::shared_ptr<Buffer> arrayBuffer;
    std::shared_ptr<Buffer> elementBuffer;
    std::shared_ptr<Renderbuffer> renderbuffer;
    std::map<GLuint, std::shared_ptr<Framebuffer>> framebuffers;
    std::shared_ptr<Framebuffer> framebuffer;
    std::array<std::array<std::shared_ptr<Texture>, 2>, 8> textures;
    std::array<std::shared_ptr<Texture>, 2> defaultTextures;
    unsigned activeTexture = 0;
    Context();
};

struct ThreadState {
    EGLint error = 0x3000;
    std::shared_ptr<Context> context;
    std::shared_ptr<Surface> draw;
    std::shared_ptr<Surface> read;
    ~ThreadState();
};

extern std::recursive_mutex mutex;
ThreadState& Thread();
extern std::map<EGLContext, std::shared_ptr<Context>> contexts;
extern std::map<EGLSurface, std::shared_ptr<Surface>> surfaces;
extern bool initialized;
extern int displayToken;
extern int configToken;

Context& Current_nid_no_patch();
void Error_nid_no_patch(GLenum error);
EGLBoolean EglError_nid_no_patch(EGLint error);
bool Display_nid_no_patch(EGLDisplay display);
bool Config_nid_no_patch(EGLConfig config);
bool Capability_nid_no_patch(GLenum cap);
bool Query_nid_no_patch(GLenum name, std::vector<double>& result);
GLenum Complete_nid_no_patch(Context& context);
Image* Color_nid_no_patch(Context& context, bool read);
float Clamp_nid_no_patch(float value);

template<class TObject>
GLuint Name_nid_no_patch(const std::map<GLuint, std::shared_ptr<TObject>>& objects) {
    GLuint name = 1;
    for (const auto& entry : objects) {
        if (entry.first != name) break;
        if (++name == 0) throw std::bad_alloc();
    }
    return name;
}

template<class TObject>
void Generate_nid_no_patch(GLsizei count, GLuint* names, std::map<GLuint, std::shared_ptr<TObject>>& objects) {
    if (count < 0) { Error_nid_no_patch(0x0501); return; }
    try {
        auto next = objects;
        std::vector<GLuint> result;
        for (GLsizei i = 0; i < count; ++i) {
            GLuint name = Name_nid_no_patch(next);
            next.emplace(name, nullptr);
            result.push_back(name);
        }
        objects.swap(next);
        std::copy(result.begin(), result.end(), names);
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

}
