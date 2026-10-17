#ifndef CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_CONTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEPIGLETV2VSH_INCLUDE_CONTEXT_HPP

#include "prx/libScePigletv2VSH/include/GlConstants.hpp"
#include "prx/libScePigletv2VSH/include/Vulkan.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace Piglet {

struct Texture {
    Image image;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    GLenum minFilter = GL_NEAREST_MIPMAP_LINEAR;
    GLenum magFilter = GL_LINEAR;
    GLenum wrapS = GL_REPEAT;
    GLenum wrapT = GL_REPEAT;
};

struct Shader {
    GLenum type = 0;
    std::string source;
    bool deletePending = false;
};

struct Uniform {
    std::string name;
    GLenum type = 0;
    GLint size = 1;
    int offset = -1;
    int binding = -1;
};

struct Attribute {
    std::string name;
    GLenum type = 0;
    GLint location = -1;
};

struct Program {
    std::vector<GLuint> shaders;
    bool linked = false;
    bool deletePending = false;
    std::string log;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::vector<Uniform> uniforms;
    std::vector<Attribute> attributes;
    std::vector<std::uint8_t> uniformData;
    int uniformBinding = -1;
    std::vector<GLint> samplerUnits;
};

struct VertexAttrib {
    bool enabled = false;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    bool normalized = false;
    GLsizei stride = 0;
    const void* pointer = nullptr;
};

struct Ring {
    std::vector<Buffer> chunks;
    std::size_t index = 0;
    VkDeviceSize used = 0;
};

struct Surface {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    Image color;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    Buffer readback;
    bool presented = false;
};

class Context {
public:
    Context();

    void ActiveTexture(GLenum texture);
    void AttachShader(GLuint program, GLuint shader);
    void BindTexture(GLenum target, GLuint texture);
    void BlendFunc(GLenum source, GLenum destination);
    void Clear(GLbitfield mask);
    void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    GLuint CreateProgram();
    GLuint CreateShader(GLenum type);
    void DeleteProgram(GLuint program);
    void DeleteShader(GLuint shader);
    void DeleteTextures(GLsizei count, const GLuint* textures);
    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    void Enable(GLenum capability);
    void EnableVertexAttribArray(GLuint index);
    void GenTextures(GLsizei count, GLuint* textures);
    void GetActiveAttrib(GLuint program, GLuint index, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name);
    void GetActiveUniform(GLuint program, GLuint index, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name);
    GLint GetAttribLocation(GLuint program, const char* name);
    GLenum GetError();
    void GetProgramiv(GLuint program, GLenum name, GLint* value);
    const char* GetString(GLenum name);
    GLint GetUniformLocation(GLuint program, const char* name);
    void LinkProgram(GLuint program);
    void PixelStorei(GLenum name, GLint value);
    void ShaderBinary(GLsizei count, const GLuint* shaders, GLenum format, const void* binary, GLsizei length);
    void TexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void TexParameteri(GLenum target, GLenum name, GLint value);
    void Uniform1f(GLint location, GLfloat value);
    void Uniform1i(GLint location, GLint value);
    void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void UseProgram(GLuint program);
    void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
    void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);

    void SetSurface(Surface* surface);
    void Finish();

private:
    struct PipelineKey {
        const Program* program;
        bool blend;
        GLenum source;
        GLenum destination;
        bool cull;
        VkPrimitiveTopology topology;
        std::vector<std::tuple<std::uint32_t, VkFormat, std::uint32_t>> inputs;
        bool operator<(const PipelineKey& other) const;
    };

    void SetError(GLenum code);
    Program* FindProgram(GLuint program);
    Uniform* FindUniform(GLint location, GLenum type);
    void Record();
    VkDeviceSize Allocate(Ring& ring, const void* data, VkDeviceSize size, VkDeviceSize alignment, VkBuffer& buffer);
    VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout);
    void ReleasePrograms();
    void DropPipelines(const Program* program);
    VkPipeline Pipeline(const PipelineKey& key);
    VkSampler Sampler(const Texture& texture);
    const Texture& SampledTexture(GLint unit);
    void Retire(Image image);

    Device& device;
    Surface* surface = nullptr;
    GLenum error = GL_NO_ERROR;
    GLuint nextName = 1;
    std::map<GLuint, Texture> textures;
    std::map<GLuint, Shader> shaders;
    std::map<GLuint, std::unique_ptr<Program>> programs;
    std::array<GLuint, 16> boundTextures{};
    GLuint activeUnit = 0;
    GLuint currentProgram = 0;
    std::array<VertexAttrib, 16> attributes{};
    std::array<float, 4> clearColor{};
    std::array<GLint, 4> viewport{};
    bool viewportSet = false;
    bool blend = false;
    bool cull = false;
    GLenum blendSource = GL_ONE;
    GLenum blendDestination = GL_ZERO;
    GLint unpackAlignment = 4;
    GLint packAlignment = 4;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    bool recording = false;
    Ring vertexRing;
    Ring uniformRing;
    std::vector<VkDescriptorPool> pools;
    std::size_t poolUsed = 0;
    std::map<PipelineKey, VkPipeline> pipelines;
    std::map<std::tuple<GLenum, GLenum, GLenum, GLenum>, VkSampler> samplers;
    std::vector<Image> retired;
    Texture incomplete;
};

Context* Current();
void MakeCurrent(Context* context);
VkRenderPass ColorRenderPass();
void LinkProgram(Program& program, const Shader& vertex, const Shader& fragment);

}

#endif
