#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <set>

namespace AgcDriver::Graphics {

struct VertexInputLayout;

// The attachments of one render pass instance. Work recorded with it references the handle until
// the batch completes, so a recorded draw keeps the object (Recorder::Keep) like its pipeline.
class Framebuffer {
public:
    Framebuffer(const Context& context, VkRenderPass renderPass, std::span<const VkImageView> targets, VkExtent2D extent);
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    VkFramebuffer Handle() const { return framebuffer; }
    // Forgets the handle without destroying it (the device it belongs to is already gone).
    void Abandon() noexcept;

private:
    Context context;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

// The shader modules, layout, render pass and VkPipeline of one draw configuration. Viewport and
// scissor are dynamic state set at Begin, so pipelines are shared by draws that differ only there
// (see CachedPipeline).
class Pipeline {
public:
    // `state` carries the blend states with the outputs the pixel shader lacks already masked and
    // `vertexInput` the layout of the draw's vertex descriptors; the shaders were validated by the
    // caller (ValidateShaders).
    Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders);
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    VkPipelineLayout Layout() const;
    // The framebuffer of these attachment views. A resident target's view (`owners[i]` set) is stable
    // while its StorageTexture lives, so framebuffers made of resident views are kept in the pipeline
    // and reused when every owner is still the same object; any other set is built per call.
    std::shared_ptr<Framebuffer> AcquireFramebuffer(std::span<const VkImageView> targets, std::span<const std::shared_ptr<StorageTexture>> owners, VkExtent2D extent);
    // Begins the render pass on the framebuffer, binds the pipeline and sets viewport and scissor.
    void Begin(VkCommandBuffer commands, const Framebuffer& framebuffer, VkExtent2D extent, const VkViewport& viewport, const VkRect2D& scissor) const;
    void PushConstants(VkCommandBuffer commands, std::span<const CompiledShader> shaders) const;
    // Forgets the Vulkan objects without destroying them: for entries of a device that is already gone.
    void Abandon() noexcept;

private:
    struct CachedFramebuffer {
        std::vector<VkImageView> views;
        std::vector<std::weak_ptr<StorageTexture>> owners;
        VkExtent2D extent;
        std::shared_ptr<Framebuffer> framebuffer;
    };
    void release() noexcept;
    Context context;
    std::vector<VkShaderModule> _modules;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::size_t attachments = 0;
    std::vector<CachedFramebuffer> framebuffers;
};

// The pipeline for this configuration, shared across draws and frames: keyed by the stages' variant
// ids (the rect-list tessellation pair by the vertex and fragment ids that generate it), the vertex
// input layout, the descriptor set layout key, push constant stages and every pipeline state field.
// A configuration whose stage has no variant id gets a private pipeline. Entries are LRU-bounded and
// only evicted once no recorded draw holds them. Debug aid: APS5_NO_PIPELINE_CACHE=1 builds one per
// draw as before.
std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders);
// Destroys the cached pipelines of a device; to be called before the device goes away. Without it,
// entries of a gone device are recognised by their buffer pool (made and reset with the device, so
// it tells device instances apart when the loader reuses a VkDevice handle) and forgotten unused.
void ClearCachedPipelines(VkDevice device);
// Device limit checks of the viewport, which is dynamic state and so no longer checked by Pipeline.
void ValidateViewport(const Context& context, const VkViewport& viewport);

void ValidateShaderPair(const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment);
// Returns the color attachment locations the pixel shader writes.
std::set<std::uint32_t> ValidateShaders(std::span<const CompiledShader> shaders, const State& state, const VkPhysicalDeviceSubgroupProperties& subgroup, bool fragmentShaderBarycentric);

}

#endif
