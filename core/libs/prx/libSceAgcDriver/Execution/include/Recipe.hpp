#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_RECIPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_RECIPE_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace AgcDriver {

// A compute dispatch's Vulkan objects (VulkanDevice.cpp), shared by every dispatch of one compiled
// variant.
struct ComputePipelineObjects;

// What a value-equal dispatch-cache hit needs to record its dispatch without preparing, finding or
// comparing anything again (design_cpu_final M4): the template the resource cache served the
// entry's compiled result with, the pipeline objects of its variant, the push constants and data
// words the compiled result fixes, and the pre-sync answer. Built at the end of a successful device
// call of a reusable object and attached to the dispatch-cache variant; immutable after that. It
// holds weak references only: the resource cache owns the template (its 1024-entry bound stays
// what pins textures and storage images) and the device's pipeline map owns the objects, so a dead
// reference is a recipe miss, never a leak past the caches' budgets.
struct Recipe {
    // R10: unusable on any other device.
    VkDevice device = VK_NULL_HANDLE;
    std::weak_ptr<Graphics::ShaderResources> templateRef;
    // The template's content key: touches the cache's LRU on a hit.
    Graphics::ResourceCache::Key key;
    std::weak_ptr<ComputePipelineObjects> objects;
    std::array<std::byte, Graphics::PipelinePushConstantBytes> pushBytes{};
    bool pushes = false;
    // FNV over the compiled result's ShaderData/FlattenedSrt words (ShaderResources::DataWordsHash):
    // the template's data buffers need a refresh iff its own hash differs.
    std::uint64_t dataWordsHash = 0;
    // The template's PresyncSurfaces() as of the attach and the host-import identity they were
    // computed under (a surface is CPU-read only while its memory has no import).
    std::vector<std::pair<std::uint64_t, std::uint64_t>> presyncSurfaces;
    Graphics::HostImportsProof presyncProof;
    // Both false for every recipe (only reusable objects get one); kept for the asserts.
    bool needsCompletion = false;
    bool holdsLease = false;
};

// Recorded: the dispatch went into the open batch from the recipe; Rebuild: the proof failed or an
// object went away, the caller runs the ordinary path (the recipe is re-attached after it).
enum class RecipeOutcome { Recorded, Rebuild };

// What a recorded draw needs to record again from its draw-cache hit without looking anything up
// (design_cpu_final M8, step 8b): the resource-cache template the draw's stages were served with
// (weak, as a dispatch recipe's), its pipeline and framebuffer (weak, R10: the pipeline store owns
// the pipeline and destroys it with its device (ClearCachedPipelines), the pipeline owns its
// framebuffer list; a recipe outlives a device replacement in the draw cache, so an owning
// reference would destroy the objects against a dead VkDevice), the resident targets (weak: a hit
// proves them on the stored objects while they are still Cached()) with their attachment views,
// and the per-draw derivations of the compiled stages and the state: the vertex input layout, the
// push constant block, the state with the unwritten outputs masked, the fragment outputs and the
// pipeline stages. Built at the end of a recorded, cacheable, reusable, non-indirect draw and
// attached to the draw entry's matched stage variants; immutable.
struct DrawRecipe {
    VkDevice device = VK_NULL_HANDLE;
    std::weak_ptr<Graphics::ShaderResources> templateRef;
    Graphics::ResourceCache::Key key;
    std::weak_ptr<Graphics::Pipeline> pipeline;
    std::weak_ptr<Graphics::Framebuffer> framebuffer;
    std::vector<std::weak_ptr<Graphics::StorageTexture>> targets;
    std::vector<VkImageView> targetViews;
    std::uint64_t passKey = 0;
    std::shared_ptr<const Graphics::VertexInputLayout> vertexInput;
    std::array<std::byte, Graphics::PipelinePushConstantBytes> pushBytes{};
    VkShaderStageFlags pushStages = 0;
    std::optional<Graphics::State> masked;
    std::set<std::uint32_t> fragmentOutputs;
    VkPipelineStageFlags shaderStages = 0;
};

}

#endif
