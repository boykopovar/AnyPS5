#include "prx/libScePigletv2VSH/include/Context.hpp"
#include "prx/libScePrecompiledShaders/include/PrecompiledShaders.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace Piglet;

extern "C" {
extern const PrecompiledShaders::Entry scePrecompiledShaderEntries_nid_postfix[];
void* APS5_VABI eglGetDisplay(void*);
EGLBoolean APS5_VABI eglInitialize(void*, EGLint*, EGLint*);
EGLBoolean APS5_VABI eglBindAPI(EGLenum);
EGLBoolean APS5_VABI eglChooseConfig(void*, const EGLint*, void**, EGLint, EGLint*);
EGLBoolean APS5_VABI eglSwapInterval(void*, EGLint);
EGLint APS5_VABI eglGetError();
bool APS5_VABI scePigletSetConfigurationVSH(const void*);
void APS5_VABI glActiveTexture(GLenum);
void APS5_VABI glAttachShader(GLuint, GLuint);
void APS5_VABI glBindTexture(GLenum, GLuint);
void APS5_VABI glBlendFunc(GLenum, GLenum);
void APS5_VABI glClear(GLbitfield);
void APS5_VABI glClearColor(GLclampf, GLclampf, GLclampf, GLclampf);
GLuint APS5_VABI glCreateProgram();
GLuint APS5_VABI glCreateShader(GLenum);
void APS5_VABI glDrawArrays(GLenum, GLint, GLsizei);
void APS5_VABI glEnable(GLenum);
void APS5_VABI glEnableVertexAttribArray(GLuint);
void APS5_VABI glGenTextures(GLsizei, GLuint*);
void APS5_VABI glGetActiveAttrib(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, char*);
GLint APS5_VABI glGetAttribLocation(GLuint, const char*);
GLenum APS5_VABI glGetError();
void APS5_VABI glGetProgramiv(GLuint, GLenum, GLint*);
GLint APS5_VABI glGetUniformLocation(GLuint, const char*);
void APS5_VABI glLinkProgram(GLuint);
void APS5_VABI glPixelStorei(GLenum, GLint);
void APS5_VABI glShaderBinary(GLsizei, const GLuint*, GLenum, const void*, GLsizei);
void APS5_VABI glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
void APS5_VABI glTexParameteri(GLenum, GLenum, GLint);
void APS5_VABI glUniform1f(GLint, GLfloat);
void APS5_VABI glUniform1i(GLint, GLint);
void APS5_VABI glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*);
void APS5_VABI glUseProgram(GLuint);
void APS5_VABI glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
void APS5_VABI glViewport(GLint, GLint, GLsizei, GLsizei);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Piglet check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr std::uint32_t Size = 64;

static const PrecompiledShaders::Entry* FindEntry(const char* name) {
    for (const auto* entry = scePrecompiledShaderEntries_nid_postfix; entry->name; ++entry)
        if (std::strcmp(entry->name, name) == 0) return entry;
    return nullptr;
}

static std::array<int, 4> Pixel(const Surface& surface, std::uint32_t x, std::uint32_t row) {
    const auto* texel = static_cast<const std::uint8_t*>(surface.readback.mapped) + (static_cast<std::size_t>(row) * Size + x) * 4;
    return {texel[0], texel[1], texel[2], texel[3]};
}

static bool Near(const std::array<int, 4>& actual, const std::array<int, 4>& expected) {
    for (std::size_t index = 0; index < 4; ++index)
        if (std::abs(actual[index] - expected[index]) > 2) return false;
    return true;
}

template <typename TCall> static bool ThrowsWith(TCall call, const std::string& prefix) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).rfind(prefix, 0) == 0;
    }
    return false;
}

