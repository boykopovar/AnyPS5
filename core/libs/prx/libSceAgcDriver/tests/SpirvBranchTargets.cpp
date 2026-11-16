#include "Recompiler.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include <spirv-tools/libspirv.hpp>
#endif
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> recompile(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 12> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu, 0u, 0u, 0u, 0u, 7u, 9u, 0x10002000u, 0u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    require(!result.spirv.empty(), "the shader did not recompile");
    return result.spirv.Copy();
}

enum class SampleSelection { Plain, Entry, EntryLoop, Loop };

void requireUniformSampleSelection(SampleSelection shape) {
    const bool selected = shape != SampleSelection::Plain;
    const bool loop = shape == SampleSelection::EntryLoop || shape == SampleSelection::Loop;
    IrProgram program;
    program.SetWaveSize(32u);
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    auto& entry = program.CreateBlock();
    program.SetEntryBlock(entry);
    program.BlockOrder().push_back(&entry);
    program.Metadata().blockInfo.emplace_back();
    IrBlock* headerBlock = &entry;
    if (shape == SampleSelection::Loop) {
        headerBlock = &program.CreateBlock();
        program.BlockOrder().push_back(headerBlock);
        program.Metadata().blockInfo.emplace_back().id = headerBlock->Id();
        program.Metadata().blockInfo.front().terminator.kind = TerminatorKind::Branch;
        program.Metadata().blockInfo.front().terminator.trueBlock = headerBlock->Id();
        entry.AddBranch(headerBlock);
    }
    IrBuilder builder(program);
    builder.SetInsertionPoint(*headerBlock);
    auto& zero = builder.Constant(0u);
    auto& flag = builder.GetUserData(static_cast<ScalarReg>(0u));
    auto& condition = builder.INotEqual(flag, zero);
    auto& imageHandle = builder.Emit(IrOpcode::GetImageResource, IrType::ImageResource, {&zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero});
    auto& samplerHandle = builder.Emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {&zero, &zero, &zero, &zero});
    auto& address = builder.Emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero, &zero});
    auto& sample = builder.Emit(IrOpcode::ImageSampleRaw, IrType::U32x4, {&imageHandle, &samplerHandle, &address, &builder.ConstantBool(true)});
    auto& channel = builder.CompositeExtract(sample, 0u);
    if (selected) static_cast<void>(builder.Select(condition, channel, zero));
    MemoryInfo memory{};
    memory.kind = ResourceKind::Image;
    memory.imageDimension = RdnaImageDimension::Dim2D;
    memory.imageAddressComponents = 3u;
    memory.imageSampleFlags = RdnaImageSampleFlagCompare;
    memory.dmask = 0xfu;
    memory.dataDwords = 4u;
    memory.componentCount = 4u;
    program.Resources().memoryInfo.push_back(memory);
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.numericClass = IrTextureNumericClass::Float;
    image.dimension = memory.imageDimension;
    image.read = true;
    image.depthCompare = true;
    image.emulatedCompare = EmulatedCompare::Unsupported;
    image.fmaskCompatible = false;
    image.cubeFaceCompatible = false;
    image.flatLineCompatible = false;
    program.Info().images.push_back(image);
    program.Info().samplers.emplace_back().depthCompare = true;
    auto& layout = program.Metadata().bindings;
    layout.userDataRegisters = {0u};
    layout.memoryOffsetDword = 1u;
    layout.pushDataStartDword = 0u;
    layout.descriptors = {{DescriptorBindingForImage(image), {0u}}, {DescriptorBindingKind::Samplers, {0u}}};
    if (loop) {
        auto& continuation = program.CreateBlock();
        auto& exit = program.CreateBlock();
        program.BlockOrder().push_back(&continuation);
        program.BlockOrder().push_back(&exit);
        program.Metadata().blockInfo.resize(program.BlockOrder().size());
        auto& header = program.Metadata().blockInfo[headerBlock->Id()];
        header.condition = &condition;
        header.terminator.kind = TerminatorKind::ConditionalBranch;
        header.terminator.trueBlock = continuation.Id();
        header.terminator.falseBlock = exit.Id();
        header.terminator.mergeBlock = exit.Id();
        header.terminator.continueBlock = continuation.Id();
        header.terminator.loopHeader = true;
        auto& backedge = program.Metadata().blockInfo[continuation.Id()];
        backedge.id = continuation.Id();
        backedge.terminator.kind = TerminatorKind::Branch;
        backedge.terminator.trueBlock = headerBlock->Id();
        program.Metadata().blockInfo[exit.Id()].id = exit.Id();
        headerBlock->AddBranch(&continuation);
        headerBlock->AddBranch(&exit);
        continuation.AddBranch(headerBlock);
    }
    ShaderPixelInputInfo pixel{};
    ShaderStageInputInfo inputs{};
    inputs.pixel = &pixel;
    CompiledBindingLayout bindings{};
    bindings.layout = layout;
    bindings.pushConstantSizeBytes = 4u;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00401000u;
    target.spirvVersion = 0x00010300u;
    target.subgroupSize = 32u;
    std::vector<std::uint32_t> words;
    try {
        words = SpirvEmitter().Emit(program, inputs, bindings, target);
    } catch (const std::runtime_error& error) {
        if (shape == SampleSelection::EntryLoop && std::string_view(error.what()) == "value IR entry block has a predecessor") return;
        throw;
    }
    require(shape != SampleSelection::EntryLoop, "uniform sample selection: accepted a backedge to the entry block");
    std::uint32_t label = 0u;
    std::uint32_t loopLabel = 0u;
    std::uint32_t continueLabel = 0u;
    bool backedgeFound = false;
    std::uint32_t selections = 0u;
    for (std::size_t cursor = 5u; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        require(count != 0u && count <= words.size() - cursor, "uniform sample selection: truncated SPIR-V instruction");
        const auto opcode = words[cursor] & 0xffffu;
        if (opcode == spv::OpLabel) label = words[cursor + 1u];
        if (opcode == spv::OpLoopMerge) {
            loopLabel = label;
            continueLabel = words[cursor + 2u];
        }
        if (opcode == spv::OpBranch && label == continueLabel) {
            require(words[cursor + 1u] == loopLabel, "uniform sample selection: backedge does not target the loop header");
            backedgeFound = true;
        }
        if (opcode == spv::OpSelectionMerge && words[cursor + 2u] == spv::SelectionControlDontFlattenMask) ++selections;
        cursor += count;
    }
    require(loop ? loopLabel != 0u && backedgeFound : loopLabel == 0u, "uniform sample selection: unexpected loop structure");
    require(selections == (selected && !loop ? 1u : 0u), "uniform sample selection: unexpected guarded sample region count");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    std::string diagnostics;
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
    validator.SetMessageConsumer([&](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message != nullptr) diagnostics += std::string(message) + '\n';
    });
    const bool valid = validator.Validate(words.data(), words.size());
    require(valid, "uniform sample selection: invalid SPIR-V:\n" + diagnostics);
