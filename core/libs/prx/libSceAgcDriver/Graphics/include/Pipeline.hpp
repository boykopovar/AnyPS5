#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include <set>

namespace AgcDriver::Graphics {

struct VertexInputLayout;

void LogPipelineStatistics_nid_no_patch(const Context& context, VkPipeline pipeline);

// The attachments of one render pass instance. Work recorded with it references the handle until
// the batch completes, so a recorded draw keeps the object (Recorder::Keep) like its pipeline.
class Framebuffer {
public:
    Framebuffer(const Context& context, VkRenderPass renderPass, std::span<const VkImageView> targets, VkExtent2D extent);
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    VkFramebuffer Handle() const { return framebuffer; }
    std::span<const VkImageView> Views() const { return views; }
    // Forgets the handle without destroying it (the device it belongs to is already gone).
    void Abandon() noexcept;

private:
    Context context;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    std::vector<VkImageView> views;
};

// The shader modules, layout, render pass and VkPipeline of one draw configuration. Viewport,
// scissor, depth bounds and depth bias are dynamic state set at Begin, so pipelines are shared by draws that differ only there
// (see CachedPipeline).
class Pipeline {
public:
    // `state` carries the blend states with the outputs the pixel shader lacks already masked and
    // `vertexInput` the layout of the draw's vertex descriptors; the shaders were validated by the
    // caller (ValidateShaders). `attachmentLayout` is the layout the color attachments are in
    // before, during and after the pass (GENERAL for resident targets, which then need no
    // transitions).
    Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    VkPipelineLayout Layout() const;
    std::shared_ptr<Framebuffer> AcquireFramebuffer(std::span<const VkImageView> targets, std::span<const std::shared_ptr<StorageTexture>> owners, VkExtent2D extent);
    // Begins the render pass on the framebuffer, binds the pipeline and sets its dynamic state.
    void Begin(VkCommandBuffer commands, const Framebuffer& framebuffer, VkExtent2D extent, const State& state) const;
    // The same inside a render pass another pipeline of the same attachments began (compatible by
    // construction: the attachment formats alone decide).
    void Continue(VkCommandBuffer commands, const State& state) const;
    bool SplitsFaces() const { return backFaces != VK_NULL_HANDLE; }
    bool Optimized() const { return optimized != nullptr && optimized->handle.load() != VK_NULL_HANDLE; }
    void ContinueBackFaces(VkCommandBuffer commands, const State& state) const;
    void PushConstants(VkCommandBuffer commands, VkShaderStageFlags stages, std::span<const std::byte, PipelinePushConstantBytes> bytes) const;
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
    void setLibraryState(VkCommandBuffer commands, const State& state) const;
    Context context;
    std::vector<VkShaderModule> _modules;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipeline backFaces = VK_NULL_HANDLE;
    std::size_t attachments = 0;
    std::size_t colorAttachments = 0;
    bool depthBounds = false;
    bool depthBias = false;
    bool libraries = false;
    std::shared_ptr<OptimizedPipeline> optimized;
    bool dynamicRendering = false;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::vector<std::uint32_t> colorSlots;
    std::vector<CachedFramebuffer> framebuffers;
};

// The pipeline for this configuration, shared across draws and frames: keyed by the stages' variant
// ids (the rect-list tessellation pair by the vertex and fragment ids that generate it), the vertex
// input layout, the descriptor set layout key, push constant stages and every pipeline state field.
// A configuration whose stage has no variant id gets a private pipeline. Entries are LRU-bounded and
// only evicted once no recorded draw holds them. Debug aid: APS5_NO_PIPELINE_CACHE=1 builds one per
// draw as before.
std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
// Destroys the cached pipelines of a device; to be called before the device goes away. Without it,
// entries of a gone device are recognised by their buffer pool (made and reset with the device, so
// it tells device instances apart when the loader reuses a VkDevice handle) and forgotten unused.
void ClearCachedPipelines(VkDevice device);
bool HasStencil(VkFormat format);
// Device limit checks of the viewport, which is dynamic state and so no longer checked by Pipeline.
void ValidateViewport(const Context& context, const VkViewport& viewport);
void ValidateDepthBounds(const Context& context, const State& state);
void ValidateProvokingVertex(const Context& context, const State& state, std::span<const CompiledShader> shaders = {});

void ValidateShaderPair(const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment);
// Returns the color attachment locations the pixel shader writes.
std::set<std::uint32_t> ValidateShaders(std::span<const CompiledShader> shaders, const State& state, const VkPhysicalDeviceSubgroupProperties& subgroup, bool fragmentShaderBarycentric, bool descriptorIndexing = false, bool imageInt64Atomics = false, bool geometryShader = false, bool sampleRateShading = false, bool bufferInt64Atomics = false);

}

#endif