int main() {
    try {
        Device::Get();
    } catch (const std::runtime_error& error) {
        std::printf("skipped: %s\n", error.what());
        return 77;
    }
    auto& device = Device::Get();

    void* display = eglGetDisplay(nullptr);
    EGLint major = 0;
    EGLint minor = 0;
    Require(display && eglInitialize(display, &major, &minor) && major == 1 && minor == 4);
    Require(eglBindAPI(EGL_OPENGL_ES_API) && !eglBindAPI(0x30A2) && eglGetError() == EGL_BAD_PARAMETER);
    const EGLint sampleAttributes[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
                                       EGL_SAMPLE_BUFFERS, 0, EGL_SAMPLES, 0, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
    void* config = nullptr;
    EGLint configs = 0;
    Require(eglChooseConfig(display, sampleAttributes, &config, 1, &configs) && configs == 1 && config);
    const EGLint depthAttributes[] = {EGL_DEPTH_SIZE, 24, EGL_NONE};
    Require(eglChooseConfig(display, depthAttributes, &config, 1, &configs) && configs == 0);
    Require(eglSwapInterval(display, 0) && eglGetError() == EGL_SUCCESS);
    const std::uint32_t configuration[0x22] = {0x88, 0xe8};
    Require(scePigletSetConfigurationVSH(configuration));

    Surface surface;
    surface.width = Size;
    surface.height = Size;
    surface.color = device.CreateImage(Size, Size, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    surface.readback = device.CreateBuffer(Size * Size * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer.renderPass = ColorRenderPass();
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &surface.color.view;
    framebuffer.width = Size;
    framebuffer.height = Size;
    framebuffer.layers = 1;
    Require(device.vkCreateFramebuffer(device.device, &framebuffer, nullptr, &surface.framebuffer) == VK_SUCCESS);
    Context context;
    context.SetSurface(&surface);
    MakeCurrent(&context);

    const auto* vertex = FindEntry("texmap/v_2.vert");
    const auto* fragment = FindEntry("texmap/f_2.frag");
    Require(vertex && fragment);
    const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    Require(glCreateShader(0x1234) == 0 && glGetError() == GL_INVALID_ENUM);
    glShaderBinary(1, &vertexShader, 1, vertex->data, static_cast<GLsizei>(vertex->size));
    Require(glGetError() == GL_INVALID_ENUM);
    const char notSource[] = "\x02\x03\x02\x07";
    Require(ThrowsWith([&] { glShaderBinary(1, &vertexShader, 0, notSource, 4); }, "glShaderBinary: PS4 shader binaries are not implemented"));
    glShaderBinary(1, &vertexShader, 0, vertex->data, static_cast<GLsizei>(vertex->size));
    glShaderBinary(1, &fragmentShader, 0, fragment->data, static_cast<GLsizei>(fragment->size));
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glUseProgram(program);
    Require(glGetError() == GL_INVALID_OPERATION);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    GLint value = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &value);
    Require(value == 1);
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &value);
    Require(value == 5);
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &value);
    Require(value == 1);
    char name[16] = {};
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveAttrib(program, 0, sizeof(name), &length, &size, &type, name);
    Require(std::strcmp(name, "a_vertex") == 0 && length == 8 && size == 1 && type == 0x8B52);
    const GLint attribute = glGetAttribLocation(program, "a_vertex");
    const GLint projection = glGetUniformLocation(program, "u_projectionMatrix");
    const GLint modelView = glGetUniformLocation(program, "u_modelViewMatrix");
    const GLint textureSpace = glGetUniformLocation(program, "u_textureSpaceMatrix");
    const GLint opacity = glGetUniformLocation(program, "u_opacity");
    const GLint sampler = glGetUniformLocation(program, "s_sampler");
    Require(attribute >= 0 && projection >= 0 && modelView >= 0 && textureSpace >= 0 && opacity >= 0 && sampler >= 0);
    Require(glGetUniformLocation(program, "u_missing") == -1 && glGetError() == GL_NO_ERROR);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const std::uint8_t texels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, 0x9999, GL_NEAREST);
    Require(glGetError() == GL_INVALID_ENUM);

    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float leftHalf[] = {-1, -1, 0, 0, 0, -1, 1, 0, -1, 1, 0, 1, -1, 1, 0, 1, 0, -1, 1, 0, 0, 1, 1, 1};
    const float fullScreen[] = {-1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, -1, 1, 0, 1, 1, -1, 1, 0, 1, 1, 1, 1};
    glUseProgram(program);
    glUniformMatrix4fv(projection, 1, 0, identity);
    glUniformMatrix4fv(modelView, 1, 0, identity);
    glUniformMatrix4fv(textureSpace, 1, 0, identity);
    glUniformMatrix4fv(projection, 1, 1, identity);
    Require(glGetError() == GL_INVALID_VALUE);
    glUniform1i(sampler, 0);
    glUniform1f(sampler, 1.0f);
    Require(glGetError() == GL_INVALID_OPERATION);
    glUniform1f(opacity, 1.0f);
    glEnableVertexAttribArray(static_cast<GLuint>(attribute));
    glVertexAttribPointer(static_cast<GLuint>(attribute), 4, GL_FLOAT, 0, 16, leftHalf);

    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    context.Finish();
    Require(Near(Pixel(surface, 8, 48), {255, 0, 0, 255}) && Near(Pixel(surface, 24, 48), {0, 255, 0, 255}));
    Require(Near(Pixel(surface, 8, 16), {0, 0, 255, 255}) && Near(Pixel(surface, 24, 16), {255, 255, 255, 255}));
    Require(Near(Pixel(surface, 48, 32), {64, 64, 64, 255}));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform1f(opacity, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    context.Finish();
    Require(Near(Pixel(surface, 8, 48), {160, 32, 32, 191}) && Near(Pixel(surface, 48, 32), {64, 64, 64, 255}));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glUniform1f(opacity, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    context.Finish();
    Require(Near(Pixel(surface, 8, 48), {0, 0, 0, 255}) && Near(Pixel(surface, 24, 16), {0, 0, 0, 255}));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glVertexAttribPointer(static_cast<GLuint>(attribute), 4, GL_FLOAT, 0, 16, fullScreen);
    glViewport(0, 0, 32, 32);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    context.Finish();
    Require(Near(Pixel(surface, 8, 56), {255, 0, 0, 255}) && Near(Pixel(surface, 24, 40), {255, 255, 255, 255}));
    Require(Near(Pixel(surface, 8, 8), {64, 64, 64, 255}) && Near(Pixel(surface, 48, 56), {64, 64, 64, 255}));

    const std::uint8_t paddedRows[] = {255, 0, 0, 0, 0, 0, 255, 0};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, paddedRows);
    glViewport(0, 0, Size, Size);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    context.Finish();
    Require(Near(Pixel(surface, 32, 48), {255, 0, 0, 255}) && Near(Pixel(surface, 32, 16), {0, 0, 255, 255}));
    std::puts("guest_piglet: all checks passed");
}