#endif
}

void requireDistinctTargets(const char* name, std::span<const std::uint32_t> code) {
    const auto words = recompile(code);
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        require(count != 0 && cursor + count <= words.size(), std::string(name) + ": truncated SPIR-V instruction");
        if ((words[cursor] & 0xffffu) == spv::OpBranchConditional) {
            require(words[cursor + 2] != words[cursor + 3], std::string(name) + ": OpBranchConditional with the same true and false label, invalid in SPIR-V 1.6");
        }
        cursor += count;
    }
}

}

int main() {
    try {
        requireUniformSampleSelection(SampleSelection::Plain);
        requireUniformSampleSelection(SampleSelection::Entry);
        requireUniformSampleSelection(SampleSelection::EntryLoop);
        requireUniformSampleSelection(SampleSelection::Loop);
        constexpr std::array<std::uint32_t, 2> zeroOffsetBranch{0xbf840000u, 0xbf810000u};
        requireDistinctTargets("s_cbranch_scc0 to the next instruction", zeroOffsetBranch);
        constexpr std::array<std::uint32_t, 4> unalignedStore{0x7e020281u, 0xe0700000u, 0x80020100u, 0xbf810000u};
        requireDistinctTargets("store through a descriptor with an unaligned base", unalignedStore);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvBranchTargets: %s\n", error.what());
        return 1;
    }
    std::puts("SpirvBranchTargets: branch targets and uniform sample selections preserve structure");
    return 0;
}
