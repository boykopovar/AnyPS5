#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINEJOURNAL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINEJOURNAL_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "Recompiler.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

// What a graphics pipeline is built from besides the device: the stage results (named by their
// run-stable identity), the vertex input layout, the descriptor set layout keys and the state
// fields the Pipeline constructor reads, with every other State field cleared
// (CanonicalPipelineState). The pipeline store keys on exactly this, and the journal records it so
// the next run builds the same VkPipeline before the title asks for it. A generated stage (the
// rect-list control and evaluation pair, the barycentric geometry stage) is built from the vertex
// and fragment results and the device's limits; its identity is in the key like any other stage's,
// so a replayed pipeline built from another device's generated module is missed, not used.
struct GraphicsPipelineRecipe {
    struct Stage {
        ShaderRecompiler::ShaderStage stage;
        ShaderRecompiler::PipelineIdentity identity;
        std::uint32_t pushConstantOffset;
        bool generated;
    };
    VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::vector<Stage> stages;
    VertexInputLayout vertexInput;
    std::vector<std::uint32_t> mainLayoutKey;
    std::vector<std::uint32_t> fragmentLayoutKey;
    bool splitSets = false;
    State state;
};

[[nodiscard]] State CanonicalPipelineState(const State& state);
[[nodiscard]] bool GeneratedPipelineStage(const State& state, ShaderRecompiler::ShaderStage stage);
[[nodiscard]] GraphicsPipelineRecipe MakeGraphicsPipelineRecipe(const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout);
[[nodiscard]] std::vector<std::byte> EncodeGraphicsPipelineRecipe(const GraphicsPipelineRecipe& recipe);
[[nodiscard]] bool DecodeGraphicsPipelineRecipe(std::span<const std::byte> bytes, GraphicsPipelineRecipe& recipe);

struct PipelineJournalCounters {
    std::uint64_t loadedRecipes = 0;
    std::uint64_t loadedModules = 0;
    std::uint64_t loadedBytes = 0;
    std::uint64_t droppedRecords = 0;
    double loadMs = 0;
    std::uint64_t replayedGraphics = 0;
    std::uint64_t replayedCompute = 0;
    std::uint64_t replayPresent = 0;
    std::uint64_t replaySkipped = 0;
    std::uint64_t replayRejected = 0;
    std::uint64_t replayFailed = 0;
    double replayMs = 0;
    std::uint32_t replayThreads = 0;
    std::uint64_t recordedGraphics = 0;
    std::uint64_t recordedCompute = 0;
    std::uint64_t recordedModules = 0;
    std::uint64_t writtenBytes = 0;
    std::uint64_t writeFailures = 0;
};

// A decimal switch of the pipeline store or journal, or its default when unset; anything else throws.
[[nodiscard]] std::uint64_t PipelineSwitch(const char* name, std::uint64_t fallback);
[[nodiscard]] bool PipelineJournalEnabled();
// True on a replay thread while it builds an entry: what it builds is already in the file.
[[nodiscard]] bool PipelineJournalReplaying();
[[nodiscard]] std::filesystem::path PipelineJournalPath();
[[nodiscard]] PipelineJournalCounters PipelineJournalTotals();
void RecordGraphicsPipeline(const GraphicsPipelineRecipe& recipe, std::span<const CompiledShader> shaders);
void RecordComputePipeline(const ShaderRecompiler::RecompileResult& shader, bool pushConstants);
void FlushPipelineJournal();
void ReportPipelineJournal();

// Cancel stops the threads taking further entries (the pipeline each is building completes);
// Wait joins them. A device cancels its replay before it is replaced or destroyed.
class PipelineJournalReplay {
public:
    virtual ~PipelineJournalReplay() = default;
    virtual void Cancel() = 0;
    virtual void Wait() = 0;
    [[nodiscard]] virtual bool Done() const = 0;
};

struct PipelineJournalHooks {
    std::function<bool(const ShaderRecompiler::RecompileResult& shader, bool pushConstants)> compute;
};

// The journal belongs to the device that wrote it, named by the same properties as the driver's
// pipeline cache file; a replay for another device starts the file over.
[[nodiscard]] std::uint64_t PipelineJournalDeviceFingerprint(const VkPhysicalDeviceProperties& properties);
[[nodiscard]] std::shared_ptr<PipelineJournalReplay> StartPipelineJournalReplay(const Context& context, std::uint64_t deviceFingerprint, PipelineJournalHooks hooks);

}

#endif
