#include "prx/libScePigletv2VSH/include/Context.hpp"

#include "prx/libScePrecompiledShaders/include/PrecompiledShaders.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace Piglet {

namespace {

thread_local Context* current = nullptr;

constexpr VkDeviceSize ChunkSize = 4u << 20;

VkBlendFactor BlendFactor(GLenum factor) {
    switch (factor) {
        case GL_ZERO: return VK_BLEND_FACTOR_ZERO;
        case GL_ONE: return VK_BLEND_FACTOR_ONE;
        case GL_SRC_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
        case GL_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case GL_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case GL_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
        case GL_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case GL_DST_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
        case GL_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case GL_SRC_ALPHA_SATURATE: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default: throw std::runtime_error("libScePigletv2VSH: invalid blend factor");
    }
}

bool ValidBlendFactor(GLenum factor, bool source) {
    switch (factor) {
        case GL_ZERO: case GL_ONE: case GL_SRC_COLOR: case GL_ONE_MINUS_SRC_COLOR: case GL_SRC_ALPHA: case GL_ONE_MINUS_SRC_ALPHA:
        case GL_DST_ALPHA: case GL_ONE_MINUS_DST_ALPHA: case GL_DST_COLOR: case GL_ONE_MINUS_DST_COLOR: return true;
        case GL_SRC_ALPHA_SATURATE: return source;
        default: return false;
    }
}

bool MipmapFilter(GLenum filter) {
    return filter == GL_NEAREST_MIPMAP_NEAREST || filter == GL_LINEAR_MIPMAP_NEAREST || filter == GL_NEAREST_MIPMAP_LINEAR || filter == GL_LINEAR_MIPMAP_LINEAR;
}

VkFilter Filter(GLenum filter) {
    return filter == GL_NEAREST || filter == GL_NEAREST_MIPMAP_NEAREST || filter == GL_NEAREST_MIPMAP_LINEAR ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerAddressMode AddressMode(GLenum wrap) {
    switch (wrap) {
        case GL_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case GL_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkFormat VertexFormat(GLint size, GLenum type, bool normalized) {
    if (type == GL_FLOAT) {
        constexpr VkFormat formats[] = {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
        return formats[size - 1];
    }
    constexpr VkFormat unorm[] = {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    constexpr VkFormat scaled[] = {VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED};
    return normalized ? unorm[size - 1] : scaled[size - 1];
}

void Transition(Device& device, VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
                VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    device.vkCmdPipelineBarrier(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Upload(Device& device, Image& image, std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& texels) {
    Buffer staging = device.CreateBuffer(texels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(staging.mapped, texels.data(), texels.size());
    device.RunOnce([&](VkCommandBuffer commands) {
        Transition(device, commands, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        device.vkCmdCopyBufferToImage(commands, staging.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        Transition(device, commands, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    });
    device.DestroyBuffer(staging);
}

}

Context* Current() {
    if (!current) throw std::runtime_error("libScePigletv2VSH: a GL function was called without a current context");
    return current;
}

void MakeCurrent(Context* context) {
    current = context;
}

VkRenderPass ColorRenderPass() {
    static const VkRenderPass pass = [] {
        auto& device = Device::Get();
        VkAttachmentDescription attachment{};
        attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &reference;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        VkRenderPass result = VK_NULL_HANDLE;
        Check(device.vkCreateRenderPass(device.device, &info, nullptr, &result), "vkCreateRenderPass");
        return result;
    }();
    return pass;
}

bool Context::PipelineKey::operator<(const PipelineKey& other) const {
    return std::tie(program, blend, source, destination, cull, topology, inputs) <
           std::tie(other.program, other.blend, other.source, other.destination, other.cull, other.topology, other.inputs);
}

Context::Context() : device(Device::Get()) {
    renderPass = ColorRenderPass();
    commands = device.AllocateCommandBuffer();
    incomplete.width = 1;
    incomplete.height = 1;
    incomplete.minFilter = GL_NEAREST;
    incomplete.magFilter = GL_NEAREST;
    incomplete.image = device.CreateImage(1, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    Upload(device, incomplete.image, 1, 1, {0, 0, 0, 255});
}

Context::~Context() {
    for (auto& [name, program] : programs) program->deletePending = true;
    currentProgram = 0;
    ReleasePrograms();
    for (auto& image : retired) device.DestroyImage(image);
    for (auto& [name, texture] : textures) device.DestroyImage(texture.image);
    device.DestroyImage(incomplete.image);
    for (const auto& [key, sampler] : samplers) device.vkDestroySampler(device.device, sampler, nullptr);
    for (const auto pool : pools) device.vkDestroyDescriptorPool(device.device, pool, nullptr);
    for (auto* ring : {&vertexRing, &uniformRing})
        for (auto& chunk : ring->chunks) device.DestroyBuffer(chunk);
    device.vkFreeCommandBuffers(device.device, device.pool, 1, &commands);
}

void Context::SetError(GLenum code) {
    if (error == GL_NO_ERROR) error = code;
}

GLenum Context::GetError() {
    const GLenum result = error;
    error = GL_NO_ERROR;
    return result;
}

void Context::SetSurface(Surface* target) {
    if (recording && target != surface) throw std::runtime_error("eglMakeCurrent: switching surfaces in the middle of a frame is not implemented");
    surface = target;
    if (surface && !viewportSet) {
        viewport = {0, 0, static_cast<GLint>(surface->width), static_cast<GLint>(surface->height)};
        viewportSet = true;
    }
}

void Context::Retire(Image image) {
    if (!image.image) return;
    if (recording) retired.push_back(image);
    else device.DestroyImage(image);
}

void Context::ActiveTexture(GLenum texture) {
    if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + boundTextures.size()) return SetError(GL_INVALID_ENUM);
    activeUnit = texture - GL_TEXTURE0;
}

void Context::BindTexture(GLenum target, GLuint texture) {
    if (target != GL_TEXTURE_2D) {
        if (target == 0x8513) throw std::runtime_error("glBindTexture: cube map textures are not implemented");
        return SetError(GL_INVALID_ENUM);
    }
    textures.try_emplace(texture);
    boundTextures[activeUnit] = texture;
}

void Context::GenTextures(GLsizei count, GLuint* names) {
    if (count < 0) return SetError(GL_INVALID_VALUE);
    for (GLsizei index = 0; index < count; ++index) {
        names[index] = nextName++;
        textures.try_emplace(names[index]);
    }
}

void Context::DeleteTextures(GLsizei count, const GLuint* names) {
    if (count < 0) return SetError(GL_INVALID_VALUE);
    for (GLsizei index = 0; index < count; ++index) {
        const auto found = textures.find(names[index]);
        if (names[index] == 0 || found == textures.end()) continue;
        for (auto& bound : boundTextures)
            if (bound == names[index]) bound = 0;
        Retire(found->second.image);
        textures.erase(found);
    }
}

void Context::TexParameteri(GLenum target, GLenum name, GLint value) {
    if (target != GL_TEXTURE_2D) return SetError(GL_INVALID_ENUM);
    auto& texture = textures[boundTextures[activeUnit]];
    const auto parameter = static_cast<GLenum>(value);
    switch (name) {
        case GL_TEXTURE_MIN_FILTER:
            if (parameter != GL_NEAREST && parameter != GL_LINEAR && !MipmapFilter(parameter)) return SetError(GL_INVALID_ENUM);
            texture.minFilter = parameter;
            return;
        case GL_TEXTURE_MAG_FILTER:
            if (parameter != GL_NEAREST && parameter != GL_LINEAR) return SetError(GL_INVALID_ENUM);
            texture.magFilter = parameter;
            return;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            if (parameter != GL_REPEAT && parameter != GL_CLAMP_TO_EDGE && parameter != GL_MIRRORED_REPEAT) return SetError(GL_INVALID_ENUM);
            (name == GL_TEXTURE_WRAP_S ? texture.wrapS : texture.wrapT) = parameter;
            return;
        default:
            return SetError(GL_INVALID_ENUM);
    }
}

void Context::PixelStorei(GLenum name, GLint value) {
    if (name != GL_UNPACK_ALIGNMENT && name != GL_PACK_ALIGNMENT) return SetError(GL_INVALID_ENUM);
    if (value != 1 && value != 2 && value != 4 && value != 8) return SetError(GL_INVALID_VALUE);
    (name == GL_UNPACK_ALIGNMENT ? unpackAlignment : packAlignment) = value;
}

void Context::TexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    if (target != GL_TEXTURE_2D) return SetError(GL_INVALID_ENUM);
    if (level < 0 || width < 0 || height < 0 || border != 0) return SetError(GL_INVALID_VALUE);
    if (format != GL_RGBA && format != GL_RGB && format != GL_ALPHA && format != GL_LUMINANCE && format != GL_LUMINANCE_ALPHA) return SetError(GL_INVALID_ENUM);
    if (static_cast<GLenum>(internalFormat) != format) return SetError(GL_INVALID_OPERATION);
    if (type != GL_UNSIGNED_BYTE) throw std::runtime_error("glTexImage2D: texel types other than GL_UNSIGNED_BYTE are not implemented");
    if (level != 0) throw std::runtime_error("glTexImage2D: mipmap levels are not implemented");
    auto& texture = textures[boundTextures[activeUnit]];
    Retire(texture.image);
    texture.image = {};
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    if (width == 0 || height == 0) return;
    const std::size_t channels = format == GL_RGBA ? 4 : format == GL_RGB ? 3 : format == GL_LUMINANCE_ALPHA ? 2 : 1;
    const std::size_t rowBytes = static_cast<std::size_t>(width) * channels;
    const std::size_t stride = (rowBytes + static_cast<std::size_t>(unpackAlignment) - 1) / static_cast<std::size_t>(unpackAlignment) * static_cast<std::size_t>(unpackAlignment);
    std::vector<std::uint8_t> texels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    if (pixels) {
        const auto* source = static_cast<const std::uint8_t*>(pixels);
        for (GLsizei y = 0; y < height; ++y) {
            const std::uint8_t* row = source + static_cast<std::size_t>(y) * stride;
            std::uint8_t* out = texels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;
            for (GLsizei x = 0; x < width; ++x, out += 4) {
                const std::uint8_t* in = row + static_cast<std::size_t>(x) * channels;
                switch (format) {
                    case GL_RGBA: std::memcpy(out, in, 4); break;
                    case GL_RGB: out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = 255; break;
                    case GL_LUMINANCE: out[0] = out[1] = out[2] = in[0]; out[3] = 255; break;
                    case GL_LUMINANCE_ALPHA: out[0] = out[1] = out[2] = in[0]; out[3] = in[1]; break;
                    default: out[0] = out[1] = out[2] = 0; out[3] = in[0]; break;
                }
            }
        }
    }
    texture.image = device.CreateImage(texture.width, texture.height, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    Upload(device, texture.image, texture.width, texture.height, texels);
}

const Texture& Context::SampledTexture(GLint unit) {
    const auto found = textures.find(boundTextures[static_cast<std::size_t>(unit)]);
    if (found == textures.end() || !found->second.image.image) return incomplete;
    if (MipmapFilter(found->second.minFilter) && (found->second.width > 1 || found->second.height > 1)) return incomplete;
    return found->second;
}

VkSampler Context::Sampler(const Texture& texture) {
    const auto key = std::make_tuple(texture.minFilter, texture.magFilter, texture.wrapS, texture.wrapT);
    const auto found = samplers.find(key);
    if (found != samplers.end()) return found->second;
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = Filter(texture.magFilter);
    info.minFilter = Filter(texture.minFilter);
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = AddressMode(texture.wrapS);
    info.addressModeV = AddressMode(texture.wrapT);
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.maxLod = 0.25f;
    VkSampler sampler = VK_NULL_HANDLE;
    Check(device.vkCreateSampler(device.device, &info, nullptr, &sampler), "vkCreateSampler");
    samplers.emplace(key, sampler);
    return sampler;
}

GLuint Context::CreateShader(GLenum type) {
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER) {
        SetError(GL_INVALID_ENUM);
        return 0;
    }
    const GLuint name = nextName++;
    shaders[name].type = type;
    return name;
}

void Context::DeleteShader(GLuint shader) {
    if (shader == 0) return;
    const auto found = shaders.find(shader);
    if (found == shaders.end()) return SetError(GL_INVALID_VALUE);
    for (const auto& [name, program] : programs)
        if (std::find(program->shaders.begin(), program->shaders.end(), shader) != program->shaders.end()) {
            found->second.deletePending = true;
            return;
        }
    shaders.erase(found);
}

void Context::ShaderBinary(GLsizei count, const GLuint* names, GLenum format, const void* binary, GLsizei length) {
    if (count < 0 || length < 0) return SetError(GL_INVALID_VALUE);
    if (format != 0) return SetError(GL_INVALID_ENUM);
    if (count != 1) throw std::runtime_error("glShaderBinary: a binary for more than one shader is not implemented");
    const std::string_view bytes(static_cast<const char*>(binary), static_cast<std::size_t>(length));
    if (!bytes.starts_with(PrecompiledShaders::SourceMagic)) throw std::runtime_error("glShaderBinary: PS4 shader binaries are not implemented");
    const auto found = shaders.find(names[0]);
    if (found == shaders.end()) return SetError(GL_INVALID_VALUE);
    found->second.source = std::string(bytes.substr(PrecompiledShaders::SourceMagic.size()));
}

GLuint Context::CreateProgram() {
    const GLuint name = nextName++;
    programs.emplace(name, std::make_unique<Program>());
    return name;
}

Program* Context::FindProgram(GLuint program) {
    const auto found = programs.find(program);
    return found == programs.end() ? nullptr : found->second.get();
}

void Context::AttachShader(GLuint program, GLuint shader) {
    auto* target = FindProgram(program);
    const auto found = shaders.find(shader);
    if (!target || found == shaders.end()) return SetError(GL_INVALID_VALUE);
    for (const GLuint attached : target->shaders)
        if (attached == shader || shaders[attached].type == found->second.type) return SetError(GL_INVALID_OPERATION);
    target->shaders.push_back(shader);
}

void Context::LinkProgram(GLuint program) {
    auto* target = FindProgram(program);
    if (!target) return SetError(GL_INVALID_VALUE);
    if (recording && target->linked) throw std::runtime_error("glLinkProgram: relinking a program during a frame is not implemented");
    const Shader* vertex = nullptr;
    const Shader* fragment = nullptr;
    for (const GLuint attached : target->shaders) {
        const auto& shader = shaders[attached];
        (shader.type == GL_VERTEX_SHADER ? vertex : fragment) = &shader;
    }
    DropPipelines(target);
    if (!vertex || !fragment || vertex->source.empty() || fragment->source.empty()) {
        target->linked = false;
        target->log = "a vertex and a fragment shader with code are required";
        return;
    }
    Piglet::LinkProgram(*target, *vertex, *fragment);
}

void Context::UseProgram(GLuint program) {
    if (program != 0) {
        const auto* target = FindProgram(program);
        if (!target) return SetError(GL_INVALID_VALUE);
        if (!target->linked) return SetError(GL_INVALID_OPERATION);
    }
    currentProgram = program;
}

void Context::DeleteProgram(GLuint program) {
    if (program == 0) return;
    auto* target = FindProgram(program);
    if (!target) return SetError(GL_INVALID_VALUE);
    target->deletePending = true;
    if (!recording) ReleasePrograms();
}

void Context::ReleasePrograms() {
    for (auto found = programs.begin(); found != programs.end();) {
        if (!found->second->deletePending || found->first == currentProgram) {
            ++found;
            continue;
        }
        const Program* program = found->second.get();
        DropPipelines(program);
        for (const GLuint shader : program->shaders) {
            const auto attached = shaders.find(shader);
            if (attached != shaders.end() && attached->second.deletePending) shaders.erase(attached);
        }
        if (program->vertex) device.vkDestroyShaderModule(device.device, program->vertex, nullptr);
        if (program->fragment) device.vkDestroyShaderModule(device.device, program->fragment, nullptr);
        if (program->layout) device.vkDestroyPipelineLayout(device.device, program->layout, nullptr);
        if (program->setLayout) device.vkDestroyDescriptorSetLayout(device.device, program->setLayout, nullptr);
        found = programs.erase(found);
    }
}

void Context::DropPipelines(const Program* program) {
    for (auto pipeline = pipelines.begin(); pipeline != pipelines.end();) {
        if (pipeline->first.program != program) {
            ++pipeline;
            continue;
        }
        device.vkDestroyPipeline(device.device, pipeline->second, nullptr);
        pipeline = pipelines.erase(pipeline);
    }
}

void Context::GetProgramiv(GLuint program, GLenum name, GLint* value) {
    const auto* target = FindProgram(program);
    if (!target) return SetError(GL_INVALID_VALUE);
    const auto longest = [](const auto& items) {
        std::size_t length = 0;
        for (const auto& item : items) length = std::max(length, item.name.size() + 1);
        return static_cast<GLint>(length);
    };
    switch (name) {
        case GL_LINK_STATUS:
        case GL_VALIDATE_STATUS: *value = target->linked ? 1 : 0; return;
        case GL_DELETE_STATUS: *value = target->deletePending ? 1 : 0; return;
        case GL_INFO_LOG_LENGTH: *value = target->log.empty() ? 0 : static_cast<GLint>(target->log.size() + 1); return;
        case GL_ATTACHED_SHADERS: *value = static_cast<GLint>(target->shaders.size()); return;
        case GL_ACTIVE_UNIFORMS: *value = static_cast<GLint>(target->uniforms.size()); return;
        case GL_ACTIVE_UNIFORM_MAX_LENGTH: *value = longest(target->uniforms); return;
        case GL_ACTIVE_ATTRIBUTES: *value = static_cast<GLint>(target->attributes.size()); return;
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: *value = longest(target->attributes); return;
        default: return SetError(GL_INVALID_ENUM);
    }
}

namespace {

template <typename T> void Describe(const T& item, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name, GLint itemSize) {
    const std::size_t copied = bufferSize > 0 ? std::min(item.name.size(), static_cast<std::size_t>(bufferSize - 1)) : 0;
    if (bufferSize > 0 && name) {
        std::memcpy(name, item.name.data(), copied);
        name[copied] = '\0';
    }
    if (length) *length = static_cast<GLsizei>(copied);
    if (size) *size = itemSize;
    if (type) *type = item.type;
}

}

void Context::GetActiveAttrib(GLuint program, GLuint index, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name) {
    const auto* target = FindProgram(program);
    if (!target || index >= target->attributes.size() || bufferSize < 0) return SetError(GL_INVALID_VALUE);
    Describe(target->attributes[index], bufferSize, length, size, type, name, 1);
}

void Context::GetActiveUniform(GLuint program, GLuint index, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name) {
    const auto* target = FindProgram(program);
    if (!target || index >= target->uniforms.size() || bufferSize < 0) return SetError(GL_INVALID_VALUE);
    Describe(target->uniforms[index], bufferSize, length, size, type, name, target->uniforms[index].size);
}

GLint Context::GetAttribLocation(GLuint program, const char* name) {
    const auto* target = FindProgram(program);
    if (!target) {
        SetError(GL_INVALID_VALUE);
        return -1;
    }
    if (!target->linked) {
        SetError(GL_INVALID_OPERATION);
        return -1;
    }
    for (const auto& attribute : target->attributes)
        if (attribute.name == name) return attribute.location;
    return -1;
}

GLint Context::GetUniformLocation(GLuint program, const char* name) {
    const auto* target = FindProgram(program);
    if (!target) {
        SetError(GL_INVALID_VALUE);
        return -1;
    }
    if (!target->linked) {
        SetError(GL_INVALID_OPERATION);
        return -1;
    }
    const std::string_view wanted(name);
    for (std::size_t index = 0; index < target->uniforms.size(); ++index) {
        const auto& uniform = target->uniforms[index];
        if (uniform.name == wanted || (uniform.size > 1 && uniform.name + "[0]" == wanted)) return static_cast<GLint>(index);
    }
    return -1;
}

Uniform* Context::FindUniform(GLint location, GLenum type) {
    auto* program = FindProgram(currentProgram);
    if (!program) {
        SetError(GL_INVALID_OPERATION);
        return nullptr;
    }
    if (location == -1) return nullptr;
    if (location < 0 || static_cast<std::size_t>(location) >= program->uniforms.size()) {
        SetError(GL_INVALID_OPERATION);
        return nullptr;
    }
    auto& uniform = program->uniforms[static_cast<std::size_t>(location)];
    const bool sampler = uniform.type == GL_SAMPLER_2D || uniform.type == 0x8B60;
    if (type == GL_SAMPLER_2D ? !(sampler || uniform.type == 0x1404 || uniform.type == 0x8B56) : uniform.type != type) {
        SetError(GL_INVALID_OPERATION);
        return nullptr;
    }
    return &uniform;
}

void Context::Uniform1f(GLint location, GLfloat value) {
    const auto* uniform = FindUniform(location, GL_FLOAT);
    if (!uniform) return;
    std::memcpy(FindProgram(currentProgram)->uniformData.data() + uniform->offset, &value, sizeof(value));
}

void Context::Uniform1i(GLint location, GLint value) {
    const auto* uniform = FindUniform(location, GL_SAMPLER_2D);
    if (!uniform) return;
    auto* program = FindProgram(currentProgram);
    if (uniform->offset < 0) {
        if (value < 0 || static_cast<std::size_t>(value) >= boundTextures.size()) return SetError(GL_INVALID_VALUE);
        program->samplerUnits[static_cast<std::size_t>(location)] = value;
        return;
    }
    std::memcpy(program->uniformData.data() + uniform->offset, &value, sizeof(value));
}

void Context::UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (count < 0 || transpose) return SetError(GL_INVALID_VALUE);
    const auto* uniform = FindUniform(location, GL_FLOAT_MAT4);
    if (!uniform) return;
    if (count > uniform->size) return SetError(GL_INVALID_OPERATION);
    std::memcpy(FindProgram(currentProgram)->uniformData.data() + uniform->offset, value, static_cast<std::size_t>(count) * 64);
}

void Context::EnableVertexAttribArray(GLuint index) {
    if (index >= attributes.size()) return SetError(GL_INVALID_VALUE);
    attributes[index].enabled = true;
}

void Context::VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
    if (index >= attributes.size() || size < 1 || size > 4 || stride < 0) return SetError(GL_INVALID_VALUE);
    if (type != GL_FLOAT && type != GL_UNSIGNED_BYTE) {
        if (type >= 0x1400 && type <= 0x140C) throw std::runtime_error("glVertexAttribPointer: attribute types other than GL_FLOAT and GL_UNSIGNED_BYTE are not implemented");
        return SetError(GL_INVALID_ENUM);
    }
    auto& attribute = attributes[index];
    attribute.size = size;
    attribute.type = type;
    attribute.normalized = normalized != 0;
    attribute.stride = stride;
    attribute.pointer = pointer;
}

void Context::Enable(GLenum capability) {
    switch (capability) {
        case GL_BLEND: blend = true; return;
        case GL_CULL_FACE: cull = true; return;
        case GL_DITHER:
        case GL_DEPTH_TEST:
        case GL_STENCIL_TEST:
        case GL_SCISSOR_TEST: return;
        case 0x8037:
        case 0x809E:
        case 0x80A0: throw std::runtime_error("glEnable: polygon offset and sample coverage are not implemented");
        default: return SetError(GL_INVALID_ENUM);
    }
}

void Context::BlendFunc(GLenum source, GLenum destination) {
    if (!ValidBlendFactor(source, true) || !ValidBlendFactor(destination, false)) return SetError(GL_INVALID_ENUM);
    blendSource = source;
    blendDestination = destination;
}

void Context::ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    clearColor = {std::clamp(red, 0.0f, 1.0f), std::clamp(green, 0.0f, 1.0f), std::clamp(blue, 0.0f, 1.0f), std::clamp(alpha, 0.0f, 1.0f)};
}

void Context::Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (width < 0 || height < 0) return SetError(GL_INVALID_VALUE);
    viewport = {x, y, width, height};
    viewportSet = true;
}

const char* Context::GetString(GLenum name) {
    switch (name) {
        case GL_VENDOR: return "AnyPS5";
        case GL_RENDERER: return "AnyPS5 Piglet on Vulkan";
        case GL_VERSION: return "OpenGL ES 2.0 AnyPS5";
        case GL_SHADING_LANGUAGE_VERSION: return "OpenGL ES GLSL ES 1.00";
        case GL_EXTENSIONS: return "GL_OES_texture_npot";
        default:
            SetError(GL_INVALID_ENUM);
            return nullptr;
    }
}

void Context::Record() {
    if (recording) return;
    if (!surface) throw std::runtime_error("libScePigletv2VSH: drawing without a current surface");
    Check(device.vkResetCommandBuffer(commands, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(device.vkBeginCommandBuffer(commands, &begin), "vkBeginCommandBuffer");
    Transition(device, commands, surface->color.image, surface->presented ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = renderPass;
    pass.framebuffer = surface->framebuffer;
    pass.renderArea = {{0, 0}, {surface->width, surface->height}};
    device.vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);
    vertexRing.index = vertexRing.used = 0;
    uniformRing.index = uniformRing.used = 0;
    for (const auto pool : pools) Check(device.vkResetDescriptorPool(device.device, pool, 0), "vkResetDescriptorPool");
    poolUsed = 0;
    recording = true;
}

VkDeviceSize Context::Allocate(Ring& ring, const void* data, VkDeviceSize size, VkDeviceSize alignment, VkBuffer& buffer) {
    VkDeviceSize offset = (ring.used + alignment - 1) / alignment * alignment;
    if (ring.chunks.empty() || offset + size > ring.chunks[ring.index].size) {
        if (!ring.chunks.empty()) ++ring.index;
        while (ring.index < ring.chunks.size() && ring.chunks[ring.index].size < size) ++ring.index;
        if (ring.index >= ring.chunks.size()) {
            ring.chunks.push_back(device.CreateBuffer(std::max(ChunkSize, size), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
            ring.index = ring.chunks.size() - 1;
        }
        offset = 0;
    }
    auto& chunk = ring.chunks[ring.index];
    std::memcpy(static_cast<std::uint8_t*>(chunk.mapped) + offset, data, static_cast<std::size_t>(size));
    ring.used = offset + size;
    buffer = chunk.buffer;
    return offset;
}

VkDescriptorSet Context::AllocateSet(VkDescriptorSetLayout layout) {
    while (true) {
        if (poolUsed == pools.size()) {
            const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024}};
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.maxSets = 256;
            info.poolSizeCount = 2;
            info.pPoolSizes = sizes;
            VkDescriptorPool pool = VK_NULL_HANDLE;
            Check(device.vkCreateDescriptorPool(device.device, &info, nullptr, &pool), "vkCreateDescriptorPool");
            pools.push_back(pool);
        }
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool = pools[poolUsed];
        info.descriptorSetCount = 1;
        info.pSetLayouts = &layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkResult result = device.vkAllocateDescriptorSets(device.device, &info, &set);
        if (result == VK_SUCCESS) return set;
        if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL) Check(result, "vkAllocateDescriptorSets");
        ++poolUsed;
    }
}

VkPipeline Context::Pipeline(const PipelineKey& key) {
    const auto found = pipelines.find(key);
    if (found != pipelines.end()) return found->second;
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = key.program->vertex;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = key.program->fragment;
    stages[1].pName = "main";
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> inputs;
    for (std::uint32_t index = 0; index < key.inputs.size(); ++index) {
        const auto& [location, format, stride] = key.inputs[index];
        bindings.push_back({index, stride, VK_VERTEX_INPUT_RATE_VERTEX});
        inputs.push_back({location, index, format, 0});
    }
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(inputs.size());
    vertexInput.pVertexAttributeDescriptions = inputs.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = key.topology;
    VkPipelineViewportDepthClipControlCreateInfoEXT clip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
    clip.negativeOneToOne = VK_TRUE;
    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.pNext = device.depthClipControl ? &clip : nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = key.cull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = key.blend ? VK_TRUE : VK_FALSE;
    attachment.srcColorBlendFactor = attachment.srcAlphaBlendFactor = BlendFactor(key.source);
    attachment.dstColorBlendFactor = attachment.dstAlphaBlendFactor = BlendFactor(key.destination);
    attachment.colorBlendOp = attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &attachment;
    const VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamic;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = &dynamicState;
    info.layout = key.program->layout;
    info.renderPass = renderPass;
    VkPipeline pipeline = VK_NULL_HANDLE;
    Check(device.vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "vkCreateGraphicsPipelines");
    pipelines.emplace(key, pipeline);
    return pipeline;
}

void Context::Clear(GLbitfield mask) {
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) return SetError(GL_INVALID_VALUE);
    if (!(mask & GL_COLOR_BUFFER_BIT)) return;
    Record();
    VkClearAttachment attachment{};
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = 0;
    std::memcpy(attachment.clearValue.color.float32, clearColor.data(), sizeof(float) * 4);
    VkClearRect rect{};
    rect.rect = {{0, 0}, {surface->width, surface->height}};
    rect.layerCount = 1;
    device.vkCmdClearAttachments(commands, 1, &attachment, 1, &rect);
}

void Context::DrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (mode > GL_TRIANGLE_FAN) return SetError(GL_INVALID_ENUM);
    if (first < 0 || count < 0) return SetError(GL_INVALID_VALUE);
    if (mode == GL_LINE_LOOP) throw std::runtime_error("glDrawArrays: GL_LINE_LOOP is not implemented");
    if (currentProgram == 0 || count == 0) return;
    auto* program = FindProgram(currentProgram);
    constexpr VkPrimitiveTopology topologies[] = {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
                                                  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN};
    Record();
    PipelineKey key{program, blend, blendSource, blendDestination, cull, topologies[mode], {}};
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceSize> offsets;
    for (const auto& input : program->attributes) {
        if (input.location < 0 || static_cast<std::size_t>(input.location) >= attributes.size()) throw std::runtime_error("glDrawArrays: an attribute location above 15 is not implemented");
        const auto& attribute = attributes[static_cast<std::size_t>(input.location)];
        VkBuffer buffer = VK_NULL_HANDLE;
        if (attribute.enabled) {
            const std::size_t element = static_cast<std::size_t>(attribute.size) * (attribute.type == GL_FLOAT ? 4 : 1);
            const std::size_t stride = attribute.stride ? static_cast<std::size_t>(attribute.stride) : element;
            const auto* data = static_cast<const std::uint8_t*>(attribute.pointer) + static_cast<std::size_t>(first) * stride;
            const std::size_t bytes = static_cast<std::size_t>(count - 1) * stride + element;
            offsets.push_back(Allocate(vertexRing, data, bytes, 4, buffer));
            key.inputs.emplace_back(static_cast<std::uint32_t>(input.location), VertexFormat(attribute.size, attribute.type, attribute.normalized), static_cast<std::uint32_t>(stride));
        } else {
            constexpr float fallback[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            offsets.push_back(Allocate(vertexRing, fallback, sizeof(fallback), 4, buffer));
            key.inputs.emplace_back(static_cast<std::uint32_t>(input.location), VK_FORMAT_R32G32B32A32_SFLOAT, 0u);
        }
        buffers.push_back(buffer);
    }
    const VkPipeline pipeline = Pipeline(key);
    const VkDescriptorSet set = AllocateSet(program->setLayout);
    std::vector<VkWriteDescriptorSet> writes;
    VkDescriptorBufferInfo uniformInfo{};
    if (program->uniformBinding >= 0) {
        uniformInfo.offset = Allocate(uniformRing, program->uniformData.data(), program->uniformData.size(), device.uniformAlignment, uniformInfo.buffer);
        uniformInfo.range = program->uniformData.size();
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = static_cast<std::uint32_t>(program->uniformBinding);
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &uniformInfo;
        writes.push_back(write);
    }
    std::vector<VkDescriptorImageInfo> images;
    images.reserve(program->uniforms.size());
    for (std::size_t index = 0; index < program->uniforms.size(); ++index) {
        const auto& uniform = program->uniforms[index];
        if (uniform.offset >= 0 || uniform.binding < 0) continue;
        const auto& texture = SampledTexture(program->samplerUnits[index]);
        images.push_back({Sampler(texture), texture.image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = static_cast<std::uint32_t>(uniform.binding);
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &images.back();
        writes.push_back(write);
    }
    device.vkUpdateDescriptorSets(device.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    device.vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    device.vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, program->layout, 0, 1, &set, 0, nullptr);
    if (!buffers.empty()) device.vkCmdBindVertexBuffers(commands, 0, static_cast<std::uint32_t>(buffers.size()), buffers.data(), offsets.data());
    const VkViewport view{static_cast<float>(viewport[0]), static_cast<float>(static_cast<GLint>(surface->height) - viewport[1]), static_cast<float>(viewport[2]),
                          -static_cast<float>(viewport[3]), 0.0f, 1.0f};
    device.vkCmdSetViewport(commands, 0, 1, &view);
    const VkRect2D scissor{{0, 0}, {surface->width, surface->height}};
    device.vkCmdSetScissor(commands, 0, 1, &scissor);
    device.vkCmdDraw(commands, static_cast<std::uint32_t>(count), 1, 0, 0);
}

void Context::Finish() {
    Record();
    device.vkCmdEndRenderPass(commands);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {surface->width, surface->height, 1};
    device.vkCmdCopyImageToBuffer(commands, surface->color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, surface->readback.buffer, 1, &copy);
    VkMemoryBarrier hostRead{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    hostRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostRead.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    device.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hostRead, 0, nullptr, 0, nullptr);
    Check(device.vkEndCommandBuffer(commands), "vkEndCommandBuffer");
    device.Submit(commands);
    recording = false;
    surface->presented = true;
    for (auto& image : retired) device.DestroyImage(image);
    retired.clear();
    ReleasePrograms();
}

}
