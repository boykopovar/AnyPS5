#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Optimization/SrtWalker.hpp"
#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include "Optimization/SrtWalker.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "CacheKey.hpp"
#include "CompiledVariant.hpp"
#include "BdaAbi.hpp"
#include "ImageTableAbi.hpp"
#include "ShaderDiskCache.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <future>
#include <memory>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

ShaderRecompiler::SpirvTarget BufferTarget() {
    static constexpr std::array<std::uint32_t, 6> capabilities{spv::CapabilityShader, spv::CapabilityImageGatherExtended, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance};
    static constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    ShaderRecompiler::SpirvTarget target{};
    target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = extensions;
    return target;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void expectFailure(TAction action, const char* expected, const char* message) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(expected) == std::string::npos) throw std::runtime_error(std::string(message) + ": expected failure containing '" + expected + "', got: " + error.what());
        return;
    }
    throw std::runtime_error(message);
}

void verifyResult(const ShaderRecompiler::RecompileResult& first, const ShaderRecompiler::RecompileResult& second) {
    require(first.spirv == second.spirv, "replayed SPIR-V differs");
    require(first.pushConstants == second.pushConstants, "replayed push constants differ");
    require(first.bdaAbiVersion == second.bdaAbiVersion && first.bindings.size() == second.bindings.size(), "replayed layout differs");
    for (std::size_t index = 0; index < first.bindings.size(); ++index) {
        const auto& left = first.bindings[index];
        const auto& right = second.bindings[index];
        require(left.kind == right.kind && left.role == right.role && left.descriptorSet == right.descriptorSet && left.binding == right.binding && left.count == right.count && left.guestDescriptor == right.guestDescriptor && left.readOnly == right.readOnly, "replayed binding differs");
    }
}

ShaderRecompiler::RecompileRequest FrontendRequest(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x100000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target = BufferTarget();
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32u;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

void verifyFrontendIdentity() {
    using namespace ShaderRecompiler;
    const bool reuse = std::getenv("APS5_NO_PERF_FRONTEND_PAIR") == nullptr;
    const std::vector<std::uint32_t> code{0xbf800000u, 0xbf810000u};
    const std::array<std::uint32_t, 2> userData{};
    const auto request = FrontendRequest(code, userData);
    ShaderPreparationContext preparation;
    const auto first = preparation.AcquireFrontend(request);
    require(first != nullptr, "frontend pair: no frontend was prepared");
    require((preparation.AcquireFrontend(request) == first) == reuse, "frontend pair: repeated acquisition ignored the reuse switch");

    const auto relocatedCode = code;
    auto relocated = request;
    relocated.shader.code = relocatedCode;
    relocated.shader.codeAddress += 0x1000u;
    require((preparation.AcquireFrontend(relocated) == first) == reuse, "frontend pair: identical words at another allocation or address changed reuse");
    auto partial = request;
    partial.context.compute->partialThreads = {33u, 1u, 1u};
    require((preparation.AcquireFrontend(partial) == first) == reuse, "frontend pair: partial-thread state changed the frontend identity");
    auto target = request;
    target.target.subgroupSize = 64u;
    target.context.waveSize = 32u;
    target.context.compute->numThreads = {32u, 2u, 1u};
    require((preparation.AcquireFrontend(target) == first) == reuse, "frontend pair: translation-only state changed the frontend identity");
    const std::array<std::uint32_t, 2> changedUserData{1u, 2u};
    auto userValues = request;
    userValues.context.userData = changedUserData;
    require((preparation.AcquireFrontend(userValues) == first) == reuse, "frontend pair: user-data values changed the frontend identity");

    const auto verifyMiss = [&](const RecompileRequest& changed, const char* message) {
        ShaderPreparationContext isolated;
        const auto original = isolated.AcquireFrontend(request);
        const auto replacement = isolated.AcquireFrontend(changed);
        require(replacement != original, message);
        require((isolated.AcquireFrontend(changed) == replacement) == reuse, "frontend pair: a successful miss was not retained");
        require(isolated.AcquireFrontend(request) != original, "frontend pair: replacement retained more than the last frontend");
    };
    auto changed = request;
    auto changedCode = code;
    changedCode[0] = 0xbf800001u;
    changed.shader.code = changedCode;
    verifyMiss(changed, "frontend pair: changed code words reused the frontend");
    changedCode = code;
    changedCode.push_back(0xbf800000u);
    changed.shader.code = changedCode;
    verifyMiss(changed, "frontend pair: changed code length reused the frontend");
    changed = request;
    changed.context.userDataBaseRegister = 4u;
    verifyMiss(changed, "frontend pair: changed user-data base reused the frontend");
    changed = request;
    changed.context.userData = std::span<const std::uint32_t>(userData).first(1u);
    verifyMiss(changed, "frontend pair: changed user-data count reused the frontend");
    changed = request;
    changed.shader.stage = ShaderStage::Fragment;
    changed.context.compute.reset();
    changed.context.pixel = ShaderPixelStageInfo{};
    verifyMiss(changed, "frontend pair: a different stage reused the frontend");
    changed = request;
    changed.shader.stage = ShaderStage::Vertex;
    changed.context.compute.reset();
    changed.context.vertex = ShaderVertexStageInfo{};
    verifyMiss(changed, "frontend pair: a fetch-capable stage reused the compute frontend");

    auto invalid = request;
    invalid.shader.code = {};
    bool failed = false;
    try {
        static_cast<void>(preparation.AcquireFrontend(invalid));
    } catch (const std::out_of_range& error) {
        failed = std::string_view(error.what()).find("before s_endpgm") != std::string_view::npos;
    }
    require(failed, "frontend pair: invalid code did not fail during frontend construction");
    require((preparation.AcquireFrontend(request) == first) == reuse, "frontend pair: a failed miss replaced the prior frontend");
}

void verifyFrontendProgram(const ShaderRecompiler::IrProgram& reused, const ShaderRecompiler::IrProgram& fresh) {
    using namespace ShaderRecompiler;
    require(ProgramToString(reused) == ProgramToString(fresh), "frontend pair: reused frontend changed the IR");
    require(reused.WaveSize() == fresh.WaveSize() && reused.Info() == fresh.Info(), "frontend pair: reused frontend changed shader information");
    const auto& left = reused.Resources();
    const auto& right = fresh.Resources();
    require(left.stage == right.stage && left.shaderHash == right.shaderHash && left.userDataBase == right.userDataBase && left.userDataCount == right.userDataCount && left.srgbDecodeFormats == right.srgbDecodeFormats, "frontend pair: resource identity changed");
    require(left.memoryInfo == right.memoryInfo && left.materializationSources == right.materializationSources && left.guardedSrtSlots == right.guardedSrtSlots && left.srtGuardOffset == right.srtGuardOffset && left.cleanFlatSlots == right.cleanFlatSlots && left.pureFlatSlots == right.pureFlatSlots, "frontend pair: resource planning changed");
    require(left.descriptorSources.size() == right.descriptorSources.size() && left.srtReads.size() == right.srtReads.size(), "frontend pair: resource counts changed");
    for (std::size_t index = 0; index < left.descriptorSources.size(); ++index) {
        const auto& a = left.descriptorSources[index];
        const auto& b = right.descriptorSources[index];
        require(a.dwordCount == b.dwordCount && a.tableColumn == b.tableColumn, "frontend pair: descriptor planning changed");
        for (std::size_t word = 0; word < a.dwords.size(); ++word) {
            require((a.dwords[word] == nullptr) == (b.dwords[word] == nullptr), "frontend pair: descriptor word presence changed");
            if (a.dwords[word] != nullptr) require(a.dwords[word] != b.dwords[word] && a.dwords[word]->Id() == b.dwords[word]->Id(), "frontend pair: descriptor values were shared or changed");
        }
    }
    require(reused.BlockOrder().size() == fresh.BlockOrder().size(), "frontend pair: block count changed");
    for (std::size_t block = 0; block < reused.BlockOrder().size(); ++block) {
        const auto* a = reused.BlockOrder()[block];
        const auto* b = fresh.BlockOrder()[block];
        require(a != b && a->Instructions().size() == b->Instructions().size(), "frontend pair: mutable IR blocks were shared or changed");
        auto other = b->Instructions().begin();
        for (const auto* x : a->Instructions()) {
            const auto* y = *other++;
            require(x != y && x->Flags<std::uint64_t>() == y->Flags<std::uint64_t>(), "frontend pair: mutable IR values were shared or their flags changed");
        }
    }
}

void verifyFrontendPair() {
    using namespace ShaderRecompiler;
    verifyFrontendIdentity();
    const bool reuse = std::getenv("APS5_NO_PERF_FRONTEND_PAIR") == nullptr;
    const std::vector<std::uint32_t> code{0xe0700000u, 0x80000000u, 0xbf810000u};
    const std::array<std::uint32_t, 4> buffer{0x10000000u, 0u, 0x40u, 0x00027facu};
    const auto request = FrontendRequest(code, buffer);
    auto partial = request;
    partial.context.compute->partialThreads = {33u, 1u, 1u};
    const auto freshFull = PrepareResourceProgram(request);
    const auto freshPartial = PrepareResourceProgram(partial);
    require(ProgramToString(freshFull) != ProgramToString(freshPartial), "frontend pair: fixture does not distinguish full and partial threads");
    const auto storeThreadLimits = [](const IrProgram& program) {
        std::vector<const IrValue*> pending;
        for (const auto* block : program.BlockOrder()) {
            for (const auto* inst : block->Instructions()) {
                if (inst->Opcode() == IrOpcode::StoreBufferU32) pending.push_back(inst->Argument(inst->ArgumentCount() - 1u));
            }
        }
        require(pending.size() == 1u, "frontend pair: fixture does not retain exactly one buffer store");
        std::vector<const IrValue*> visited;
        std::uint32_t axes = 0u;
        while (!pending.empty()) {
            const auto* value = pending.back()->Resolve();
            pending.pop_back();
            if (std::find(visited.begin(), visited.end(), value) != visited.end()) continue;
            visited.push_back(value);
            if (value->Opcode() == IrOpcode::GetBuiltin && value->Argument(0)->Resolve()->ImmediateU32() == static_cast<std::uint32_t>(StageInputKind::DispatchThreadLimit)) {
                const auto axis = value->Argument(1)->Resolve()->ImmediateU32();
                require(axis < 3u, "frontend pair: invalid dispatch-limit axis");
                axes |= 1u << axis;
            }
            for (const auto* argument : value->Arguments()) pending.push_back(argument);
        }
        return axes;
    };
    require(storeThreadLimits(freshFull) == 0u && storeThreadLimits(freshPartial) == 7u, "frontend pair: buffer-store predicate does not retain all partial-thread bounds");
    require(!freshFull.Resources().memoryInfo.empty() && !freshFull.Resources().descriptorSources.empty() && !freshFull.Info().buffers.empty(), "frontend pair: fixture has no buffer resources");

    for (const bool partialFirst : {false, true}) {
        ShaderPreparationContext preparation;
        std::shared_ptr<const PreparedControlFlow> retained;
        {
            auto temporary = code;
            auto borrowed = request;
            borrowed.shader.code = temporary;
            retained = preparation.AcquireFrontend(borrowed);
            std::fill(temporary.begin(), temporary.end(), 0xffffffffu);
            require((preparation.AcquireFrontend(request) == retained) == reuse, "frontend pair: mutating the caller's code changed the retained frontend");
        }
        require((preparation.AcquireFrontend(request) == retained) == reuse, "frontend pair: destroying the caller's code changed the retained frontend");
        const auto first = PrepareResourceProgram(partialFirst ? partial : request, &preparation);
        const auto second = PrepareResourceProgram(partialFirst ? request : partial, &preparation);
        verifyFrontendProgram(first, partialFirst ? freshPartial : freshFull);
        verifyFrontendProgram(second, partialFirst ? freshFull : freshPartial);
        require(first.BlockOrder().front() != second.BlockOrder().front(), "frontend pair: full and partial variants share mutable IR");
        const auto repeated = PrepareResourceProgram(partialFirst ? partial : request, &preparation);
        verifyFrontendProgram(repeated, first);
        auto translated = request;
        translated.context.waveSize = 32u;
        translated.context.compute->numThreads = {32u, 2u, 1u};
        const auto changed = PrepareResourceProgram(translated, &preparation);
        const auto freshChanged = PrepareResourceProgram(translated);
        verifyFrontendProgram(changed, freshChanged);
        require(changed.WaveSize() != freshFull.WaveSize(), "frontend pair: changed translation inputs were ignored");
        require((preparation.AcquireFrontend(request) == retained) == reuse, "frontend pair: translating a variant replaced the frontend");
    }

    for (const bool partialFirst : {false, true}) {
        auto pairCode = code;
        pairCode.insert(pairCode.begin(), 0xbf800000u | static_cast<std::uint32_t>(partialFirst));
        auto full = request;
        full.shader.code = pairCode;
        auto limited = partial;
        limited.shader.code = pairCode;
        const auto fullReference = Recompile(full);
        const auto partialReference = Recompile(limited);
        ShaderPreparationContext preparation;
        const auto first = PrepareShader(partialFirst ? limited : full, &preparation);
        const auto second = PrepareShader(partialFirst ? full : limited, &preparation);
        const auto& fullHandle = partialFirst ? second : first;
        const auto& partialHandle = partialFirst ? first : second;
        const auto& fullArtifact = GetPreparedArtifact(*fullHandle);
        const auto& partialArtifact = GetPreparedArtifact(*partialHandle);
        require(first->source != second->source, "frontend pair: full and partial fixtures did not exercise separate sources");
        require(fullArtifact.spirv != partialArtifact.spirv, "frontend pair: full and partial threads share the same artifact");
        SrtRuntime runtime{};
        runtime.userData = buffer;
        runtime.shaderBase = full.shader.codeAddress;
        const auto fullCapture = CaptureResources(full, runtime, *fullHandle);
        const auto partialCapture = CaptureResources(limited, runtime, *partialHandle);
        const auto fullResult = MaterializeShader(full, *fullCapture, *fullHandle);
        const auto partialResult = MaterializeShader(limited, *partialCapture, *partialHandle);
        verifyResult(fullReference, *fullResult);
        verifyResult(partialReference, *partialResult);
    }

    const std::array<std::uint32_t, 1> color{0x3f800000u};
    for (const bool zeroFirst : {false, true}) {
        const std::vector<std::uint32_t> pixelCode{0xbf800000u | static_cast<std::uint32_t>(zeroFirst), 0x7e000200u, 0xf800180fu, 0u, 0xbf810000u};
        auto pixel = FrontendRequest(pixelCode, color);
        pixel.shader.stage = ShaderStage::Fragment;
        pixel.context.compute.reset();
        pixel.context.pixel = ShaderPixelStageInfo{};
        pixel.context.pixel->wave32 = true;
        pixel.context.pixel->targetOutputMode[0] = 9u;
        pixel.context.waveSize = 32u;
        auto zero = pixel;
        zero.layout.pushConstantSizeBytes = 0u;
        const auto pixelReference = Recompile(pixel);
        const auto zeroReference = Recompile(zero);
        ShaderPreparationContext preparation;
        const auto first = PrepareShader(zeroFirst ? zero : pixel, &preparation);
        const auto second = PrepareShader(zeroFirst ? pixel : zero, &preparation);
        const auto& pixelHandle = zeroFirst ? second : first;
        const auto& zeroHandle = zeroFirst ? first : second;
        require(first->source == second->source, "frontend pair: pixel capacities did not exercise the same source");
        require(GetPreparedArtifact(*pixelHandle).spirv != GetPreparedArtifact(*zeroHandle).spirv, "frontend pair: pixel fixture does not distinguish push-constant capacities");
        SrtRuntime runtime{};
        runtime.userData = color;
        runtime.shaderBase = pixel.shader.codeAddress;
        const auto pixelCapture = CaptureResources(pixel, runtime, *pixelHandle);
        const auto zeroCapture = CaptureResources(zero, runtime, *zeroHandle);
        verifyResult(pixelReference, *MaterializeShader(pixel, *pixelCapture, *pixelHandle));
        verifyResult(zeroReference, *MaterializeShader(zero, *zeroCapture, *zeroHandle));
    }
}

ShaderRecompiler::RecompileRequest LayoutPairRequest(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, bool registration, std::uint32_t wordLimit) {
    using namespace ShaderRecompiler;
    auto request = FrontendRequest(code, userData);
    request.shader.stage = ShaderStage::Fragment;
    request.context.compute.reset();
    request.context.pixel = ShaderPixelStageInfo{};
    request.context.pixel->wave32 = true;
    request.context.pixel->targetOutputMode[0] = 9u;
    request.context.waveSize = 32u;
    request.target.registrationModules = registration;
    request.target.registrationWordLimit = wordLimit;
    request.useCache = true;
    return request;
}

void verifyLayoutPairVariant(const ShaderRecompiler::SourceHandle& paired, const ShaderRecompiler::SourceHandle& isolated) {
    const auto& left = *paired.artifact;
    const auto& right = *isolated.artifact;
    require(left.artifact.spirv == right.artifact.spirv, "layout pair: the SPIR-V differs from an isolated compile");
    require(left.bindings.layout == right.bindings.layout && left.bindings.layout.UsesPushData() == right.bindings.layout.UsesPushData() && left.bindings.pushConstantOffsetBytes == right.bindings.pushConstantOffsetBytes && left.bindings.pushConstantSizeBytes == right.bindings.pushConstantSizeBytes, "layout pair: the binding layout or push range differs from an isolated compile");
    const auto& a = left.artifact;
    const auto& b = right.artifact;
    const auto sameParameter = [](const ShaderRecompiler::FragmentParameter& p, const ShaderRecompiler::FragmentParameter& q) {
        return p.location == q.location && p.sourceLocation == q.sourceLocation && p.flat == q.flat && p.perVertex == q.perVertex && p.custom == q.custom && p.flatP10 == q.flatP10;
    };
    require(a.vertexInputPatches == b.vertexInputPatches && a.memoryOffsetDword == b.memoryOffsetDword && a.shaderDataDwords == b.shaderDataDwords && a.imageMetadataDword == b.imageMetadataDword && a.runtimeImageCount == b.runtimeImageCount && a.runtimeImageResources == b.runtimeImageResources && a.registrationModule == b.registrationModule && a.bdaAbiVersion == b.bdaAbiVersion && a.runtimeAbiVersion == b.runtimeAbiVersion, "layout pair: the artifact's bindings or ABI differ from an isolated compile");
    require(a.vertexInputs == b.vertexInputs && a.vertexOffsetSgpr == b.vertexOffsetSgpr && a.instanceOffsetSgpr == b.instanceOffsetSgpr && a.vertexOffsetShared == b.vertexOffsetShared && a.instanceOffsetShared == b.instanceOffsetShared && a.vertexOffsetConflict == b.vertexOffsetConflict && a.instanceOffsetConflict == b.instanceOffsetConflict && a.hostSubgroupSize == b.hostSubgroupSize, "layout pair: the artifact's vertex interface differs from an isolated compile");
    require(a.parameterExports == b.parameterExports && std::ranges::equal(a.fragmentParameters, b.fragmentParameters, sameParameter) && a.barycentricEmulation.active == b.barycentricEmulation.active && a.barycentricEmulation.smooth == b.barycentricEmulation.smooth && a.barycentricEmulation.linear == b.barycentricEmulation.linear, "layout pair: the artifact's fragment interface differs from an isolated compile");
    const auto& x = left.info;
    const auto& y = right.info;
    require(x.stage == y.stage && x.shaderHash == y.shaderHash && x.waveSize == y.waveSize && x.userDataBase == y.userDataBase && x.userDataCount == y.userDataCount && x.scratchDwords == y.scratchDwords && x.paramExportMask == y.paramExportMask && x.info == y.info && x.bindings == y.bindings, "layout pair: the compiled shader information differs from an isolated compile");
}

void verifyLayoutPairIsolation(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle& handle, std::uint32_t separateLimit) {
    using namespace ShaderRecompiler;
    const std::vector<std::uint32_t> isolatedCode(request.shader.code.begin(), request.shader.code.end());
    auto isolated = request;
    isolated.shader.code = isolatedCode;
    isolated.target.registrationWordLimit = separateLimit;
    const auto reference = PrepareShader(isolated);
    require(reference->source != handle.source, "layout pair: the isolated compile shared a source");
    verifyLayoutPairVariant(handle, *reference);
    SrtRuntime runtime{};
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    auto uncached = request;
    uncached.useCache = false;
    verifyResult(Recompile(uncached), *MaterializeShader(request, *CaptureResources(request, runtime, handle), handle));
}

void verifyLayoutPair() {
    using namespace ShaderRecompiler;
    const bool reuse = std::getenv("APS5_NO_PERF_LAYOUT_PAIR") == nullptr;
    const std::array<std::uint32_t, 4> buffer{0x10000000u, 0u, 0x40u, 0x00027facu};
    const auto pixelCode = [](std::uint32_t variant) {
        return std::vector<std::uint32_t>{0xbf800000u | variant, 0xf4200100u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0xf800180fu, 0u, 0xbf810000u};
    };
    const auto interpolatedCode = [](std::uint32_t variant) {
        return std::vector<std::uint32_t>{0xbf800000u | variant, 0xc8100000u, 0xc8110001u, 0xc8140500u, 0xc8150501u, 0xf4200100u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0xf800180fu, 0x05040500u, 0xbf810000u};
    };
    const auto defaultLimit = SpirvTarget{}.registrationWordLimit;
    struct Configuration {
        bool registration;
        std::uint32_t wordLimit;
        std::uint32_t pushOffset;
        bool interpolated;
    };
    std::uint32_t variant = 0u;
    for (const auto& [registration, wordLimit, pushOffset, interpolated] : {Configuration{false, defaultLimit, 0u, false}, Configuration{true, defaultLimit, 0u, false}, Configuration{true, defaultLimit, 128u, false}, Configuration{true, 1u, 128u, false}, Configuration{true, defaultLimit, 128u, true}}) {
        for (const bool zeroFirst : {false, true}) {
            const auto code = interpolated ? interpolatedCode(variant++) : pixelCode(variant++);
            auto pixel = LayoutPairRequest(code, buffer, registration, wordLimit);
            pixel.layout.pushConstantOffsetBytes = pushOffset;
            if (interpolated) {
                pixel.context.pixel->interpolatorCount = 2u;
                pixel.context.pixel->interpolatorSettings[1] = 1u;
                pixel.context.pixel->inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
                pixel.context.pixel->hasPerspectiveCenterVgpr = true;
            }
            auto zero = pixel;
            zero.layout.pushConstantSizeBytes = 0u;
            ShaderPreparationContext preparation;
            const auto first = PrepareShader(zeroFirst ? zero : pixel, &preparation);
            const auto second = PrepareShader(zeroFirst ? pixel : zero, &preparation);
            require(first->source == second->source, "layout pair: the two layouts did not share a source");
            require(preparation.ResourcePrograms() == (reuse ? 1u : 2u), "layout pair: the second layout ignored the reuse switch");
            const auto& pixelHandle = zeroFirst ? second : first;
            const auto& zeroHandle = zeroFirst ? first : second;
            require(pixelHandle->artifact->bindings.layout.UsesPushData() && !zeroHandle->artifact->bindings.layout.UsesPushData(), "layout pair: the fixture does not distinguish the push-constant capacities");
            const bool module = registration && wordLimit == defaultLimit;
            require(GetPreparedArtifact(*pixelHandle).registrationModule == module && GetPreparedArtifact(*zeroHandle).registrationModule == module, "layout pair: the word limit did not choose the module mode");
            require(GetPreparedArtifact(*pixelHandle).fragmentParameters.size() == (interpolated ? 2u : 0u) && GetPreparedArtifact(*zeroHandle).fragmentParameters.size() == (interpolated ? 2u : 0u), "layout pair: the interpolated fixture did not declare its two parameters");
            verifyLayoutPairIsolation(pixel, *pixelHandle, wordLimit + 1u);
            verifyLayoutPairIsolation(zero, *zeroHandle, wordLimit + 2u);
        }
    }

    const auto code = pixelCode(variant++);
    const auto pixel = LayoutPairRequest(code, buffer, true, defaultLimit);
    auto relocated = pixel;
    relocated.shader.codeAddress += 0x1000u;
    relocated.layout.pushConstantSizeBytes = 0u;
    ShaderPreparationContext preparation;
    const auto first = PrepareShader(pixel, &preparation);
    const auto moved = PrepareShader(relocated, &preparation);
    require(first->source == moved->source, "layout pair: relocated code did not share the source");
    require(preparation.ResourcePrograms() == 2u && moved->artifact->info.shaderHash == relocated.shader.codeAddress, "layout pair: a program translated at another code address was reused");
    verifyLayoutPairIsolation(relocated, *moved, defaultLimit + 1u);

    const std::vector<std::uint32_t> colorCode{0xbf800000u | variant++, 0x7e000200u, 0xf800180fu, 0u, 0xbf810000u};
    auto color = LayoutPairRequest(colorCode, std::span(buffer).first(1u), true, defaultLimit);
    color.shader.codeAddress = relocated.shader.codeAddress;
    color.layout.pushConstantSizeBytes = 0u;
    require(ResolveSource(color) != nullptr, "layout pair: the other source was not resolved");
    const auto colored = PrepareShader(color, &preparation);
    require(colored->source != first->source && preparation.ResourcePrograms() == 2u, "layout pair: another source at the same code address reused the retained program");
    verifyLayoutPairIsolation(color, *colored, defaultLimit + 1u);

    const auto retained = colored->source.use_count();
    const auto otherCode = pixelCode(variant++);
    static_cast<void>(PrepareResourceProgram(LayoutPairRequest(otherCode, buffer, true, defaultLimit), &preparation));
    require(preparation.ResourcePrograms() == 3u && retained - colored->source.use_count() == (reuse ? 1 : 0), "layout pair: the context kept a program while it built another");

    const std::vector<std::uint32_t> computeCode{0xbf800000u, 0xbf810000u};
    auto compute = FrontendRequest(computeCode, buffer);
    compute.context.userDataBaseRegister = 2u;
    compute.context.compute->scratchDwords = 64u;
    auto program = PrepareResourceProgram(compute);
    const auto copied = program.CompiledInfo();
    const auto taken = std::move(program).TakeCompiledInfo();
    require(taken.userDataBase == 2u && taken.scratchDwords == 64u, "layout pair: the compute fixture did not set a user-data base and scratch");
    require(copied.stage == taken.stage && copied.shaderHash == taken.shaderHash && copied.waveSize == taken.waveSize && copied.userDataBase == taken.userDataBase && copied.userDataCount == taken.userDataCount && copied.scratchDwords == taken.scratchDwords && copied.paramExportMask == taken.paramExportMask && copied.info == taken.info && copied.bindings == taken.bindings, "layout pair: the copied compiled information differs from the moved one");
}

void verifyRegisterSources() {
    using namespace ShaderRecompiler;
    IrResourcePlan plan;
    IrValue samplerRegister(IrOpcode::Void, IrType::ScalarReg, 0);
    IrValue bufferRegister(IrOpcode::Void, IrType::ScalarReg, 1);
    IrValue sameSamplerRegister(IrOpcode::Void, IrType::ScalarReg, 2);
    samplerRegister.SetRegister({RegisterBank::Scalar, 8});
    bufferRegister.SetRegister({RegisterBank::Scalar, 12});
    sameSamplerRegister.SetRegister({RegisterBank::Scalar, 8});
    IrValue samplerRead(IrOpcode::GetUserData, IrType::U32, 3);
    IrValue bufferRead(IrOpcode::GetUserData, IrType::U32, 4);
    IrValue sameSamplerRead(IrOpcode::GetUserData, IrType::U32, 5);
    samplerRead.AddArgument(&samplerRegister);
    bufferRead.AddArgument(&bufferRegister);
    sameSamplerRead.AddArgument(&sameSamplerRegister);
    require(!EquivalentValue(plan, &samplerRead, &bufferRead), "sampler SGPRs were merged with buffer SGPRs");
    require(EquivalentValue(plan, &samplerRead, &sameSamplerRead), "identical user data reads were not recognized");
    sameSamplerRegister.SetRegister({RegisterBank::UserData, 8});
    require(!EquivalentValue(plan, &samplerRead, &sameSamplerRead), "different register banks were merged");
    IrValue firstVector(IrOpcode::Void, IrType::VectorReg, 6);
    IrValue secondVector(IrOpcode::Void, IrType::VectorReg, 7);
    firstVector.SetRegister({RegisterBank::Vector, 0});
    secondVector.SetRegister({RegisterBank::Vector, 1});
    require(!EquivalentValue(plan, &firstVector, &secondVector), "different vector registers were merged");
    require(!EquivalentValue(plan, &samplerRegister, &firstVector), "different register types were merged");
}

void verifySignedMinimum() {
    using namespace ShaderRecompiler;
    IrResourcePlan plan;
    IrValue reg(IrOpcode::Void, IrType::ScalarReg, 0);
    reg.SetRegister({RegisterBank::Scalar, 0});
    IrValue input(IrOpcode::GetUserData, IrType::U32, 1);
    input.AddArgument(&reg);
    IrValue limit(IrOpcode::Void, IrType::U32, 2);
    limit.SetImmediateU32(8);
    IrValue minimum(IrOpcode::SMin32, IrType::U32, 3);
    minimum.AddArgument(&input);
    minimum.AddArgument(&limit);
    require(Detail::IsRuntimeUniformOp(IrOpcode::SMin32), "signed minimum must be a runtime uniform operation");
    for (const auto& [value, expected] : std::array<std::pair<std::uint32_t, std::uint32_t>, 6>{{{0, 0}, {4, 4}, {20, 8}, {0x7fffffffu, 8}, {0xffffffffu, 0xffffffffu}, {0x80000000u, 0x80000000u}}}) {
        std::array<std::uint32_t, 1> data{value};
        SrtRuntime runtime{data};
        Detail::Evaluator evaluator(plan, runtime);
        std::uint32_t result = 0;
        require(evaluator.Evaluate(Detail::IrNode(&minimum), result) && result == expected, "SRT signed minimum used unsigned ordering or failed evaluation");
    }
}

void verifyMayEvaluate() {
    using namespace ShaderRecompiler;
    IrResourcePlan plan;
    std::vector<std::unique_ptr<IrValue>> values;
    const auto make = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments = {}) -> IrValue& {
        values.push_back(std::make_unique<IrValue>(opcode, type, static_cast<std::uint32_t>(values.size())));
        for (auto* argument : arguments) values.back()->AddArgument(argument);
        return *values.back();
    };
    const auto constant = [&](std::uint32_t value) -> IrValue& {
        auto& leaf = make(IrOpcode::Void, IrType::U32);
        leaf.SetImmediateU32(value);
        return leaf;
    };
    auto& scalar = make(IrOpcode::Void, IrType::ScalarReg);
    scalar.SetRegister({RegisterBank::Scalar, 0});
    auto& vector = make(IrOpcode::Void, IrType::VectorReg);
    vector.SetRegister({RegisterBank::Vector, 0});
    auto& input = make(IrOpcode::GetUserData, IrType::U32, {&scalar});
    auto& lane = make(IrOpcode::GetRegister, IrType::U32, {&vector});
    auto& uniform = make(IrOpcode::IEqual32, IrType::Bool, {&input, &constant(4)});
    auto& varying = make(IrOpcode::IEqual32, IrType::Bool, {&input, &lane});
    auto& select = make(IrOpcode::SelectU32, IrType::U32, {&uniform, &constant(1), &lane});
    auto& unevaluated = make(IrOpcode::FPAdd32, IrType::F32, {&input, &input});
    auto& invariant = make(IrOpcode::Phi, IrType::Bool);
    invariant.AddArgument(&uniform);
    invariant.AddArgument(&uniform);
    auto& divergent = make(IrOpcode::Phi, IrType::Bool);
    divergent.AddArgument(&uniform);
    divergent.AddArgument(&varying);
    auto& counter = make(IrOpcode::Phi, IrType::U32);
    auto& next = make(IrOpcode::CompositeExtractU32x2, IrType::U32, {&make(IrOpcode::IAddCarry32, IrType::U32x2, {&counter, &constant(1)}), &constant(0)});
    counter.AddArgument(&constant(0));
    counter.AddArgument(&next);
    auto& latch = make(IrOpcode::ULessThan32, IrType::Bool, {&next, &constant(4)});
    auto& sum = make(IrOpcode::CompositeExtractU32x2, IrType::U32, {&make(IrOpcode::IAddCarry32, IrType::U32x2, {&input, &constant(1)}), &constant(0)});
    std::array<std::uint32_t, 1> data{4};
    SrtRuntime runtime{data};
    for (const auto& [value, expected, name] : std::array<std::tuple<IrValue*, bool, const char*>, 11>{{
             {&constant(3), true, "a constant"},
             {&input, true, "user data"},
             {&uniform, true, "a comparison of user data"},
             {&lane, false, "a vector register read"},
             {&varying, false, "a comparison with a vector register"},
             {&select, false, "a select with a vector register arm"},
             {&unevaluated, false, "an opcode the evaluator does not evaluate"},
             {&invariant, true, "a phi whose operands agree"},
             {&divergent, false, "a phi whose operands differ"},
             {&latch, false, "a scalar loop counter's latch condition"},
             {&sum, true, "the low word of a carry add of user data"},
         }}) {
        require(Detail::Evaluator::MayEvaluate(plan, value) == expected, (std::string("MayEvaluate misjudged ") + name).c_str());
        Detail::Evaluator evaluator(plan, runtime);
        std::uint32_t result = 0;
        require(evaluator.Evaluate(Detail::IrNode(value), result) == expected, (std::string("MayEvaluate disagreed with the evaluator on ") + name).c_str());
    }
    const std::array<IrOpcode, 9> takenAsEvaluable{IrOpcode::Identity, IrOpcode::Phi, IrOpcode::Void, IrOpcode::GetUserData, IrOpcode::GetShaderBase, IrOpcode::ReadFirstLane, IrOpcode::ReadConst, IrOpcode::LoadAddressU32, IrOpcode::ReadConstBuffer};
    for (std::uint16_t raw = 0; raw < static_cast<std::uint16_t>(IrOpcode::Count); ++raw) {
        const auto opcode = static_cast<IrOpcode>(raw);
        if (std::ranges::find(takenAsEvaluable, opcode) != takenAsEvaluable.end()) continue;
        auto& value = make(opcode, IrType::U32, {&constant(0), &constant(0), &constant(0), &constant(0)});
        Detail::Evaluator evaluator(plan, runtime);
        std::uint32_t result = 0;
        require(Detail::Evaluator::MayEvaluate(plan, &value) == evaluator.Evaluate(Detail::IrNode(&value), result), (std::string("MayEvaluate disagreed with the evaluator on ") + std::string(IrOpcodeName(opcode)) + " over constants").c_str());
    }
}

void verifyEvaluatedValues() {
    using namespace ShaderRecompiler;
    std::vector<std::unique_ptr<IrValue>> values;
    for (std::uint32_t id = 0; id < 1000u; id++) {
        values.push_back(std::make_unique<IrValue>(IrOpcode::Void, IrType::U32, id));
    }
    Detail::EvaluatedValues table;
    std::uint64_t found = 0;
    require(!table.Find(values.front().get(), found), "evaluated values: an empty table found a value");
    for (std::uint32_t id = 0; id < values.size(); id++) {
        table.Insert(values[id].get(), std::uint64_t{id} * 3u);
    }
    for (std::uint32_t id = 0; id < values.size(); id++) {
        require(table.Find(values[id].get(), found) && found == std::uint64_t{id} * 3u, "evaluated values: a value was lost when the table grew");
    }
    table.Insert(values[7].get(), 0u);
    require(table.Find(values[7].get(), found) && found == 21u, "evaluated values: a second insert replaced the first value");
    IrValue absent(IrOpcode::Void, IrType::U32, 1000u);
    require(!table.Find(&absent, found), "evaluated values: a value that was never inserted was found");
}

void verifySignedSrtComparison() {
    using namespace ShaderRecompiler;
    IrResourcePlan plan;
    plan.srtPlanComplete = true;
    plan.userDataBase = 8;
    std::vector<std::unique_ptr<IrValue>> values;
    std::uint32_t id = 0;
    const auto make = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments = {}) -> IrValue& {
        auto value = std::make_unique<IrValue>(opcode, type, id++);
        for (auto* argument : arguments) value->AddArgument(argument);
        values.push_back(std::move(value));
        return *values.back();
    };
    const auto constant = [&](std::uint32_t bits) -> IrValue& {
        auto& value = make(IrOpcode::Void, IrType::U32);
        value.SetImmediateU32(bits);
        return value;
    };
    auto& registerValue = make(IrOpcode::Void, IrType::ScalarReg);
    registerValue.SetRegister({RegisterBank::Scalar, 8u});
    auto& userData = make(IrOpcode::GetUserData, IrType::U32, {&registerValue});
    auto& zero = constant(0u);
    auto& intMin = constant(0x80000000u);
    auto& intMax = constant(0x7fffffffu);
    auto& negative = constant(0xau);
    auto& nonnegative = constant(0xbu);
    auto& greaterThanMin = constant(0xcu);
    auto& equalMin = constant(0xdu);
    auto& belowMax = constant(0xeu);
    auto& equalMax = constant(0xfu);
    auto& isNegative = make(IrOpcode::SLessThan32, IrType::Bool, {&userData, &zero});
    auto& aboveMin = make(IrOpcode::SLessThan32, IrType::Bool, {&intMin, &userData});
    auto& belowMaxValue = make(IrOpcode::SLessThan32, IrType::Bool, {&userData, &intMax});
    DescriptorSource source;
    source.dwordCount = 3u;
    source.dwords[0] = &make(IrOpcode::SelectU32, IrType::U32, {&isNegative, &negative, &nonnegative});
    source.dwords[1] = &make(IrOpcode::SelectU32, IrType::U32, {&aboveMin, &greaterThanMin, &equalMin});
    source.dwords[2] = &make(IrOpcode::SelectU32, IrType::U32, {&belowMaxValue, &belowMax, &equalMax});
    plan.descriptorSources.push_back(source);
    for (std::uint32_t dword = 0; dword < source.dwordCount; ++dword) {
        require(SrtWalker().ValidateRuntimeValue(plan, source.dwords[dword]), "signed SRT comparison: the runtime validator rejects a descriptor dword the evaluator computes");
    }
    const std::array<std::uint32_t, 1> sourceIndex{0u};
    const std::array<std::pair<std::uint32_t, std::array<std::uint32_t, 3>>, 4> cases{{
        {0x80000000u, {0xau, 0xdu, 0xeu}},
        {0xffffffffu, {0xau, 0xcu, 0xeu}},
        {0u, {0xbu, 0xcu, 0xeu}},
        {0x7fffffffu, {0xbu, 0xcu, 0xfu}},
    }};
    std::vector<DescriptorValue> descriptors;
    std::vector<std::uint32_t> flat;
    std::vector<std::uint8_t> active;
    for (const auto& [input, expected] : cases) {
        const std::array<std::uint32_t, 1> userDataWords{input};
        const SrtRuntime runtime{userDataWords};
        require(Detail::EvaluateRuntimeSourcesImpl(plan, sourceIndex, runtime, descriptors, flat, false, {}, active), "signed SRT comparison: descriptor expression could not be evaluated");
        require(descriptors.size() == 1u && descriptors[0].dwordCount == expected.size(), "signed SRT comparison: descriptor shape changed");
        for (std::size_t index = 0; index < expected.size(); ++index) {
            require(descriptors[0].dwords[index] == expected[index], "signed SRT comparison: signed boundary selected the wrong descriptor dword");
        }
    }
}

void verifyTypeZeroImageMaterialization() {
    using namespace ShaderRecompiler;
    for (const std::uint32_t type : {0u, 1u}) {
        IrResourcePlan plan;
        plan.srtPlanComplete = true;
        plan.resourceTrackingComplete = true;
        plan.userDataCount = 0u;
        plan.materializationSources = {0u};

        const std::array<std::uint32_t, 8> words{0x12345678u, (56u << 20u) | 0x100u, 0x0000c000u, 0x00000facu | (type << 28u), 1u, 2u, 3u, 4u};
        std::vector<std::unique_ptr<IrValue>> values;
        DescriptorSource source;
        source.dwordCount = static_cast<std::uint32_t>(words.size());
        for (std::uint32_t i = 0u; i < words.size(); ++i) {
            auto value = std::make_unique<IrValue>(IrOpcode::Void, IrType::U32, i);
            value->SetImmediateU32(words[i]);
            source.dwords[i] = value.get();
            values.push_back(std::move(value));
        }
        plan.descriptorSources.push_back(source);

        ImageResource image;
        image.source = 0u;
        image.resourceClass = ImageResourceClass::Sampled;
        image.dimension = RdnaImageDimension::Dim2D;
        image.read = true;
        plan.info.images.push_back(image);
        plan.info.runtimeImageModes.push_back(ResourceMaterializer::RuntimeImageModes(image));

        ResourceSnapshot snapshot;
        ResourceMaterializer{}.Materialize(plan, SrtRuntime{}, snapshot);
        require(snapshot.images.size() == 1u && snapshot.images[0].dwordCount == 8u && snapshot.images[0].dwords == words,
                type == 0u ? "TYPE=0 image descriptor with a nonzero base was rejected or altered during materialization" : "TYPE=1 image descriptor with a nonzero base was rejected or altered during materialization");

        for (const bool atomic : {false, true}) {
            image.resourceClass = ImageResourceClass::Storage;
            image.read = false;
            image.written = !atomic;
            image.atomic = atomic;
            plan.info.images[0] = image;
            plan.info.runtimeImageModes[0] = ResourceMaterializer::RuntimeImageModes(image);
            expectFailure([&] { ResourceMaterializer{}.Materialize(plan, SrtRuntime{}, snapshot); }, "runtime image descriptor is invalid",
                          atomic ? "TYPE=0 or 1 atomic image descriptor was accepted without hardware evidence" : "TYPE=0 or 1 writable image descriptor was accepted without hardware evidence");
        }
    }
}

void verifyOutOfRangeConstantReads() {
    std::vector<std::uint32_t> code{0xf40c0200u, 0xfa000000u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf4000400u, 0xfa000050u, 0xf4080a00u, 0xfa000080u, 0xbf8cc07fu, 0xf42c0814u, 0xfa000000u, 0xbf8cc07fu, 0xbf068010u, 0xbf850008u};
    for (std::uint32_t dword = 0; dword < 8u; dword++) {
        code.push_back(0xbe800300u | ((8u + dword) << 16u) | (32u + dword));
    }
    code.insert(code.end(), {0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u});
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    const auto imageDescriptor = [&](const Texture& texture) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    std::array<std::uint32_t, 36> srt{};
    const auto first = imageDescriptor(textures[0]);
    const auto second = imageDescriptor(textures[1]);
    std::copy(first.begin(), first.end(), srt.begin());
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
    srt[20] = 1u;
    std::copy(second.begin(), second.end(), srt.begin() + 24);
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x23000u, code, 0, {}};
    request.context.waveSize = 32u;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 2u && plan->info.samplers.size() == 1u, "out-of-range s_buffer_load: the two T#s were not planned as one image each");
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    request.context.memory = memory.Regions();
    static_cast<void>(Recompile(request, *capture));
    const auto holds = [&](const std::array<std::uint32_t, 8>& words) {
        return std::ranges::any_of(capture->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(holds(first) && holds(std::array<std::uint32_t, 8>{}), "out-of-range s_buffer_load: a T# read through a null V# was not evaluated as zero");
}

// The pure flat slots of a hand-built plan (Detail::ComputePureFlatSlots): a slot is pure unless
// a descriptor dword, a condition, a uniform value or another slot's address cone reaches it.
void verifyPureFlatSlots() {
    using namespace ShaderRecompiler;
    std::vector<std::unique_ptr<IrValue>> values;
    std::uint32_t ids = 0;
    const auto make = [&](IrOpcode opcode, IrType type) -> IrValue& {
        values.push_back(std::make_unique<IrValue>(opcode, type, ids++));
        return *values.back();
    };
    const auto constant = [&](std::uint32_t value) -> IrValue& {
        auto& immediate = make(IrOpcode::Void, IrType::U32);
        immediate.SetImmediateU32(value);
        return immediate;
    };
    auto& resource = make(IrOpcode::GetSrtResource, IrType::SrtResource);
    const auto userData = [&](std::uint32_t index) -> IrValue& {
        auto& reg = make(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister({RegisterBank::Scalar, index});
        auto& read = make(IrOpcode::GetUserData, IrType::U32);
        read.AddArgument(&reg);
        return read;
    };
    const auto handle = [&](IrValue& low, IrValue& high) -> IrValue& {
        auto& composed = make(IrOpcode::CompositeConstructU64, IrType::U64);
        composed.AddArgument(&low);
        composed.AddArgument(&high);
        return composed;
    };
    const auto rawRead = [&](IrValue& address, std::uint32_t offset) -> IrValue& {
        auto& read = make(IrOpcode::LoadAddressU32, IrType::U32);
        read.AddArgument(&address);
        read.AddArgument(&constant(offset));
        return read;
    };
    const auto readConst = [&](std::uint32_t slot) -> IrValue& {
        auto& read = make(IrOpcode::ReadConst, IrType::U32);
        read.AddArgument(&resource);
        read.AddArgument(&constant(slot));
        return read;
    };
    // Slots: A (0) reads through a pointer B (1) read from user data; C (2) and D (3) read from
    // user-data handles; a descriptor source consumes C.
    auto& b = rawRead(handle(userData(0), userData(1)), 0);
    auto& c = rawRead(handle(userData(2), userData(3)), 4);
    auto& a = rawRead(handle(readConst(1), userData(4)), 8);
    auto& d = rawRead(handle(userData(5), userData(6)), 12);
    IrResourcePlan plan;
    plan.srtPlanComplete = true;
    plan.resourceTrackingComplete = true;
    plan.srtReads = {{&a, 0}, {&b, 1}, {&c, 2}, {&d, 3}};
    DescriptorSource source;
    source.dwordCount = 1;
    source.dwords[0] = &readConst(2);
    plan.descriptorSources.push_back(source);
    using Pure = std::vector<std::uint8_t>;
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 0, 1}, "pure flat slots: the address cone or the descriptor source was not excluded");
    plan.controlFlow.push_back({&readConst(3), {}, {}});
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 0, 0}, "pure flat slots: a control-flow condition was not excluded");
    plan.controlFlow.clear();
    auto& phi = make(IrOpcode::Phi, IrType::U32);
    phi.AddArgument(&readConst(0));
    phi.AddArgument(&constant(0));
    plan.descriptorSources[0].dwords[0] = &phi;
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 1, 1}, "pure flat slots: a phi argument was not excluded");
    plan.descriptorSources[0].dwords[0] = &b;
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 1, 1}, "pure flat slots: a raw read named directly was not excluded");
    plan.descriptorSources[0].dwords[0] = &readConst(2);
    plan.uniformFill.fill.kind = UniformFillKind::Buffer;
    plan.uniformFill.fill.words = 1;
    plan.uniformFill.values[0] = &readConst(3);
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 0, 0}, "pure flat slots: a uniform-fill value was not excluded");
    plan.uniformFill = {};
    plan.descriptorSources[0].tableColumn = TableColumn{};
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 0, 0}, "pure flat slots: a table column did not disqualify the plan");
    plan.descriptorSources[0].tableColumn.reset();
    plan.requiresSpecializationMemory = true;
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 0, 0}, "pure flat slots: specialization memory did not disqualify the plan");
    plan.requiresSpecializationMemory = false;
    plan.srtPlanComplete = false;
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 0, 0}, "pure flat slots: an incomplete plan was classified");
}

void verifyGuardedDescriptorSources() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const std::array<std::uint32_t, 8> valid{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 8> invalid{0x12345678u, 0xffffffffu, 0xffffffffu, 2u << 28u, 0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 4> sampler{0u, 0u, 0u, 0u};
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::array<std::uint32_t, 32> srt{};
    const auto fill = [&](const std::array<std::uint32_t, 8>& image, std::uint32_t mode) {
        std::copy(image.begin(), image.end(), srt.begin());
        std::copy(sampler.begin(), sampler.end(), srt.begin() + 8);
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
        srt[20] = mode;
    };
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    const std::vector<std::uint32_t> code{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xbf068010u, 0xbf850006u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    request.shader = {ShaderStage::Compute, 0x22000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;

    fill(invalid, 0u);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.samplers.size() == 1u, "guarded sources: the image and its sampler were not planned");
    require(!plan->controlFlow.empty(), "guarded sources: the branch around the image was not planned");
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::ranges::all_of(capture->snapshot.images[0].dwords, [](std::uint32_t word) { return word == 0u; }), "guarded sources: the skipped T# was not left null");
    }
    fill(invalid, 1u);
    {
        AgcDriver::ShaderMemory memory({});
        expectFailure([&] { static_cast<void>(memory.Capture(request)); }, "runtime image descriptor is invalid", "guarded sources: an invalid T# behind a taken branch was accepted");
    }
    auto typeOne = valid;
    typeOne[3] = 0xfacu | (1u << 28u);
    fill(typeOne, 1u);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::equal(typeOne.begin(), typeOne.end(), capture->snapshot.images[0].dwords.begin()), "guarded sources: a read-only TYPE 1 T# behind a taken branch was refused or altered");
    }
    fill(valid, 1u);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::equal(valid.begin(), valid.end(), capture->snapshot.images[0].dwords.begin()), "guarded sources: the T# behind a taken branch was not read");
    }
    fill(valid, 0u);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::ranges::all_of(capture->snapshot.images[0].dwords, [](std::uint32_t word) { return word == 0u; }), "guarded sources: a valid T# behind a skipped branch was read");
    }

    const std::vector<std::uint32_t> storeCode{0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850006u, 0xf40c0100u, 0xfa000000u, 0xbf8cc07fu, 0xf0200f08u, 0x00010004u, 0xbf820005u, 0xf40c0100u, 0xfa000000u, 0xbf8cc07fu, 0xf0200f10u, 0x00010004u, 0xbf810000u};
    auto storeRequest = request;
    storeRequest.shader = {ShaderStage::Compute, 0x23000u, storeCode, 0, {}};
    const auto compileStore = [&](std::uint32_t mode) {
        fill(valid, mode);
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(storeRequest);
        storeRequest.context.memory = memory.Regions();
        return Recompile(storeRequest, *capture);
    };
    const auto storePlan = GetResourcePlan(storeRequest);
    require(storePlan->info.images.size() == 2u && storePlan->descriptorSources.size() == 1u, "guarded images: the two stores were not planned as two images of one T#");
    static_cast<void>(compileStore(1u));
    expectFailure([&] { static_cast<void>(compileStore(0u)); }, "incompatible with the static runtime image interface", "guarded images: a store of the wrong dimension behind a taken branch was accepted");
}

struct alignas(256) TableTexture {
    std::array<std::uint8_t, 256> bytes{};
};

std::array<std::uint32_t, 8> TableT(const void* base, std::uint32_t format, std::uint32_t type, std::uint32_t height = 3u, std::uint32_t swizzleMode = 0u) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (3u << 30u), height << 14u, 0xfacu | (swizzleMode << 20u) | (type << 28u), 0u, 0u, 0u, 0u};
}

std::array<std::uint32_t, 4> TableV(const void* base, std::uint32_t stride, std::uint32_t records) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

bool TableCovered(const std::vector<ShaderRecompiler::MemoryRegion>& regions, const void* pointer, std::size_t bytes) {
    auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    const auto end = address + bytes;
    while (address < end) {
        const auto region = std::find_if(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& candidate) { return address >= candidate.guestAddress && address < candidate.guestAddress + candidate.bytes.size(); });
        if (region == regions.end()) return false;
        address = region->guestAddress + region->bytes.size();
    }
    return true;
}

const ShaderRecompiler::DescriptorBinding* TableBinding(const ShaderRecompiler::RecompileResult& result, ShaderRecompiler::DescriptorRole role) {
    const auto found = std::find_if(result.bindings.begin(), result.bindings.end(), [&](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == role; });
    return found == result.bindings.end() ? nullptr : &*found;
}

const std::vector<std::uint32_t>& TableMap(const ShaderRecompiler::RecompileResult& result) {
    const auto* binding = TableBinding(result, ShaderRecompiler::DescriptorRole::ImageTableMap);
    require(binding != nullptr && binding->guestDescriptor.size() >= ShaderRecompiler::ImageTableAbi::HeaderWords, "image table: the result has no map");
    return binding->guestDescriptor;
}

std::vector<std::uint32_t> TableCodes(const std::vector<std::uint32_t>& map, std::uint32_t table) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    const auto header = Abi::TableHeader(table);
    const auto start = map.at(header + Abi::TableMapStart);
    const auto keys = map.at(header + Abi::TableKeyCount);
    require(static_cast<std::size_t>(start) + keys <= map.size(), "image table: the map is truncated");
    return std::vector<std::uint32_t>(map.begin() + start, map.begin() + start + keys);
}

struct TableSpirvScan {
    bool runtimeArray = false;
    bool dynamicIndexing = false;
    bool shaderNonUniform = false;
    bool nonUniform = false;
    bool switched = false;
};

TableSpirvScan ScanTableSpirv(std::span<const std::uint32_t> words) {
    TableSpirvScan result;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        require(count != 0 && count <= words.size() - cursor, "image table: truncated SPIR-V instruction");
        const auto op = words[cursor] & 0xffffu;
        if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilityRuntimeDescriptorArray) result.runtimeArray = true;
        if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilitySampledImageArrayDynamicIndexing) result.dynamicIndexing = true;
        if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilityShaderNonUniform) result.shaderNonUniform = true;
        if (op == spv::OpDecorate && words[cursor + 2] == spv::DecorationNonUniform) result.nonUniform = true;
        if (op == spv::OpSwitch) result.switched = true;
        cursor += count;
    }
    return result;
}

struct TableGuest {
    std::array<std::array<std::uint32_t, 8>, 128> heap{};
    std::array<std::array<std::uint32_t, 4>, 3> materials{};
    std::array<std::uint32_t, 4> output{};
    std::array<std::uint32_t, 20> srt{};
};

constexpr std::uint32_t TableFormat8888UNorm = 56;
constexpr std::uint32_t TableFormat32UInt = 20;
constexpr std::uint32_t TableFormat16SInt = 12;
constexpr std::uint32_t TableFormat11_11_10UInt = 34;
constexpr std::uint32_t TableType2D = 9;
constexpr std::uint32_t TableType3D = 10;

const std::vector<std::uint32_t> TableMaterialCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x93109010u, 0xf4200406u, 0x20000004u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
const std::vector<std::uint32_t> TableSrtKeyCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0xf4000400u, 0xfa000040u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
const std::vector<std::uint32_t> TableTwoKeyCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0xf4000400u, 0xfa000040u, 0xf4000440u, 0xfa000044u, 0x8f108510u, 0x8f118511u, 0xf42c0502u, 0x20000000u, 0xf42c0902u, 0x22000000u, 0xf09c0f08u, 0x00450000u, 0xf09c0f08u, 0x00490400u, 0x06000900u, 0xe0700000u, 0x80070000u, 0xbf810000u};
const std::vector<std::uint32_t> TableMixedKeyCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0xf4000400u, 0xfa000040u, 0x34020084u, 0xbf8cc07fu, 0xe0301004u, 0x80030101u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xbf8c3f70u, 0x7e220501u, 0x8f118511u, 0xf42c0902u, 0x22000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00450000u, 0xf09c0f08u, 0x00490400u, 0xbf8c3f70u, 0x06000900u, 0xe0700000u, 0x80070000u, 0xbf810000u};
const std::vector<std::uint32_t> TableLateKeyCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0xf4000400u, 0xfa000040u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xf4000440u, 0xfa000044u, 0x8f118511u, 0xf42c0902u, 0x22000000u, 0xf09c0f08u, 0x00490400u, 0x06000900u, 0xe0700000u, 0x80070000u, 0xbf810000u};
const std::vector<std::uint32_t> TableMixedKeyFirstCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0xf4000400u, 0xfa000040u, 0x34020084u, 0xbf8cc07fu, 0xe0301004u, 0x80030101u, 0xbf8c3f70u, 0x7e220501u, 0x8f118511u, 0xf42c0902u, 0x22000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00490400u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00450000u, 0xbf8c3f70u, 0x06000900u, 0xe0700000u, 0x80070000u, 0xbf810000u};
const std::vector<std::uint32_t> TableWholeCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};

std::vector<std::uint32_t> TableLoadCode(std::vector<std::uint32_t> code) {
    const auto sample = std::find(code.begin(), code.end(), 0xf09c0f08u);
    require(sample != code.end() && sample + 1 != code.end(), "image table: the program has no image_sample_lz");
    sample[0] = 0xf0000f08u;
    sample[1] = 0x00050000u;
    return code;
}

ShaderRecompiler::RecompileRequest TableRequest(const std::vector<std::uint32_t>& code, const std::array<std::uint32_t, 2>& userData, std::uint32_t subgroupSize = 32u) {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    static constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto word : code) hash = (hash ^ word) * 0x100000001b3ull;
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x100000u + (hash & 0xffffu) * 0x100u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = subgroupSize;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    return request;
}

void FillTableSrt(TableGuest& guest, const void* heap, std::uint32_t stride, std::uint32_t records) {
    const auto heapV = TableV(heap, stride, records);
    const auto materialV = TableV(guest.materials.data(), 16u, 3u);
    const auto outputV = TableV(guest.output.data(), 0u, 16u);
    std::copy(heapV.begin(), heapV.end(), guest.srt.begin());
    std::fill(guest.srt.begin() + 4, guest.srt.begin() + 8, 0u);
    std::copy(materialV.begin(), materialV.end(), guest.srt.begin() + 8);
    std::copy(outputV.begin(), outputV.end(), guest.srt.begin() + 12);
}

std::array<std::uint32_t, 2> TableUserData(const TableGuest& guest) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.srt.data()));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
}

struct TableCompiled {
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    std::vector<ShaderRecompiler::MemoryRegion> regions;
    std::vector<std::byte> key;
};

TableCompiled CompileTable(ShaderRecompiler::RecompileRequest request) {
    TableCompiled out;
    AgcDriver::ShaderMemory memory({});
    out.capture = memory.Capture(request);
    out.regions = memory.Regions();
    request.context.memory = out.regions;
    out.result = Recompile(request, *out.capture);
    ShaderRecompiler::ShaderDiskCache::BuildKey(request, request.target.subgroupSize, out.key);
    return out;
}

void verifyImageTableKeys() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::uint32_t Entries = 3000;
    static TableTexture textures[4];
    alignas(4096) static TableGuest guest;
    alignas(4096) static std::array<std::array<std::uint32_t, 8>, Entries> large{};
    std::fill(guest.heap.begin(), guest.heap.end(), std::array<std::uint32_t, 8>{});
    guest.heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    guest.heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    large[0] = TableT(textures[2].bytes.data(), TableFormat16SInt, TableType2D);
    large[1] = TableT(textures[2].bytes.data(), TableFormat11_11_10UInt, TableType2D);
    large[2] = TableT(textures[3].bytes.data(), TableFormat32UInt, TableType2D, 3u, 4u);
    large[3] = TableT(textures[3].bytes.data(), TableFormat8888UNorm, TableType3D);
    large[4] = {0x1234u, 0x5678u, 0x9abcu, 0x1000ffacu, 0u, 0u, 0u, 0u};
    large[5] = TableT(textures[3].bytes.data(), 0u, TableType2D);
    for (std::uint32_t entry = 6; entry < Entries; entry++) large[entry] = TableT(textures[entry % 4u].bytes.data(), TableFormat8888UNorm, TableType2D, entry);
    const auto userData = TableUserData(guest);
    for (const auto& code : {TableMaterialCode, TableWholeCode, TableLoadCode(TableMaterialCode), TableLoadCode(TableWholeCode)}) {
        const auto request = TableRequest(code, userData);
        guest.materials = {{{0u, 1u, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 2u, 0u, 0u}}};
        FillTableSrt(guest, guest.heap.data(), 32u, 3u);
        const auto first = CompileTable(request);
        guest.materials = {{{0u, 3u, 0u, 0u}, {0u, 4u, 0u, 0u}, {0u, 2999u, 0u, 0u}}};
        FillTableSrt(guest, large.data(), 32u, Entries);
        const auto second = CompileTable(request);
        const auto& a = *first.result;
        const auto& b = *second.result;
        require(a.variantId == b.variantId, "image table keys: the table contents changed the compiled artifact");
        require(a.PipelineVariantId() == b.PipelineVariantId() && a.specializationId == b.specializationId && a.specialization == b.specialization, "image table keys: the table contents changed the specialization");
        require(a.spirv == b.spirv, "image table keys: the table contents changed the module");
        require(first.key == second.key, "image table keys: the table contents changed the cache key");
        require(TableMap(a) != TableMap(b), "image table keys: the maps do not differ");
        const auto* tableA = TableBinding(a, DescriptorRole::ImageTable);
        const auto* tableB = TableBinding(b, DescriptorRole::ImageTable);
        require(tableA != nullptr && tableB != nullptr && tableA->binding == tableB->binding && tableA->guestDescriptor != tableB->guestDescriptor, "image table keys: the image table bindings are missing or equal");
        require(a.bindings.size() == b.bindings.size(), "image table keys: the table contents changed the binding list");
        for (std::size_t index = 0; index < a.bindings.size(); index++) {
            require(a.bindings[index].role == b.bindings[index].role && a.bindings[index].binding == b.bindings[index].binding && a.bindings[index].kind == b.bindings[index].kind, "image table keys: the table contents changed a binding");
        }
        const auto& mapB = TableMap(b);
        require(mapB[Abi::HeaderTableCount] == 1u && mapB[Abi::TableHeader(0) + Abi::TableKeyCount] == Entries, "image table keys: the large table was not snapshotted whole");
        if (code.size() == TableWholeCode.size()) require(mapB[Abi::HeaderElementCount] > 2000u, "image table keys: the large table's entries were not bound");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        static_cast<void>(ValidateAndOptimizeSpirv(a.spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif
    }
}

struct CountingMemory {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::uint64_t unmapped = 0;
    std::uint64_t wordReads = 0;
    std::uint64_t bulkCalls = 0;
    std::uint64_t bulkWords = 0;
};

bool CountingAccessible(void* context, std::uint64_t address, std::uint64_t bytes) {
    const auto& memory = *static_cast<const CountingMemory*>(context);
    return memory.unmapped == 0u || address >= memory.unmapped + 4096u || address + bytes <= memory.unmapped;
}

bool CountingRead(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& memory = *static_cast<CountingMemory*>(context);
    if (!CountingAccessible(context, address, sizeof(*value))) return false;
    if (address >= memory.begin && address < memory.end) memory.wordReads++;
    std::memcpy(value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), sizeof(*value));
    return true;
}

bool CountingReadWords(void* context, std::uint64_t address, std::uint32_t* values, std::uint64_t count) {
    auto& memory = *static_cast<CountingMemory*>(context);
    if (!CountingAccessible(context, address, count * sizeof(std::uint32_t))) return false;
    memory.bulkCalls++;
    memory.bulkWords += count;
    std::memcpy(values, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), static_cast<std::size_t>(count) * sizeof(std::uint32_t));
    return true;
}

void verifyImageTableBulkReads() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::uint32_t Records = 128;
    static TableTexture textures[3];
    alignas(4096) static TableGuest guest;
    alignas(4096) static std::array<std::uint32_t, 3 * 1024> words{};
    const auto capture = [&](const RecompileRequest& request, CountingMemory& memory, bool bulk) {
        SrtRuntime runtime;
        runtime.userData = request.context.userData;
        runtime.shaderBase = request.shader.codeAddress;
        runtime.userContext = &memory;
        runtime.readMemory = &CountingRead;
        runtime.readSpecializationMemory = &CountingRead;
        runtime.accessible = &CountingAccessible;
        if (bulk) runtime.readWords = &CountingReadWords;
        return CaptureResources(request, runtime);
    };
    const auto place = [&](std::uint32_t firstWord, std::uint32_t records) {
        std::fill(words.begin(), words.end(), 0u);
        for (std::uint32_t record = 0; record < records; record++) {
            const auto image = TableT(textures[record % 3u].bytes.data(), TableFormat8888UNorm, TableType2D, 1u + record % 5u);
            std::copy(image.begin(), image.end(), words.begin() + firstWord + record * 8u);
        }
        FillTableSrt(guest, words.data() + firstWord, 32u, records);
    };
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableWholeCode, userData);
    place(512u, Records);
    CountingMemory byWordReads{reinterpret_cast<std::uintptr_t>(words.data() + 512u), reinterpret_cast<std::uintptr_t>(words.data() + 512u + Records * 8u)};
    CountingMemory byRunReads = byWordReads;
    const auto byWord = capture(request, byWordReads, false);
    const auto byRun = capture(request, byRunReads, true);
    require(byWordReads.wordReads == Records * 8u && byWordReads.bulkCalls == 0u, "image table bulk reads: without a bulk reader the records were not read word by word once");
    require(byRunReads.wordReads == 0u && byRunReads.bulkCalls == 1u && byRunReads.bulkWords == Records * 8u, "image table bulk reads: the contiguous records across a page boundary were not read in one run");
    require(byRun->snapshot.tables == byWord->snapshot.tables && byRun->snapshot.tables.tables.at(0).keys == Records, "image table bulk reads: reading in runs changed the snapshot");
    AgcDriver::ShaderMemory memory({});
    const auto captured = memory.Capture(request);
    require(captured->snapshot.tables == byWord->snapshot.tables && TableCovered(memory.Regions(), words.data() + 512u, Records * 32u), "image table bulk reads: the shader memory's run reads changed the snapshot or its captured regions");

    place(1012u, 4u);
    CountingMemory cutWordReads{reinterpret_cast<std::uintptr_t>(words.data() + 1012u), reinterpret_cast<std::uintptr_t>(words.data() + 1044u), reinterpret_cast<std::uintptr_t>(words.data() + 1024u)};
    CountingMemory cutRunReads = cutWordReads;
    const auto cutByWord = capture(request, cutWordReads, false);
    const auto cutByRun = capture(request, cutRunReads, true);
    const auto unmapped = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped));
    const auto& cut = cutByRun->snapshot.tables.tables.at(0);
    require(cut.keys == 4u && (cut.codes[0] & Abi::PoisonFlag) == 0u && cut.codes[1] == unmapped && cut.codes[2] == unmapped && cut.codes[3] == unmapped, "image table bulk reads: a record crossing into an unmapped page was not Unmapped poison");
    require(cutByRun->snapshot.tables == cutByWord->snapshot.tables && cutWordReads.wordReads == 12u && cutRunReads.wordReads == 0u && cutRunReads.bulkWords == 12u, "image table bulk reads: a record cut by an unmapped page read other words in runs than word by word");
}

void verifyImageTables() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(4096) static TableGuest guest;
    auto& heap = guest.heap;
    auto& materials = guest.materials;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    heap[3] = heap[0];
    materials = {{{0u, 1u, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 3u, 0u, 0u}}};
    FillTableSrt(guest, heap.data(), 32u, 4u);
    const auto userData = TableUserData(guest);
    auto request = TableRequest(TableMaterialCode, userData);

    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u && plan->info.images[0].tableOperation == TableOperation::Supported, "image tables: the T# was not planned as a table");
    const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.has_value() && column->stride == 32u && column->addend == 0u && column->offset == 0u && column->dwordCount == 8u && !column->sampler, "image tables: the column is wrong");
    require(!column->keyDomains.empty() && column->keyDomains[0].offset == 4u && column->keyDomains[0].stride == 16u, "image tables: the material key domain was not recorded");
    auto staticRequest = request;
    const std::array<std::uint32_t, 2> absentResources{};
    staticRequest.context.userData = absentResources;
    staticRequest.context.memory = {};
    auto staticProgram = PrepareResourceProgram(staticRequest);
    ResourceMaterializer{}.ApplyStaticInterface(staticProgram);
    const auto& staticInfo = staticProgram.Resources().info;
    require(staticInfo.images.size() == 1u && staticInfo.images[0].table == 0u && !staticInfo.runtimeImageModes.at(0).empty() && staticInfo.runtimeImageModes[0].size() <= Abi::MaxModes && staticInfo.usesFaultBuffer, "image tables: the static interface lacks the table modes or the fault buffer");

    const auto compiled = CompileTable(request);
    const auto& tables = compiled.capture->snapshot.tables;
    require(tables.tables.size() == 1u, "image tables: the snapshot has no table");
    const auto& snapshot = tables.tables[0];
    require(snapshot.base == static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(heap.data())) && snapshot.size == 128u && snapshot.records == 4u && snapshot.keys == 4u && snapshot.fault == 0u && !snapshot.outside, "image tables: the column snapshot is wrong");
    require(snapshot.codes.size() == 4u && snapshot.codes[0] == snapshot.codes[3] && snapshot.codes[0] != snapshot.codes[1] && snapshot.codes[2] == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain)), "image tables: the narrowed codes are wrong");
    require(tables.words.at(snapshot.codes[0]).dwords == heap[0] && tables.words.at(snapshot.codes[1]).dwords == heap[1], "image tables: the snapshot words are wrong");
    for (const auto& material : materials) require(TableCovered(compiled.regions, &material[1], sizeof(std::uint32_t)), "image tables: a material key was not captured");
    for (const auto entry : {0u, 1u, 3u}) require(TableCovered(compiled.regions, heap[entry].data(), 32u), "image tables: a table entry was not captured");
    const auto& result = *compiled.result;
    const auto* images = TableBinding(result, DescriptorRole::ImageTable);
    require(images != nullptr && images->kind == DescriptorKind::SampledImage && images->count == 2u && images->guestDescriptor.size() == 16u && images->imageShapes.size() == 2u, "image tables: the image table binding is wrong");
    require(std::none_of(images->imageWritten.begin(), images->imageWritten.end(), [](bool written) { return written; }), "image tables: a table element is marked written");
    require(TableBinding(result, DescriptorRole::FaultBuffer) != nullptr && TableBinding(result, DescriptorRole::GuestImages) == nullptr, "image tables: the bindings lack the fault buffer or hold table slots");
    const auto& map = TableMap(result);
    require(map[Abi::HeaderVersion] == Abi::Version && map[Abi::HeaderTableCount] == 1u && map[Abi::HeaderElementCount] == 2u && map[Abi::HeaderSamplerCount] == 0u, "image tables: the map header is wrong");
    const auto codes = TableCodes(map, 0);
    require(codes.size() == 4u && codes[0] == codes[3] && codes[2] == Abi::PoisonCode(Abi::OutsideDomainPoison) && (codes[0] & Abi::PoisonFlag) == 0u && (codes[1] & Abi::PoisonFlag) == 0u && codes[0] != codes[1], "image tables: the map codes are wrong");
    const auto modeOf = [&](std::uint32_t code) { return (map.at(code + Abi::ImageRecordFlags) >> Abi::ModeShift) & Abi::ModeMask; };
    const auto& modes = staticInfo.runtimeImageModes[0];
    require(map.at(codes[0] + Abi::ImageRecordElement) == 0u && map.at(codes[1] + Abi::ImageRecordElement) == 1u && modes.at(modeOf(codes[0])).numericClass == IrTextureNumericClass::Float && modes.at(modeOf(codes[1])).numericClass == IrTextureNumericClass::Uint, "image tables: the records are wrong");
    require(map.at(codes[1] + Abi::ImageRecordDword3) == heap[1][3], "image tables: the record does not carry the T# swizzle word");
    require(result.imageTableFaults == 0u, "image tables: a narrowed table expects faults");
    const auto uniform = ScanTableSpirv(result.spirv);
    require(uniform.runtimeArray && uniform.dynamicIndexing && uniform.switched, "image tables: the SPIR-V does not index the table");
    require(!uniform.shaderNonUniform && !uniform.nonUniform, "image tables: a single-subgroup workgroup was decorated NonUniform");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(result.spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif

    const auto split = CompileTable(TableRequest(TableMaterialCode, userData, 64u));
    const auto splitScan = ScanTableSpirv(split.result->spirv);
    require(splitScan.shaderNonUniform && splitScan.nonUniform, "image tables: a split wave's table index is not decorated NonUniform");

    for (const auto unmapped : {9u, 0xffffffffu}) {
        materials[2][1] = unmapped;
        const auto range = CompileTable(request);
        const auto rangeCodes = TableCodes(TableMap(*range.result), 0);
        require(rangeCodes.size() == 4u && rangeCodes[3] == Abi::PoisonCode(Abi::OutsideDomainPoison) && !range.capture->snapshot.tables.tables[0].outside && range.result->imageTableFaults == 0u, "image tables: a key past the V# range was kept or expects a fault");
        require(range.result->variantId == result.variantId, "image tables: the keys changed the variant");
    }
    materials[2][1] = 2u;
    const auto nulled = CompileTable(request);
    require(TableCodes(TableMap(*nulled.result), 0).at(2) == Abi::NullCode, "image tables: a null entry's key was mapped to a record");
    materials[2][1] = 3u;

    std::vector<std::uint32_t> latchCode(TableWholeCode.begin(), TableWholeCode.begin() + 12);
    latchCode.insert(latchCode.end(), {0xbea80380u, 0xf09c0f08u, 0x00450000u, 0x80288128u, 0xbf0a8428u, 0xbf85fffbu, 0xe0700000u, 0x80070000u, 0xbf810000u});
    const auto latch = CompileTable(TableRequest(latchCode, userData));
    require(!latch.result->spirv.empty(), "image tables: a sample in a loop latch did not compile");

    const auto whole = CompileTable(TableRequest(TableWholeCode, userData));
    const auto& wholePlan = *whole.capture->plan;
    require(wholePlan.descriptorSources.at(wholePlan.info.images.at(0).source).tableColumn->keyDomains.empty(), "image tables: a key domain was recorded without a material read");
    const auto wholeCodes = TableCodes(TableMap(*whole.result), 0);
    require(wholeCodes.size() == 4u && wholeCodes[0] == wholeCodes[3] && wholeCodes[2] == Abi::NullCode && wholeCodes[1] != wholeCodes[0], "image tables: the whole table map is wrong");

    for (std::uint32_t entry = 4; entry < 100u; entry++) heap[entry] = TableT(textures[entry % 2u].bytes.data(), entry % 3u == 0u ? TableFormat32UInt : TableFormat8888UNorm, TableType2D, entry);
    FillTableSrt(guest, heap.data(), 32u, 100u);
    const auto wide = CompileTable(TableRequest(TableWholeCode, userData));
    const auto& wideMap = TableMap(*wide.result);
    require(wideMap[Abi::TableHeader(0) + Abi::TableKeyCount] == 100u && wideMap[Abi::HeaderElementCount] == 98u && wide.result->variantId == whole.result->variantId, "image tables: a wide table was not bound in full");
    for (std::uint32_t entry = 4; entry < 100u; entry++) heap[entry] = {};
    FillTableSrt(guest, heap.data(), 32u, 4u);

    const auto saved = heap;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    const auto empty = CompileTable(request);
    const auto emptyCodes = TableCodes(TableMap(*empty.result), 0);
    require(TableMap(*empty.result)[Abi::HeaderElementCount] == 0u && std::all_of(emptyCodes.begin(), emptyCodes.end(), [](std::uint32_t code) { return code == Abi::NullCode || code == Abi::PoisonCode(Abi::OutsideDomainPoison); }), "image tables: an empty table has records");
    require(empty.result->variantId == result.variantId, "image tables: an empty table changed the artifact");
    heap = saved;

    std::vector<std::uint32_t> queryCode(TableWholeCode.begin(), TableWholeCode.begin() + 8);
    queryCode.insert(queryCode.end(), {0xbea80380u, 0x7e040280u, 0x7e200500u, 0x80102810u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf0380f08u, 0x00050402u, 0xf09c0f08u, 0x00450800u, 0x80288128u, 0xbf0a8228u, 0xbf85fff4u, 0xe0700000u, 0x80070800u, 0xe0700004u, 0x80070400u, 0xbf810000u});
    const auto query = CompileTable(TableRequest(queryCode, userData));
    const auto& queryPlan = *query.capture->plan;
    require(std::ranges::count_if(queryPlan.descriptorSources, [](const DescriptorSource& source) { return source.tableColumn.has_value(); }) == 1 && queryPlan.info.images.size() == 1u && queryPlan.info.images[0].table == 0u, "image tables: a T# both queried and sampled was not planned as one table column");
    const auto queryCodes = TableCodes(TableMap(*query.result), 0);
    require(queryCodes.size() == 4u && queryCodes[0] == queryCodes[3] && queryCodes[2] == Abi::NullCode && queryCodes[1] != queryCodes[0], "image tables: the queried and sampled table's map is wrong");
    require(!query.result->spirv.empty(), "image tables: a T# both queried and sampled did not compile");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(query.result->spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif

    heap[0][3] = (heap[0][3] & 0x0fffffffu) | (1u << 28u);
    const auto invalid = CompileTable(request);
    const auto& invalidMap = TableMap(*invalid.result);
    const auto invalidCodes = TableCodes(invalidMap, 0);
    const auto invalidFlags = invalidMap.at(invalidCodes[0] + Abi::ImageRecordFlags);
    require((invalidCodes[0] & Abi::PoisonFlag) == 0u && (invalidFlags & Abi::PoisonFlag) != 0u && invalidCodes[0] != invalidCodes[3], "image tables: an invalid T# was bound");
    const auto& poison = invalid.result->imageTablePoison.at(invalidFlags & ~Abi::PoisonFlag);
    require(poison.reason == static_cast<std::uint32_t>(Abi::PoisonReason::NotImage) && poison.dwordCount == 8u && poison.words[3] == heap[0][3] && invalid.result->imageTableFaults != 0u, "image tables: an invalid T# was not poisoned with its words");
    require(invalid.result->variantId == result.variantId, "image tables: an invalid T# changed the artifact");

    heap[0][3] &= 0x0fffffffu;
    const auto typeZeroTable = CompileTable(request);
    const auto typeZeroCodes = TableCodes(TableMap(*typeZeroTable.result), 0);
    const auto typeZeroFlags = TableMap(*typeZeroTable.result).at(typeZeroCodes[0] + Abi::ImageRecordFlags);
    require((typeZeroCodes[0] & Abi::PoisonFlag) == 0u && (typeZeroFlags & Abi::PoisonFlag) != 0u && typeZeroTable.result->imageTableFaults != 0u,
            "image tables: a TYPE=0 table entry was treated as a measured static null image");
    heap = saved;

    FillTableSrt(guest, heap.data(), 0u, 100u);
    const auto partial = CompileTable(request);
    const auto& partialTables = partial.capture->snapshot.tables;
    require(partialTables.tables[0].keys == 4u && partialTables.tables[0].size == 100u, "image tables: a partial table has the wrong record count");
    const auto& tail = partialTables.words.at(partialTables.tables[0].codes.at(3));
    require(tail.dwords[0] == heap[3][0] && std::all_of(tail.dwords.begin() + 1, tail.dwords.end(), [](std::uint32_t word) { return word == 0u; }), "image tables: the straddling record's tail is not zero");
    const auto& partialMap = TableMap(*partial.result);
    require((partialMap.at(TableCodes(partialMap, 0).at(3) + Abi::ImageRecordFlags) & Abi::PoisonFlag) != 0u, "image tables: a straddling record was bound");
    FillTableSrt(guest, heap.data(), 32u, 4u);

    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, 4096, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    require(reserved != nullptr, "image tables: cannot reserve an inaccessible page");
    FillTableSrt(guest, reserved, 32u, 4u);
    const auto unmapped = CompileTable(TableRequest(TableWholeCode, userData));
    const auto unmappedCodes = unmapped.capture->snapshot.tables.tables.at(0).codes;
    require(unmappedCodes.size() == 4u && std::all_of(unmappedCodes.begin(), unmappedCodes.end(), [](std::uint32_t code) { return code == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped)); }), "image tables: an unmapped table is not Unmapped poison");
    const auto unmappedRange = std::any_of(unmapped.regions.begin(), unmapped.regions.end(), [&](const MemoryRegion& region) { const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(reserved)); return region.guestAddress < address + 4096u && address < region.guestAddress + region.bytes.size(); });
    require(!unmappedRange, "image tables: an unmapped record was read");
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, 4096);
#endif
    FillTableSrt(guest, heap.data(), 32u, 4u);

    alignas(4096) static std::array<std::array<std::uint32_t, 8>, 70000> capped{};
    FillTableSrt(guest, capped.data(), 32u, static_cast<std::uint32_t>(capped.size()));
    const auto cappedResult = CompileTable(TableRequest(TableWholeCode, userData));
    const auto& cappedColumn = cappedResult.capture->snapshot.tables.tables.at(0);
    require(cappedColumn.keys == Abi::MaxKeys && cappedColumn.records == capped.size() && cappedColumn.outside && cappedResult.result->imageTableFaults == 1u, "image tables: the key cap did not mark the table's tail outside the snapshot");
    FillTableSrt(guest, heap.data(), 32u, 4u);

    guest.srt[3] |= 0x40000000u;
    const auto typed = CompileTable(request);
    const auto& typedMap = TableMap(*typed.result);
    const auto typedFault = typedMap[Abi::TableHeader(0) + Abi::TableFault];
    require((typedFault & Abi::PoisonFlag) != 0u && typed.result->imageTablePoison.at(typedFault & ~Abi::PoisonFlag).reason == static_cast<std::uint32_t>(Abi::PoisonReason::NotBuffer), "image tables: a typed table V# was not a whole-table fault");
    FillTableSrt(guest, heap.data(), 32u, 4u);
}

void verifyImageTableKeyUnion() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[3];
    alignas(4096) static TableGuest guest;
    auto& heap = guest.heap;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    heap[2] = heap[1];
    heap[3] = TableT(textures[2].bytes.data(), TableFormat8888UNorm, TableType2D);
    FillTableSrt(guest, heap.data(), 32u, 4u);
    guest.srt[16] = 1u;
    guest.srt[17] = 3u;
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableTwoKeyCode, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "image table key union: two keyed reads of one table were not planned as one image of one column");
    const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.has_value() && column->keyDomains.size() == 2u && column->keyDomains[0].value && column->keyDomains[1].value && column->keyDomains[0].source != column->keyDomains[1].source, "image table key union: the column does not carry both value key domains");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    require(compiled.capture->snapshot.tables.tables.size() == 1u && snapshot.keys == 4u && (snapshot.codes[0] & Abi::PoisonFlag) == 0u && (snapshot.codes[1] & Abi::PoisonFlag) == 0u && snapshot.codes[2] == outside && (snapshot.codes[3] & Abi::PoisonFlag) == 0u, "image table key union: the snapshot does not hold the union of the two keys' records");
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && images->count == 3u, "image table key union: the binding does not hold the three distinct images of the selected records");
    guest.srt[17] = 2u;
    const auto moved = CompileTable(request);
    const auto& movedSnapshot = moved.capture->snapshot.tables.tables.at(0);
    require((movedSnapshot.codes[2] & Abi::PoisonFlag) == 0u && movedSnapshot.codes[3] == outside && (movedSnapshot.codes[1] & Abi::PoisonFlag) == 0u && moved.result->spirv == compiled.result->spirv, "image table key union: a changed second key did not move the union or changed the module");
}

void verifyImageTableLateKey() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[3];
    alignas(4096) static TableGuest guest;
    auto& heap = guest.heap;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    heap[2] = heap[1];
    heap[3] = TableT(textures[2].bytes.data(), TableFormat8888UNorm, TableType2D);
    FillTableSrt(guest, heap.data(), 32u, 4u);
    guest.srt[16] = 1u;
    guest.srt[17] = 3u;
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableLateKeyCode, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "image table late key: two keyed reads of one table were not planned as one image of one column");
    const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.has_value() && column->keyDomains.size() == 2u && column->keyDomains[0].value && column->keyDomains[1].value && column->keyDomains[0].source != column->keyDomains[1].source, "image table late key: the column does not carry both value key domains");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    require(compiled.capture->snapshot.tables.tables.size() == 1u && snapshot.keys == 4u && (snapshot.codes[0] & Abi::PoisonFlag) == 0u && (snapshot.codes[1] & Abi::PoisonFlag) == 0u && snapshot.codes[2] == outside && (snapshot.codes[3] & Abi::PoisonFlag) == 0u, "image table late key: the snapshot does not hold the union of the two keys' records");
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && images->count == 3u, "image table late key: the binding does not hold the three distinct images of the selected records");
}

void verifyImageTableUnkeyedAccess() {
    for (const auto* code : {&TableMixedKeyCode, &TableMixedKeyFirstCode}) {
        using namespace ShaderRecompiler;
        namespace Abi = ImageTableAbi;
        static TableTexture textures[4];
        alignas(4096) static TableGuest guest;
        auto& heap = guest.heap;
        std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
        heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
        heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
        heap[2] = TableT(textures[2].bytes.data(), TableFormat8888UNorm, TableType2D);
        heap[3] = TableT(textures[3].bytes.data(), TableFormat32UInt, TableType2D);
        FillTableSrt(guest, heap.data(), 32u, 4u);
        guest.srt[16] = 2u;
        guest.materials = {};
        guest.materials[0][1] = 3u;
        guest.materials[1][1] = 1u;
        const auto userData = TableUserData(guest);
        const auto request = TableRequest(*code, userData);
        const auto plan = GetResourcePlan(request);
        require(!plan->info.images.empty() && std::all_of(plan->info.images.begin(), plan->info.images.end(), [](const ImageResource& image) { return image.table == 0u; }), "image table unkeyed access: the two reads were not planned as one table");
        const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
        require(column.has_value() && column->unkeyed && column->keyDomains.empty(), "image table unkeyed access: a column shared with a read that has no key domain kept a key domain");
        const auto compiled = CompileTable(request);
        const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
        const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
        require(snapshot.keys == 4u && std::none_of(snapshot.codes.begin(), snapshot.codes.end(), [&](std::uint32_t code) { return code == outside; }), "image table unkeyed access: the shared column was narrowed by the keyed read");
        const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
        require(images != nullptr && images->count == 4u, "image table unkeyed access: the binding does not hold the whole domain");
    }
}

void verifyImageTableSrtKey() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(4096) static TableGuest guest;
    auto& heap = guest.heap;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    heap[2] = heap[1];
    heap[3] = heap[0];
    FillTableSrt(guest, heap.data(), 32u, 4u);
    guest.srt[16] = 2u;
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableSrtKeyCode, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "image table SRT key: the T# was not planned as a table");
    const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.has_value() && !column->keyDomains.empty(), "image table SRT key: a key read through the SRT has no key domain");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    require(snapshot.records == 4u && snapshot.keys == 4u && snapshot.codes.size() == 4u && (snapshot.codes[0] & Abi::PoisonFlag) == 0u && snapshot.codes[1] == outside && (snapshot.codes[2] & Abi::PoisonFlag) == 0u && snapshot.codes[3] == outside, "image table SRT key: the snapshot was not narrowed to the key read through the SRT");
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && images->count == 2u, "image table SRT key: the binding holds other entries than the selected one and the first");
    require(TableCovered(compiled.regions, &guest.srt[16], sizeof(std::uint32_t)), "image table SRT key: the key was not captured");
    guest.srt[16] = 3u;
    const auto moved = CompileTable(request);
    const auto& movedSnapshot = moved.capture->snapshot.tables.tables.at(0);
    require((movedSnapshot.codes[3] & Abi::PoisonFlag) == 0u && movedSnapshot.codes[2] == outside && movedSnapshot.codes[1] == outside && moved.result->spirv == compiled.result->spirv && moved.key == compiled.key, "image table SRT key: a changed key did not move the snapshot alone");
}

void verifyImageTableMaskedOffset() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(4096) static TableGuest guest;
    auto& heap = guest.heap;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    heap[3] = heap[0];
    guest.materials = {{{0u, 0x101u, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 0x203u, 0u, 0u}}};
    FillTableSrt(guest, heap.data(), 32u, 4u);
    std::vector<std::uint32_t> code(TableMaterialCode);
    const auto shift = std::find(code.begin(), code.end(), 0x8f108510u);
    require(shift != code.end(), "image table masked offset: the program has no s_lshl_b32");
    code.insert(shift + 1, {0x8710ff10u, 0x60u});
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(code, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u && plan->info.images[0].tableOperation == TableOperation::Supported, "image table masked offset: the T# was not planned as a table");
    const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.has_value() && column->stride == 32u && column->addend == 0u && column->offset == 0u && column->dwordCount == 8u, "image table masked offset: the column is wrong");
    require(column->keyDomains.empty() && column->unkeyed, "image table masked offset: the masked key was narrowed by its unmasked value");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    require(snapshot.records == 4u && snapshot.keys == 4u && snapshot.codes.size() == 4u && std::none_of(snapshot.codes.begin(), snapshot.codes.end(), [&](std::uint32_t entry) { return entry == outside; }), "image table masked offset: a record was left outside the domain");
    const auto codes = TableCodes(TableMap(*compiled.result), 0);
    require(codes.size() == 4u && codes[0] == codes[3] && codes[2] == Abi::NullCode && codes[1] != codes[0] && (codes[1] & Abi::PoisonFlag) == 0u, "image table masked offset: the whole table map is wrong");
}

void verifyImageTableRegistration() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture texture;
    alignas(4096) static TableGuest guest;
    std::fill(guest.heap.begin(), guest.heap.end(), std::array<std::uint32_t, 8>{});
    guest.heap[0] = TableT(texture.bytes.data(), TableFormat32UInt, TableType2D);
    guest.heap[1] = TableT(texture.bytes.data(), TableFormat8888UNorm, TableType2D);
    FillTableSrt(guest, guest.heap.data(), 32u, 4u);
    const auto userData = TableUserData(guest);
    const std::vector<std::uint32_t> storeCode{0xf4080100u, 0xfa000000u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf0200f08u, 0x00050200u, 0xbf810000u};
    const auto store = CompileTable(TableRequest(storeCode, userData));
    const auto& storePlan = *store.capture->plan;
    require(storePlan.info.images.size() == 1u && storePlan.info.images[0].table == 0u && storePlan.info.images[0].tableOperation == TableOperation::Unsupported, "image table registration: a table T# feeding image_store was not an unsupported table operation");
    const auto& storeMap = TableMap(*store.result);
    const auto storeFault = storeMap[Abi::TableHeader(0) + Abi::TableFault];
    require((storeFault & Abi::PoisonFlag) != 0u && store.result->imageTablePoison.at(storeFault & ~Abi::PoisonFlag).reason == static_cast<std::uint32_t>(Abi::PoisonReason::UnsupportedOperation) && store.result->imageTableFaults != 0u, "image table registration: the store's table does not fault on use");
    require(TableBinding(*store.result, DescriptorRole::ImageTable) == nullptr || TableBinding(*store.result, DescriptorRole::ImageTable)->count == 0u, "image table registration: an unsupported table bound elements");

    const std::vector<std::uint32_t> compareCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf0fc0108u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    auto compareRequest = TableRequest(compareCode, userData);
    compareRequest.target.nonConstantImageOffsets = false;
    const auto compare = CompileTable(compareRequest);
    const auto& compareImage = compare.capture->plan->info.images.at(0);
    require(compareImage.table == 0u && compareImage.depthCompare, "image table registration: the compare T# is not a table");
    const auto compareFault = TableMap(*compare.result)[Abi::TableHeader(0) + Abi::TableFault];
    require((compareFault & Abi::PoisonFlag) != 0u && compare.result->imageTablePoison.at(compareFault & ~Abi::PoisonFlag).reason == static_cast<std::uint32_t>(Abi::PoisonReason::UnsupportedOperation), "image table registration: a native compare with a nonconstant offset through a table does not fault on use");

    guest.heap[0] = {0x00000092u, 0x00fff000u, 0x05500000u, 0u, 0u, 0u, 0u, 0u};
    FillTableSrt(guest, guest.heap.data(), 0u, 64u);
    const auto direct = TableT(texture.bytes.data(), TableFormat8888UNorm, TableType2D);
    std::copy(direct.begin(), direct.end(), guest.srt.begin() + 4);
    const std::vector<std::uint32_t> colorCompareCode{0xf4080100u, 0xfa000000u, 0xf40c0500u, 0xfa000010u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108410u, 0xf4280202u, 0x20000000u, 0xf0bc0108u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    auto colorRequest = TableRequest(colorCompareCode, userData);
    const auto colorPlan = GetResourcePlan(colorRequest);
    require(colorPlan->info.samplers.size() == 1u && colorPlan->info.samplers[0].table != NoTable && colorPlan->info.images.size() == 1u && colorPlan->info.images[0].table == NoTable, "image table registration: the color compare program does not pair a direct T# with a sampler table");
    AgcDriver::ShaderMemory colorMemory({});
    const auto colorCapture = colorMemory.Capture(colorRequest);
    const auto colorRegions = colorMemory.Regions();
    colorRequest.context.memory = colorRegions;
    expectFailure([&] { static_cast<void>(Recompile(colorRequest, *colorCapture)); }, "comparison sampling of a color texture through a sampler table is not implemented", "image table registration: a color compare through a sampler table was accepted");
}

bool TableTouched(const std::vector<ShaderRecompiler::MemoryRegion>& regions, const void* pointer, std::size_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    return std::any_of(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& region) { return region.guestAddress < address + bytes && address < region.guestAddress + region.bytes.size(); });
}

void verifyScalarAddressRule() {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    require(Abi::ScalarAddressDword(0x1000u, 0x23u, 0x41u) == 0x1060u && Abi::ScalarAddressDword(0x1001u, 0x20u, 0u) == 0x1021u, "scalar address rule: the offset and immediate are not dword-aligned on their own");
    require(Abi::ScalarAddressDword(0x1000u, 0x20u, 0xfffffff0u) == 0x1010u && !Abi::ScalarAddressDword(0x8u, 0x4u, 0xfffffff0u).has_value(), "scalar address rule: a negative immediate is wrong");
    require(!Abi::ScalarAddressDword(0xfffffffffffffff0ull, 0x10u, 0u).has_value() && !Abi::ScalarAddressDword(0xfffffffffffffff0ull, 0x8u, 0x8u).has_value() && Abi::ScalarAddressDword(0xfffffffffffffff0ull, 0x8u, 0x4u) == 0xfffffffffffffffcull, "scalar address rule: a carry past 2^64 is wrong");
    for (const std::uint32_t stride : {4u, 32u, 48u, 0x330u}) {
        const std::uint64_t records = Abi::ScalarAddressRecords(stride);
        require((records - 1u) * stride <= 0xffffffffull && records * stride > 0xffffffffull, "scalar address rule: the record count does not end at 2^32");
        for (const std::uint32_t maxKey : {0u, 0xffu, 0xffffu, 0x3fffffffu, 0x40000000u, 0xffffffffu}) {
            const bool exact = static_cast<std::uint64_t>(maxKey) * stride <= 0xffffffffull;
            require(Abi::KeyRecords(maxKey, stride) == (exact ? maxKey + 1u : 0xffffffffu), "scalar address rule: the key bound is wrong");
        }
    }
}

void verifyPointerImageTable() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::uint32_t Palette = 0x40;
    constexpr std::uint32_t Entries = 256;
    constexpr std::size_t Committed = 0x1f0000;
    constexpr std::size_t Reserved = 0x400000;
    static TableTexture textures[2];
    void* block = nullptr;
#ifdef _WIN32
    block = VirtualAlloc(nullptr, Reserved, MEM_RESERVE, PAGE_NOACCESS);
    require(block != nullptr && VirtualAlloc(block, Committed, MEM_COMMIT, PAGE_READWRITE) == block, "pointer image table: cannot map the root");
#else
    block = mmap(nullptr, Reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(block != MAP_FAILED && mprotect(block, Committed, PROT_READ | PROT_WRITE) == 0, "pointer image table: cannot map the root");
#endif
    auto* root = static_cast<std::uint32_t*>(block);
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root));
    static std::array<std::uint32_t, 8> keys{};
    static std::array<std::uint32_t, 4> output{};
    const auto keysV = TableV(keys.data(), 0u, static_cast<std::uint32_t>(sizeof(keys)));
    const auto outputV = TableV(output.data(), 0u, static_cast<std::uint32_t>(sizeof(output)));
    std::copy(keysV.begin(), keysV.end(), root);
    std::copy(outputV.begin(), outputV.end(), root + 4);
    const auto pointer = base + Palette + 0x100u;
    root[12] = static_cast<std::uint32_t>(pointer);
    root[13] = static_cast<std::uint32_t>(pointer >> 32u);
    const auto entry = [&](std::uint32_t index, std::array<std::uint32_t, 8> words) {
        std::copy(words.begin(), words.end(), root + (Palette + index * 32u) / 4u);
    };
    entry(0, TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D));
    entry(1, TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D));
    entry(2, TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D));
    entry(4, {0x1234u, 0x5678u, 0x9abcu, 0x1000ffacu, 0u, 0u, 0u, 0u});
    entry(255, TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D));
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const auto request = [&](const std::vector<std::uint32_t>& code, std::uint64_t at) {
        auto made = TableRequest(code, userData);
        made.shader.codeAddress = at;
        return made;
    };

    const std::vector<std::uint32_t> maskedCode{0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x7e200500u, 0x8f108210u,
        0xf4200444u, 0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c0900u, 0x24000040u, 0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    const auto masked = request(maskedCode, 0x31000u);
    const auto plan = GetResourcePlan(masked);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "pointer image table: the T# was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.address && column.stride == 32u && column.addend == 0u && column.offset == Palette && column.dwordCount == 8u && column.maxKey == 0xffu && column.keyDomains.empty() && column.unkeyed, "pointer image table: the column is wrong");
    require(plan->descriptorSources.at(column.heapSource).dwordCount == 2u, "pointer image table: the table base is not a 64-bit pointer");
    std::uint32_t planningOnly = 0;
    for (const auto& memory : plan->memoryInfo) {
        if (memory.kind == ResourceKind::ScalarAddress && memory.offset >= Palette && memory.offset < Palette + 32u && memory.planningOnly) planningOnly++;
    }
    require(planningOnly == 8u && !plan->info.usesDma, "pointer image table: the descriptor loads still read through BDA");

    const auto compiled = CompileTable(masked);
    const auto& tables = compiled.capture->snapshot.tables;
    const auto& snapshot = tables.tables.at(0);
    require(snapshot.base == base && snapshot.size == 0u && snapshot.keys == Entries && snapshot.records == Entries && !snapshot.outside && snapshot.fault == 0u, "pointer image table: the column snapshot is wrong");
    require(snapshot.codes[0] == snapshot.codes[1] && snapshot.codes[2] == snapshot.codes[255] && snapshot.codes[0] != snapshot.codes[2] && tables.words.at(snapshot.codes[2]).dwords[0] == root[(Palette + 2u * 32u) / 4u], "pointer image table: the snapshot words are wrong");
    require(tables.ranges == std::vector<std::pair<std::uint64_t, std::uint64_t>>{{base + Palette, Entries * 32u}}, "pointer image table: the snapshot range is wrong");
    require(snapshot.readRanges == tables.ranges && compiled.result->imageTableReadRanges.size() == 1u && compiled.result->imageTableReadRanges[0] == snapshot.readRanges, "pointer image table: the consumed ranges did not retain table ownership");
    require(TableCovered(compiled.regions, root + Palette / 4u, Entries * 32u) && !TableTouched(compiled.regions, root + (Palette + Entries * 32u) / 4u, Committed - Palette - Entries * 32u), "pointer image table: the captured regions are wrong");
    const auto& map = TableMap(*compiled.result);
    const auto codes = TableCodes(map, 0);
    require(codes.size() == Entries && codes[0] == codes[1] && codes[2] == codes[255] && codes[3] == Abi::NullCode && map[Abi::HeaderElementCount] == 2u, "pointer image table: the map codes are wrong");
    const auto invalidFlags = map.at(codes[4] + Abi::ImageRecordFlags);
    require((invalidFlags & Abi::PoisonFlag) != 0u && compiled.result->imageTablePoison.at(invalidFlags & ~Abi::PoisonFlag).reason == static_cast<std::uint32_t>(Abi::PoisonReason::NotImage), "pointer image table: an invalid entry is not NotImage poison");
    require(compiled.result->imageTableRanges == tables.ranges && map[Abi::TableHeader(0) + Abi::TableBaseLow] == static_cast<std::uint32_t>(base) && map[Abi::TableHeader(0) + Abi::TableBaseHigh] == static_cast<std::uint32_t>(base >> 32u), "pointer image table: the compiled table is wrong");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(compiled.result->spirv, masked.target.vulkanVersion, masked.target.spirvVersion));
#endif

    const std::vector<std::uint32_t> pointerCode{0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0xf4040500u, 0xfa000030u,
        0x7e200500u, 0x8f108210u, 0xf4200444u, 0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c090au, 0x241fff00u, 0xf09c0f08u, 0x01090000u,
        0xe0700000u, 0x80070000u, 0xbf810000u};
    const auto loaded = request(pointerCode, 0x32000u);
    const auto loadedPlan = GetResourcePlan(loaded);
    require(loadedPlan->descriptorSources.at(loadedPlan->info.images.at(0).source).tableColumn->offset == 0xffffff00u, "pointer image table: a negative immediate was not kept");
    const auto loadedTable = CompileTable(loaded);
    require(loadedTable.capture->snapshot.tables.tables.at(0).codes == snapshot.codes && loadedTable.capture->snapshot.tables.ranges == tables.ranges, "pointer image table: a base loaded from the SRT with a negative immediate snapshots other records");
    require(loadedTable.result->imageTableReadRanges == compiled.result->imageTableReadRanges, "pointer image table: a negative immediate changed the consumed ranges");
    require(TableCodes(TableMap(*loadedTable.result), 0) == codes, "pointer image table: a loaded base maps other records");
    root[12] = static_cast<std::uint32_t>(pointer) | 2u;
    const auto unaligned = CompileTable(loaded);
    const auto& unalignedCodes = unaligned.capture->snapshot.tables.tables.at(0).codes;
    require(unalignedCodes.size() == Entries && std::all_of(unalignedCodes.begin(), unalignedCodes.end(), [](std::uint32_t code) { return code == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped)); }), "pointer image table: a pointer that is not dword-aligned was read");
    require(unaligned.result->imageTableReadRanges == std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>>{{}}, "pointer image table: an unaligned pointer recorded consumed ranges");
    root[12] = static_cast<std::uint32_t>(pointer);

    const std::vector<std::uint32_t> directCode{0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0xf4000440u, 0xfa000038u, 0x8f128511u, 0xf40c0900u,
        0x24000040u, 0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    root[14] = 2u;
    const auto direct = request(directCode, 0x33000u);
    const auto directPlan = GetResourcePlan(direct);
    require(directPlan->info.images.size() == 1u && directPlan->info.images[0].table == NoTable, "pointer image table: a T# at an offset known at record time is not a direct binding");
    AgcDriver::ShaderMemory directMemory({});
    const auto directCapture = directMemory.Capture(direct);
    require(directCapture->snapshot.images.size() == 1u && directCapture->snapshot.images[0].dwords[0] == root[(Palette + 2u * 32u) / 4u] && !TableTouched(directMemory.Regions(), root + (Palette + 3u * 32u) / 4u, Committed - Palette - 3u * 32u), "pointer image table: a T# at an offset known at record time read other records");
    root[14] = 0u;

    const std::vector<std::uint32_t> wideCode{0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x7e220500u, 0x8f128511u, 0xf40c0900u, 0x24000040u,
        0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    const auto wide = request(wideCode, 0x34000u);
    const auto widePlan = GetResourcePlan(wide);
    const auto& wideColumn = *widePlan->descriptorSources.at(widePlan->info.images.at(0).source).tableColumn;
    require(wideColumn.maxKey == 0xffffffffu && wideColumn.keyDomains.empty() && wideColumn.unkeyed, "pointer image table: a lane key has a bound or a key domain");
    const auto wideTable = CompileTable(wide);
    const auto& wideSnapshot = wideTable.capture->snapshot.tables.tables.at(0);
    constexpr std::uint32_t mapped = static_cast<std::uint32_t>((Committed - Palette) / 32u);
    require(wideSnapshot.keys == Abi::MaxKeys && wideSnapshot.outside && wideTable.result->imageTableFaults != 0u, "pointer image table: an unbounded key did not snapshot the capped records");
    require(std::equal(snapshot.codes.begin(), snapshot.codes.end(), wideSnapshot.codes.begin()) && (wideSnapshot.codes[mapped - 1u] & Abi::PoisonFlag) == 0u, "pointer image table: an unbounded key changed the mapped records");
    require(wideSnapshot.codes[mapped] == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped)) && wideSnapshot.codes.back() == wideSnapshot.codes[mapped], "pointer image table: records past the mapping are not Unmapped poison");
    require(!TableTouched(wideTable.regions, root + Committed / 4u, Reserved - Committed), "pointer image table: an unmapped record was read");

    auto narrowCode = maskedCode;
    narrowCode.erase(narrowCode.begin() + 10, narrowCode.begin() + 12);
    keys = {2u, 255u, 0x102u, 2u, 0u, 0u, 0u, 0u};
    const auto narrow = request(narrowCode, 0x35000u);
    const auto narrowPlan = GetResourcePlan(narrow);
    const auto& narrowColumn = *narrowPlan->descriptorSources.at(narrowPlan->info.images.at(0).source).tableColumn;
    require(narrowColumn.keyDomains.size() == 1u && !narrowColumn.unkeyed && narrowColumn.maxKey == 0xffffffffu, "pointer image table: a key read from a V# has no key domain");
    const auto narrowTable = CompileTable(narrow);
    const auto& narrowSnapshot = narrowTable.capture->snapshot.tables.tables.at(0);
    const auto outsideDomain = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    require(narrowSnapshot.keys == Abi::MaxKeys && narrowSnapshot.codes[0] == snapshot.codes[0] && narrowSnapshot.codes[2] == snapshot.codes[2] && narrowSnapshot.codes[255] == snapshot.codes[255] && (narrowSnapshot.codes[0x102] & Abi::PoisonFlag) == 0u, "pointer image table: a record in the key domain was not resolved");
    require(narrowSnapshot.codes[1] == outsideDomain && narrowSnapshot.codes[4] == outsideDomain && narrowSnapshot.codes.back() == outsideDomain, "pointer image table: a record outside the key domain is not OutsideDomain poison");
    require(!TableTouched(narrowTable.regions, root + (Palette + 32u) / 4u, 32u) && !TableTouched(narrowTable.regions, root + Committed / 4u, Reserved - Committed), "pointer image table: a record outside the key domain was read");
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> narrowReads{{base + Palette, 32u}, {base + Palette + 2u * 32u, 32u}, {base + Palette + 255u * 32u, 32u}, {base + Palette + 0x102u * 32u, 32u}};
    require(narrowSnapshot.readRanges == narrowReads && narrowTable.result->imageTableReadRanges.size() == 1u && narrowTable.result->imageTableReadRanges[0] == narrowReads, "pointer image table: consumed ranges include skipped keys or the key-domain buffer");
    keys = {};
#ifdef _WIN32
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, Reserved);
#endif
}

void verifyPointerTableOffsetCarry() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::size_t Low = 0x10000;
    constexpr std::uint64_t High = 0xffff0000ull;
    constexpr std::size_t HighBytes = 0x20000;
    constexpr std::uint64_t Reserved = High + HighBytes;
    static TableTexture textures[2];
    void* block = nullptr;
#ifdef _WIN32
    block = VirtualAlloc(nullptr, static_cast<SIZE_T>(Reserved), MEM_RESERVE, PAGE_NOACCESS);
    require(block != nullptr && VirtualAlloc(block, Low, MEM_COMMIT, PAGE_READWRITE) == block && VirtualAlloc(static_cast<std::byte*>(block) + High, HighBytes, MEM_COMMIT, PAGE_READWRITE) == static_cast<std::byte*>(block) + High, "pointer table offset carry: cannot map the root");
#else
    block = mmap(nullptr, Reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    require(block != MAP_FAILED && mprotect(block, Low, PROT_READ | PROT_WRITE) == 0 && mprotect(static_cast<std::byte*>(block) + High, HighBytes, PROT_READ | PROT_WRITE) == 0, "pointer table offset carry: cannot map the root");
#endif
    auto* root = static_cast<std::uint32_t*>(block);
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root));
    static std::array<std::uint32_t, 8> keys{};
    static std::array<std::uint32_t, 4> output{};
    const auto keysV = TableV(keys.data(), 0u, static_cast<std::uint32_t>(sizeof(keys)));
    const auto outputV = TableV(output.data(), 0u, static_cast<std::uint32_t>(sizeof(output)));
    std::copy(keysV.begin(), keysV.end(), root);
    std::copy(outputV.begin(), outputV.end(), root + 4);
    const auto at = [&](std::uint64_t offset, std::array<std::uint32_t, 8> words) {
        std::copy(words.begin(), words.end(), root + offset / 4u);
    };
    at(0x100000010ull, TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D));
    at(0x50u, TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u)};
    const std::vector<std::uint32_t> code{0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x7e200500u, 0x8f108210u, 0xf4200444u,
        0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0x8113ff12u, 0xffffff10u, 0x8114ff12u, 0xffffff20u, 0xf4080900u, 0x26000040u, 0xf4080a00u,
        0x28000040u, 0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    auto request = TableRequest(code, userData);
    request.shader.codeAddress = 0x36000u;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "pointer table offset carry: a T# read by two loads of its record was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.address && column.stride == 32u && column.addend == 0xffffff10u && column.offset == 0x40u && column.maxKey == 0xffu && column.foldedAddend == 0xffffff10u && column.foldedCarry == 16u, "pointer table offset carry: the column is wrong");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    require(snapshot.keys == 256u && snapshot.codes[7] == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OffsetCarry)), "pointer table offset carry: the record whose second load wraps past 32 bits is not OffsetCarry poison");
    require((snapshot.codes[6] & Abi::PoisonFlag) == 0u && (snapshot.codes[8] & Abi::PoisonFlag) == 0u && compiled.capture->snapshot.tables.words.at(snapshot.codes[6]).dwords[0] == root[0x100000010ull / 4u] && compiled.capture->snapshot.tables.words.at(snapshot.codes[8]).dwords[0] == root[0x50u / 4u] && root[0x100000010ull / 4u] != root[0x50u / 4u], "pointer table offset carry: the records on either side of the carry were not resolved");
    require(!TableTouched(compiled.regions, root + 0x100000030ull / 4u, 0x20u), "pointer table offset carry: the record whose second load wraps was read");
#ifdef _WIN32
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, Reserved);
#endif
}

void verifyDescriptorPhis() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    const auto imageDescriptor = [&](const Texture& texture) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    const std::array<std::uint32_t, 4> pointWrap{0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 4> linearMirror{0x49u, 0u, 0x00500000u, 0u};
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    std::array<std::uint32_t, 32> srt{};
    const auto first = imageDescriptor(textures[0]);
    const auto second = imageDescriptor(textures[1]);
    std::copy(first.begin(), first.end(), srt.begin());
    std::copy(pointWrap.begin(), pointWrap.end(), srt.begin() + 8);
    std::copy(linearMirror.begin(), linearMirror.end(), srt.begin() + 12);
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
    srt[20] = 1u;
    std::copy(second.begin(), second.end(), srt.begin() + 24);
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const auto makeRequest = [&](std::span<const std::uint32_t> code, std::uint32_t waveSize = 32u) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x21000u, code, 0, {}};
        request.context.waveSize = waveSize;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{waveSize, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        return request;
    };
    const auto countOps = [](std::span<const std::uint32_t> words, std::uint32_t opcode) {
        std::size_t count = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto length = words[cursor] >> 16u;
            require(length != 0 && length <= words.size() - cursor, "descriptor Phi: truncated SPIR-V instruction");
            count += (words[cursor] & 0xffffu) == opcode ? 1u : 0u;
            cursor += length;
        }
        return count;
    };
    constexpr std::uint32_t OpImageSampleExplicitLod = 88;

    const std::vector<std::uint32_t> samplerCode{0xf40c0200u, 0xfa000000u, 0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0xfa000030u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const std::vector<std::uint32_t> imageCode{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const std::vector<std::uint32_t> dynamicCode{0xf40c0200u, 0xfa000000u, 0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0x20000000u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};

    const auto compile = [&](std::span<const std::uint32_t> code, std::size_t images, std::size_t samplers) {
        auto request = makeRequest(code);
        const auto plan = GetResourcePlan(request);
        require(plan->info.images.size() == images && plan->info.samplers.size() == samplers && plan->info.sampledPairs.size() == 2u, "descriptor Phi: the edges were not given one resource each");
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        request.context.memory = memory.Regions();
        const auto compiled = Recompile(request, *capture);
        require(countOps(compiled->spirv, OpImageSampleExplicitLod) == 2u, "descriptor Phi: the specialized SPIR-V does not sample once per edge");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        static_cast<void>(ValidateAndOptimizeSpirv(compiled->spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif
        return capture;
    };
    const auto samplerCapture = compile(samplerCode, 1u, 2u);
    const auto& samplers = samplerCapture->snapshot.samplers;
    require(samplers.size() == 2u, "descriptor Phi: the snapshot does not hold both S#s");
    const auto holds = [&](const std::array<std::uint32_t, 4>& words) {
        return std::ranges::any_of(samplers, [&](const DescriptorValue& value) {
            return value.dwordCount == 4u && std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(holds(pointWrap) && holds(linearMirror), "descriptor Phi: the snapshot S#s are not the two edges' S#s");

    const auto imageCapture = compile(imageCode, 2u, 1u);
    const auto& images = imageCapture->snapshot.images;
    require(images.size() == 2u, "descriptor Phi: the snapshot does not hold both T#s");
    const auto holdsImage = [&](const std::array<std::uint32_t, 8>& words) {
        return std::ranges::any_of(images, [&](const DescriptorValue& value) {
            return std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(holdsImage(first) && holdsImage(second), "descriptor Phi: the snapshot T#s are not the two edges' T#s");

    auto twoLane = makeRequest(samplerCode, 64u);
    AgcDriver::ShaderMemory twoLaneMemory({});
    const auto twoLaneCapture = twoLaneMemory.Capture(twoLane);
    twoLane.context.memory = twoLaneMemory.Regions();
    require(countOps(Recompile(twoLane, *twoLaneCapture)->spirv, OpImageSampleExplicitLod) == 4u, "descriptor Phi: the specialized two-lane SPIR-V does not sample once per edge and half");

    auto dynamic = makeRequest(dynamicCode);
    expectFailure([&] { static_cast<void>(GetResourcePlan(dynamic)); }, "GetSamplerResource dword 0 is not a valid runtime value", "descriptor Phi: an edge without an SRT slot was accepted");

    const std::vector<std::uint32_t> loopCode{0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u, 0xbe910380u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf068011u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xbf8cc07fu, 0x80118111u, 0xbf0a8211u, 0xbf85fff0u, 0xbf810000u};
    const auto loopCapture = compile(loopCode, 2u, 1u);
    const auto& loopImages = loopCapture->snapshot.images;
    const auto holdsLoopImage = [&](const std::array<std::uint32_t, 8>& words) {
        return std::ranges::any_of(loopImages, [&](const DescriptorValue& value) {
            return std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(loopImages.size() == 2u && holdsLoopImage(first) && holdsLoopImage(second), "descriptor Phi: the loop's chained T# Phis were not split into the two SRT T#s");

    const std::vector<std::uint32_t> sequentialCode{0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0xfa000030u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    auto sequential = makeRequest(sequentialCode);
    const auto sequentialPlan = GetResourcePlan(sequential);
    require(sequentialPlan->info.images.size() == 2u && sequentialPlan->info.samplers.size() == 2u, "descriptor Phi: a T# and an S# chosen in separate blocks were not split into their SRT descriptors");
    AgcDriver::ShaderMemory sequentialMemory({});
    const auto sequentialCapture = sequentialMemory.Capture(sequential);
    sequential.context.memory = sequentialMemory.Regions();
    static_cast<void>(Recompile(sequential, *sequentialCapture));

    const auto entryWrites = [&](std::initializer_list<std::uint32_t> words) {
        auto code = loopCode;
        code.insert(code.begin() + 8, words);
        return code;
    };
    const std::array<std::pair<std::vector<std::uint32_t>, const char*>, 3> invalidEntries{{
        {entryWrites({0xbe8e1f00u}), "descriptor Phi: a T# holding the program counter was accepted"},
        {entryWrites({0xbe8e037eu}), "descriptor Phi: a T# holding EXEC was accepted"},
        {entryWrites({0x7d840080u, 0xbe8e036au}), "descriptor Phi: a T# holding a compare mask was accepted"},
    }};
    for (const auto& [code, message] : invalidEntries) {
        auto request = makeRequest(code);
        expectFailure([&] { static_cast<void>(GetResourcePlan(request)); }, "GetImageResource dword 0 is not a valid runtime value", message);
    }
    auto unwritten = loopCode;
    unwritten[4] = 0xf4080200u;
    auto unwrittenRequest = makeRequest(unwritten);
    require(GetResourcePlan(unwrittenRequest)->info.images.size() == 3u, "descriptor Phi: a T# whose unwritten registers read as 0 was not planned as its own image");

    const std::array<std::uint32_t, 7> prologue{0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu};
    const std::array<std::uint32_t, 7> epilogue{0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const auto planImages = [&](std::span<const std::uint32_t> body) {
        std::vector<std::uint32_t> code(prologue.begin(), prologue.end());
        code.insert(code.end(), body.begin(), body.end());
        code.insert(code.end(), epilogue.begin(), epilogue.end());
        auto request = makeRequest(code);
        return GetResourcePlan(request)->info.images.size();
    };
    const auto distinctLoads = [](std::uint32_t loads) {
        std::vector<std::uint32_t> body;
        for (std::uint32_t load = 0; load < loads; load++) {
            body.insert(body.end(), {0xbf068014u, 0xbf850002u, 0xf40c0200u, 0xfa000060u + load * 0x20u});
        }
        return body;
    };
    require(planImages(distinctLoads(63u)) == 64u, "descriptor Phi: 64 distinct T#s were not given one image each");
    expectFailure([&] { static_cast<void>(planImages(distinctLoads(64u))); }, "GetImageResource dword 0 is not a valid runtime value", "descriptor Phi: more than 64 distinct T#s were accepted");

    const auto splitReloads = [](std::uint32_t joins) {
        std::vector<std::uint32_t> body;
        for (std::uint32_t join = 0; join < joins; join++) {
            body.insert(body.end(), {0xbf068014u, 0xbf850003u, 0xf40003c0u, join % 2u == 0u ? 0xfa000088u : 0xfa00008cu, 0xbf820006u, 0xf4080200u, 0xfa000060u, 0xf4040300u, 0xfa000070u, 0xf4000380u, 0xfa000078u});
        }
        return body;
    };
    require(planImages(splitReloads(8u)) == 4u, "descriptor Phi: separate reloads of T# dword 7 and dwords 0-6 were not split into their 4 T#s");
    expectFailure([&] { static_cast<void>(planImages(splitReloads(260u))); }, "GetImageResource dword 0 is not a valid runtime value", "descriptor Phi: a web over more than 512 descriptor tuples was accepted");
}

void verifyUniformTableImage() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    struct alignas(4096) GuestTables {
        std::array<std::array<std::uint32_t, 12>, 3> table{};
        std::array<std::uint32_t, 4> output{};
        std::array<std::uint32_t, 16> srt{};
    };
    static GuestTables guest;
    const auto texture = [](const Texture& image) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(image.bytes.data()));
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    const auto first = texture(textures[0]);
    const auto second = texture(textures[1]);
    std::copy(first.begin(), first.end(), guest.table[1].begin() + 4);
    std::copy(second.begin(), second.end(), guest.table[2].begin() + 4);
    const auto bufferDescriptor = [](const void* base, std::uint32_t records) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), records, 0xfacu};
    };
    const auto tableV = bufferDescriptor(guest.table.data(), static_cast<std::uint32_t>(sizeof(guest.table)));
    const auto outputV = bufferDescriptor(guest.output.data(), 16u);
    std::copy(tableV.begin(), tableV.end(), guest.srt.begin());
    std::copy(outputV.begin(), outputV.end(), guest.srt.begin() + 12);
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.srt.data()));
    std::array<std::uint32_t, 3> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u), 2u};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const std::vector<std::uint32_t> code{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000030u, 0x9310b002u, 0xf42c0502u, 0x20000010u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    const auto plan = GetResourcePlan(request);
    const auto& column = plan->descriptorSources.at(plan->info.images.at(0).source).tableColumn;
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u && column.has_value() && column->stride == 48u && column->dwordCount == 8u, "uniform table image: the T# read from a constant buffer at a user data offset was not planned as one table column");
    for (const auto& [key, expected] : {std::pair{2u, second}, std::pair{1u, first}}) {
        userData[2] = key;
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        const auto& tables = capture->snapshot.tables;
        require(tables.tables.size() == 1u && key < tables.tables[0].codes.size(), "uniform table image: the capture has no table for the user data key");
        const auto code = tables.tables[0].codes[key];
        require((code & ImageTableAbi::PoisonFlag) == 0u && std::equal(expected.begin(), expected.end(), tables.words.at(code).dwords.begin()), "uniform table image: the T# of the record the user data selects was not read");
    }
}

void verifyLaneSelectedDescriptors() {
    std::vector<std::uint32_t> code{0xf40c0200u, 0xfa000000u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf4000400u, 0xfa000050u, 0xf40c0800u, 0xfa000060u, 0xbf8cc07fu, 0x7d880090u, 0xbf068010u, 0xbf850011u};
    for (std::uint32_t dword = 0; dword < 8u; dword++) {
        const auto vgpr = 1u + dword;
        code.push_back(0x7e000200u | (vgpr << 17u) | (32u + dword));
        code.push_back(0x02000000u | (vgpr << 17u) | (vgpr << 9u) | (8u + dword));
    }
    code.push_back(0xbf820008u);
    for (std::uint32_t dword = 0; dword < 8u; dword++) {
        code.push_back(0x7e000200u | ((1u + dword) << 17u) | (8u + dword));
    }
    for (std::uint32_t dword = 0; dword < 8u; dword++) {
        code.push_back(0x7e000400u | ((8u + dword) << 17u) | (257u + dword));
    }
    code.insert(code.end(), {0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u});
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    const auto imageDescriptor = [&](const Texture& texture) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    std::array<std::uint32_t, 32> srt{};
    const auto first = imageDescriptor(textures[0]);
    const auto second = imageDescriptor(textures[1]);
    std::copy(first.begin(), first.end(), srt.begin());
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
    srt[20] = 1u;
    std::copy(second.begin(), second.end(), srt.begin() + 24);
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x22000u, code, 0, {}};
    request.context.waveSize = 32u;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 2u && plan->info.samplers.size() == 1u, "lane-selected T#: the T#s selected per lane were not planned as one image each");
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    request.context.memory = memory.Regions();
    static_cast<void>(Recompile(request, *capture));
    const auto holds = [&](const std::array<std::uint32_t, 8>& words) {
        return std::ranges::any_of(capture->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(holds(first) && holds(second), "lane-selected T#: the snapshot T#s are not the two SRT T#s");
}

void verifyProgramCounterRelativeData() {
    using namespace ShaderRecompiler;
    static const std::array<std::uint32_t, 15> code{
        0xbe801f00u,
        0x800000ffu, 52u,
        0x82010180u,
        0xb0020010u,
        0xbe8303ffu, 0x10005004u,
        0xf4200100u, 0xfa000000u,
        0xbf8cc07fu,
        0x7e000204u,
        0xf80008cfu, 0u,
        0xbf810000u,
        0x3f800000u
    };
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, codeAddress, code, 0, {}};
    request.target = BufferTarget();
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 8;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    const auto dataBase = [](const RecompileResult& result) {
        for (const auto& binding : result.bindings) {
            if (binding.role != DescriptorRole::GuestBuffers || binding.guestDescriptor.size() < 4u) continue;
            return static_cast<std::uint64_t>(binding.guestDescriptor[0]) | (static_cast<std::uint64_t>(binding.guestDescriptor[1] & 0xffffu) << 32u);
        }
        throw std::runtime_error("program counter data: no guest buffer was bound");
    };
    AgcDriver::ShaderMemory memory({});
    static_cast<void>(memory.Capture(request));
    request.context.memory = memory.Regions();
    const auto first = Recompile(request);
    require(dataBase(first) == codeAddress + 56u, "program counter data: the V# does not name the data at the shader's address");
    auto relocated = request;
    relocated.shader.codeAddress += 0x1000u;
    const auto moved = Recompile(relocated);
    require(moved.cacheHit, "program counter data: relocating the shader recompiled it");
    require(dataBase(moved) == codeAddress + 0x1000u + 56u, "program counter data: the relocated shader bound the old address");
}

void verifyLanesOutsideHostSubgroup() {
    using namespace ShaderRecompiler;
    const auto pixel = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize) {
        ShaderPixelStageInfo info{};
        info.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
        info.hasPerspectiveCenterVgpr = true;
        info.targetOutputMode[0] = 9u;
        info.targetExportMapping.fill(0xe4u);
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.pixel = info;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return !Recompile(request).spirv.empty();
    };
    const auto compute = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize) {
        static constexpr std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return !Recompile(request).spirv.empty();
    };
    static constexpr std::array<std::uint32_t, 6> lane63{0xd7600006u, 0x00017f00u, 0x7e020206u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 6> lane31{0xd7600006u, 0x00013f00u, 0x7e020206u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 11> waterfall{0xbe80047eu, 0xbe821400u, 0xd7600003u, 0x00000500u, 0xbe801c02u, 0xbf138000u, 0xbf85fffau, 0x7e020203u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 6> computeLane63{0xd7600006u, 0x00017f00u, 0x7e020206u, 0xe0700000u, 0x80000100u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 5> permlanex16{0xd7780001u, 0x02010100u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 5> permlane16{0xd7770001u, 0x02010100u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    expectFailure([&] { static_cast<void>(pixel(lane63, 32u)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup that runs this wave64 program at one lane per invocation", "host lanes: a wave64 pixel program read lane 63 of a 32-lane subgroup");
    expectFailure([&] { static_cast<void>(pixel(lane63, 8u)); }, "v_readlane_b32 of lane 63 is outside the 8-lane host subgroup", "host lanes: a wave64 pixel program read lane 63 of an 8-lane subgroup");
    require(pixel(lane31, 32u), "host lanes: a wave64 pixel program did not build reading lane 31 of a 32-lane subgroup");
    expectFailure([&] { static_cast<void>(pixel(lane31, 8u)); }, "v_readlane_b32 of lane 31 is outside the 8-lane host subgroup", "host lanes: a wave64 pixel program read lane 31 of an 8-lane subgroup");
    require(pixel(lane63, 64u), "host lanes: a wave64 pixel program did not build reading lane 63 of a 64-lane subgroup");
    require(compute(computeLane63, 32u), "host lanes: a wave64 compute program did not build reading lane 63 at two lanes per invocation");
    require(compute(computeLane63, 8u), "host lanes: a wave64 compute program did not build reading lane 63 of an 8-lane subgroup");
    require(pixel(waterfall, 32u) && pixel(waterfall, 8u), "host lanes: an s_ff1_i32_b64 waterfall reading a lane in a register did not build");
    expectFailure([&] { static_cast<void>(pixel(permlanex16, 8u)); }, "v_permlanex16_b32 from lanes 16-31 is outside the 8-lane host subgroup", "host lanes: v_permlanex16_b32 read lanes 16-31 of an 8-lane subgroup");
    require(pixel(permlanex16, 32u) && pixel(permlane16, 8u), "host lanes: a v_permlane16_b32 row that the host subgroup may hold did not build");
}

void verifyHalfWaveReduction() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic, spv::CapabilityClipDistance};
    const auto pixel = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize, bool arithmetic) {
        ShaderPixelStageInfo info{};
        info.inputAddr = PixelInputBit(PixelInput::PositionX);
        info.posX = true;
        info.targetOutputMode[0] = 9u;
        info.targetExportMapping.fill(0xe4u);
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.pixel = info;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        if (arithmetic) request.target.supportedCapabilities = capabilities;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    const auto reductions = [](std::span<const std::uint32_t> spirv, spv::Op opcode) {
        std::uint32_t count = 0;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            if ((spirv[cursor] & 0xffffu) == static_cast<std::uint32_t>(opcode) && (spirv[cursor] >> 16u) == 6u && spirv[cursor + 4u] == spv::GroupOperationReduce) ++count;
        }
        return count;
    };
    const auto halves = [](std::span<const std::uint32_t> spirv, spv::Op opcode, std::uint32_t identity) {
        std::map<std::uint32_t, std::size_t> definitions;
        std::vector<std::size_t> reduces;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            const auto op = spirv[cursor] & 0xffffu;
            if (op == spv::OpConstant || op == spv::OpSelect || op == spv::OpUGreaterThanEqual) definitions[spirv[cursor + 2u]] = cursor;
            if (op == static_cast<std::uint32_t>(opcode) && (spirv[cursor] >> 16u) == 6u && spirv[cursor + 4u] == spv::GroupOperationReduce) reduces.push_back(cursor);
        }
        const auto defined = [&](std::uint32_t id, spv::Op op) -> std::size_t {
            const auto found = definitions.find(id);
            return found != definitions.end() && (spirv[found->second] & 0xffffu) == static_cast<std::uint32_t>(op) ? found->second : 0u;
        };
        const auto constant = [&](std::uint32_t id, std::uint32_t value) {
            const auto at = defined(id, spv::OpConstant);
            return at != 0u && spirv[at + 3u] == value;
        };
        std::uint32_t lower = 0;
        std::uint32_t upper = 0;
        std::vector<std::uint32_t> keys;
        for (const auto reduce : reduces) {
            const auto select = defined(spirv[reduce + 5u], spv::OpSelect);
            const auto compare = select != 0u ? defined(spirv[select + 3u], spv::OpUGreaterThanEqual) : 0u;
            if (compare == 0u || !constant(spirv[compare + 4u], 32u)) continue;
            if (constant(spirv[select + 4u], identity)) {
                ++lower;
                keys.push_back(spirv[select + 5u]);
            } else if (constant(spirv[select + 5u], identity)) {
                ++upper;
                keys.push_back(spirv[select + 4u]);
            }
        }
        return reduces.size() == 2u && lower == 1u && upper == 1u && keys[0] == keys[1];
    };
    const auto scan = [](std::uint32_t identity, std::uint32_t vector, std::uint32_t scalar) {
        std::vector<std::uint32_t> code{0xbe98047eu, 0x7e160f00u, 0x8786187eu, 0xbeea287eu, 0xd501000du};
        if (identity == 0u || identity == 0xffffffffu) code.push_back(identity == 0u ? 0x001a1680u : 0x001a16c1u);
        else code.insert(code.end(), {0x001a16ffu, identity});
        for (const auto control : {0xff01110du, 0xff01120du, 0xff01140du, 0xff01180du}) code.insert(code.end(), {vector | 0x001a1afau, control});
        code.insert(code.end(), {0xd778100cu, 0x0305830du, vector | 0x001a190du, 0xbefe046au, 0xd7600006u, 0x00013f0du, 0xd7600007u, 0x00017f0du, scalar | 0x00080706u, 0x7e020208u, 0xf800180fu, 0x01010101u, 0xbf810000u});
        return code;
    };
    struct Reduction {
        std::uint32_t identity;
        std::uint32_t vector;
        std::uint32_t scalar;
        spv::Op reduce;
        const char* message;
    };
    static constexpr std::array<Reduction, 7> scans{{
        {0xffffffffu, 0x26000000u, 0x83800000u, spv::OpGroupNonUniformUMin, "half-wave reduction: the wave64 UMin scan did not read lane 31 as a subgroup UMin on 32 lanes"},
        {0u, 0x28000000u, 0x84800000u, spv::OpGroupNonUniformUMax, "half-wave reduction: the wave64 UMax scan did not read lane 31 as a subgroup UMax on 32 lanes"},
        {0x7fffffffu, 0x22000000u, 0x83000000u, spv::OpGroupNonUniformSMin, "half-wave reduction: the wave64 SMin scan did not read lane 31 as a subgroup SMin on 32 lanes"},
        {0x80000000u, 0x24000000u, 0x84000000u, spv::OpGroupNonUniformSMax, "half-wave reduction: the wave64 SMax scan did not read lane 31 as a subgroup SMax on 32 lanes"},
        {0u, 0x4a000000u, 0x80000000u, spv::OpGroupNonUniformIAdd, "half-wave reduction: the wave64 IAdd scan did not read lane 31 as a subgroup IAdd on 32 lanes"},
        {0xffffffffu, 0x36000000u, 0x87000000u, spv::OpGroupNonUniformBitwiseAnd, "half-wave reduction: the wave64 AND scan did not read lane 31 as a subgroup AND on 32 lanes"},
        {0u, 0x38000000u, 0x88000000u, spv::OpGroupNonUniformBitwiseOr, "half-wave reduction: the wave64 OR scan did not read lane 31 as a subgroup OR on 32 lanes"},
    }};
    for (const auto& reduction : scans) require(reductions(pixel(scan(reduction.identity, reduction.vector, reduction.scalar), 32u, true), reduction.reduce) == 1u, reduction.message);
    auto comparedScan = scan(0u, 0x38000000u, 0x88000000u);
    *std::find(comparedScan.begin(), comparedScan.end(), 0xbe98047eu) = 0x8a986a7eu;
    comparedScan.insert(comparedScan.begin(), 0x7c080080u);
    require(reductions(pixel(comparedScan, 32u, true), spv::OpGroupNonUniformBitwiseOr) == 1u, "half-wave reduction: an OR scan keyed by the lanes that failed a compare did not read lane 31 as a subgroup OR on 32 lanes");
    auto narrowedScan = comparedScan;
    std::vector<std::uint32_t> narrowing;
    for (std::uint32_t step = 1; step <= 600u; ++step) {
        const auto threshold = -static_cast<float>(step);
        std::uint32_t bits = 0;
        std::memcpy(&bits, &threshold, sizeof(bits));
        narrowing.insert(narrowing.end(), {0x7c0800ffu, bits, 0x87fe6a7eu});
    }
    narrowedScan.insert(narrowedScan.begin(), narrowing.begin(), narrowing.end());
    require(reductions(pixel(narrowedScan, 32u, true), spv::OpGroupNonUniformBitwiseOr) == 1u, "half-wave reduction: an OR scan keyed by lanes that 600 compares narrowed exec to did not read lane 31 as a subgroup OR on 32 lanes");
    for (const auto& reduction : scans) {
        const auto wide = "half-wave reduction: a wave64 scan reduced by SPIR-V opcode " + std::to_string(reduction.reduce) + " did not read lanes 31 and 63 as reductions of host invocations 0-31 and 32-63 on 64 lanes";
        require(halves(pixel(scan(reduction.identity, reduction.vector, reduction.scalar), 64u, true), reduction.reduce, reduction.identity), wide.c_str());
    }
    const auto vertex = [](std::vector<std::uint32_t> code, std::uint32_t subgroupSize) {
        *std::find(code.begin(), code.end(), 0xf800180fu) = 0xf80008cfu;
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.supportedCapabilities = capabilities;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    require(reductions(vertex(scan(0xffffffffu, 0x26000000u, 0x83800000u), 32u), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: the wave64 vertex UMin scan, whose entry EXEC is every invocation, did not read lane 31 as a subgroup UMin on 32 lanes");
    require(halves(vertex(scan(0x80000000u, 0x24000000u, 0x84000000u), 64u), spv::OpGroupNonUniformSMax, 0x80000000u), "half-wave reduction: the wave64 vertex SMax scan did not read lanes 31 and 63 as reductions of host invocations 0-31 and 32-63 on 64 lanes");
    const auto domain = [](std::vector<std::uint32_t> code, std::uint32_t subgroupSize) {
        *std::find(code.begin(), code.end(), 0xf800180fu) = 0xf80008cfu;
        *std::find(code.begin(), code.end(), 0x7e160f00u) = 0x7e160f01u;
        RecompileRequest request{};
        request.shader = {ShaderStage::TessellationEvaluation, 0x10000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.supportedCapabilities = capabilities;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.graphics = GraphicsCompileContext{0u, {}, std::nullopt, TessellationConfiguration{3u, 3u, 1u, 2u, 2u}, {}};
        request.useCache = false;
        return Recompile(request).spirv;
    };
    require(reductions(domain(scan(0xffffffffu, 0x26000000u, 0x83800000u), 32u), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: the wave64 domain UMin scan under s_orn2_saveexec_b64 vcc, exec did not read lane 31 as a subgroup UMin on 32 lanes");
    require(halves(domain(scan(0u, 0x38000000u, 0x88000000u), 64u), spv::OpGroupNonUniformBitwiseOr, 0u), "half-wave reduction: the wave64 domain OR scan did not read lanes 31 and 63 as reductions of host invocations 0-31 and 32-63 on 64 lanes");
    const auto floatScan = [&scan](std::uint32_t identity, std::uint32_t vector, std::uint32_t join) {
        auto code = scan(identity, vector, 0u);
        code[23] = join;
        code[24] = 0x00000e06u;
        return code;
    };
    const auto fmax = floatScan(0xff800000u, 0x20000000u, 0xd5100001u);
    const auto fmin = floatScan(0x7f800000u, 0x1e000000u, 0xd50f0001u);
    for (const auto& code : {fmax, fmin}) {
        const auto narrow = pixel(code, 32u, false);
        require(!narrow.empty() && reductions(narrow, spv::OpGroupNonUniformFMax) == 0u && reductions(narrow, spv::OpGroupNonUniformFMin) == 0u, "half-wave reduction: a wave64 v_max_f32/v_min_f32 scan did not read lane 63 as its -inf/+inf keys on 32 lanes");
        require(!pixel(code, 64u, false).empty(), "half-wave reduction: a wave64 v_max_f32/v_min_f32 scan did not build on 64 lanes");
    }
    auto fmaxUnderExec = fmax;
    fmaxUnderExec[3] = 0xbeea047eu;
    expectFailure([&] { static_cast<void>(pixel(fmaxUnderExec, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a v_max_f32 scan under the entry EXEC read lane 63");
    auto fmaxJoinedByMin = fmax;
    fmaxJoinedByMin[17] = 0x1e000000u | 0x001a190du;
    expectFailure([&] { static_cast<void>(pixel(fmaxJoinedByMin, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a v_max_f32 row scan joined by v_min_f32 read lane 63");
    auto fmaxOfZero = fmax;
    fmaxOfZero[6] = 0u;
    expectFailure([&] { static_cast<void>(pixel(fmaxOfZero, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a v_max_f32 scan of 0 outside the live lanes read lane 63");
    const auto umin = scan(0xffffffffu, 0x26000000u, 0x83800000u);
    const auto patched = [&umin](std::initializer_list<std::pair<std::size_t, std::uint32_t>> words) {
        auto code = umin;
        for (const auto& [index, word] : words) code[index] = word;
        return code;
    };
    require(reductions(pixel(patched({{3, 0xbeea25c1u}}), 32u, true), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: the scan under s_or_saveexec_b64 -1 did not read lane 31 as a subgroup UMin");
    const char* lane63 = "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup";
    auto divergent = umin;
    divergent.insert(divergent.begin() + 3, 0x88fe1a7eu);
    divergent.insert(divergent.begin() + 2, {0xbe860480u, 0x7d821688u, 0xbe9a246au, 0xbf880001u});
    require(reductions(pixel(divergent, 32u, true), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: keys masked inside a divergent branch did not read lane 31 as a subgroup UMin");
    auto windowed = umin;
    windowed.insert(windowed.begin() + 23, 0x7e008514u);
    require(reductions(pixel(windowed, 32u, true), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: a UMin scan of registers in a v_movreld_b32 window did not read lane 31 as a subgroup UMin on 32 lanes");
    require(halves(pixel(windowed, 64u, true), spv::OpGroupNonUniformUMin, 0xffffffffu), "half-wave reduction: a UMin scan of registers in a v_movreld_b32 window did not read lanes 31 and 63 as reductions of host invocations 0-31 and 32-63 on 64 lanes");
    auto overwritten = umin;
    overwritten.insert(overwritten.begin() + 17, {0x7ef80500u, 0x7e008514u});
    expectFailure([&] { static_cast<void>(pixel(overwritten, 32u, true)); }, lane63, "half-wave reduction: a scan of window registers read lane 63 after a v_movreld_b32 at a runtime M0 that may overwrite them");
    require(reductions(domain(divergent, 32u), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: domain keys masked inside a divergent branch did not read lane 31 as a subgroup UMin on 32 lanes");
    const char* arithmetic = "v_readlane_b32 of lane 31 of a wave64 half-wave reduction scan needs subgroup arithmetic";
    expectFailure([&] { static_cast<void>(pixel(umin, 32u, false)); }, arithmetic, "half-wave reduction: a device without subgroup arithmetic read lane 31 on 32 lanes");
    expectFailure([&] { static_cast<void>(pixel(umin, 64u, false)); }, arithmetic, "half-wave reduction: a device without subgroup arithmetic read lane 31 on 64 lanes");
    expectFailure([&] { static_cast<void>(pixel(umin, 128u, true)); }, "half-wave reduction scan on a 128-lane host subgroup, which is wider than the wave", "half-wave reduction: the scan built on a 128-lane subgroup");
    require(reductions(pixel(patched({{3, 0xbeea047eu}}), 64u, true), spv::OpGroupNonUniformUMin) == 0u, "half-wave reduction: a scan under the entry EXEC became a subgroup UMin on 64 lanes");
    expectFailure([&] { static_cast<void>(pixel(patched({{3, 0xbeea047eu}}), 32u, true)); }, lane63, "half-wave reduction: a scan under the entry EXEC read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{5, 0x001a1680u}}), 32u, true)); }, lane63, "half-wave reduction: a UMin scan of 0 outside the live lanes read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{0, 0xbe980a7eu}, {2, 0xbe860418u}}), 32u, true)); }, lane63, "half-wave reduction: a scan of keys under an s_wqm_b64 mask read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{7, 0xff09110du}}), 32u, true)); }, lane63, "half-wave reduction: a bound_ctrl row_shr:1 step read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{15, 0x0305010du}}), 32u, true)); }, lane63, "half-wave reduction: a v_permlanex16_b32 of lane 0 read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{16, 0x281a190du}}), 32u, true)); }, lane63, "half-wave reduction: a UMin row scan joined by v_max_u32 read lane 63");
    auto threeSteps = umin;
    threeSteps.erase(threeSteps.begin() + 12, threeSteps.begin() + 14);
    expectFailure([&] { static_cast<void>(pixel(threeSteps, 32u, true)); }, lane63, "half-wave reduction: a scan without row_shr:8 read lane 63");
    auto compared = umin;
    compared.erase(compared.begin() + 2);
    compared.insert(compared.begin() + 3, {0xd4c50006u, 0x00021680u});
    expectFailure([&] { static_cast<void>(pixel(compared, 32u, true)); }, lane63, "half-wave reduction: a scan selected by a compare under the all-lanes EXEC read lane 63");
    expectFailure([&] { static_cast<void>(pixel(umin, 8u, true)); }, "v_permlanex16_b32 from lanes 16-31 is outside the 8-lane host subgroup", "half-wave reduction: the scan built on an 8-lane subgroup");
}

void verifyNarrowVertexWave() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic, spv::CapabilityClipDistance};
    static constexpr std::array<std::uint32_t, 4> userData{0u, 0u, 0u, 0x2020u};
    const auto vertex = [](std::span<const std::uint32_t> code, std::uint32_t waveSize, std::uint32_t subgroupSize, std::uint32_t userDataBase) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x50000u, code, 0, {}};
        request.context.waveSize = waveSize;
        request.context.userDataBaseRegister = userDataBase;
        if (userDataBase == 0u) request.context.userData = userData;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.supportedCapabilities = capabilities;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        return request;
    };
    const auto prepared = [&vertex](std::span<const std::uint32_t> code, std::uint32_t subgroupSize, std::uint32_t userDataBase = 8u) {
        return GetPreparedArtifact(*PrepareShader(vertex(code, 64u, subgroupSize, userDataBase))).spirv;
    };
    const auto prepares = [&prepared](std::span<const std::uint32_t> code, std::uint32_t subgroupSize, std::uint32_t userDataBase = 8u) {
        return !prepared(code, subgroupSize, userDataBase).empty();
    };
    const auto reductions = [](std::span<const std::uint32_t> spirv) {
        std::uint32_t count = 0;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            if ((spirv[cursor] & 0xffffu) == spv::OpGroupNonUniformBitwiseOr && (spirv[cursor] >> 16u) == 6u && spirv[cursor + 4u] == spv::GroupOperationReduce) ++count;
        }
        return count;
    };
    const std::vector<std::uint32_t> culling{
        0xd7650015u, 0x000100c1u, 0xd7660015u, 0x00022ac1u, 0x7e0e0280u, 0x7da206f9u, 0x81060015u, 0x360e0a81u,
        0xbefe04c1u, 0xd4c50002u, 0x00020e80u, 0xd763000fu, 0x00022a81u, 0xd5010000u, 0x000a1e80u,
        0x380000fau, 0xff091100u, 0x380000fau, 0xff091200u, 0x380000fau, 0xff091400u, 0x380000fau, 0xff091800u,
        0xd778000fu, 0x03058300u, 0x38001f00u, 0xd7600002u, 0x00013f00u, 0xd760006bu, 0x00017f00u, 0x88026b02u,
        0x7e020202u, 0xf80008cfu, 0x01010101u, 0xbf810000u,
    };
    const auto patched = [&culling](std::initializer_list<std::pair<std::size_t, std::uint32_t>> words) {
        auto code = culling;
        for (const auto& [index, word] : words) code[index] = word;
        return code;
    };
    const char* lane63 = "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup";
    require(reductions(prepared(culling, 32u)) == 1u, "narrow vertex wave: the NGG culling scan, keyed on lanes below the primitive count in s3, did not read lane 31 as a subgroup OR on 32 lanes");
    const auto wide64 = prepared(culling, 64u);
    require(!wide64.empty() && reductions(wide64) == 0u, "narrow vertex wave: the NGG culling scan, keyed on lanes below 64 on 64 lanes, did not keep its DPP scan");
    require(prepares(patched({{6, 0x80060015u}}), 32u), "narrow vertex wave: the scan keyed on lanes below the vertex count in s3 did not prepare on 32 lanes");
    static constexpr std::array<std::uint32_t, 4> noArithmetic{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityClipDistance};
    const auto otherKeys = patched({{7, 0x360e0a82u}});
    auto withoutArithmetic = vertex(otherKeys, 64u, 32u, 8u);
    withoutArithmetic.target.supportedCapabilities = noArithmetic;
    const auto shuffled = GetPreparedArtifact(*PrepareShader(withoutArithmetic)).spirv;
    require(!shuffled.empty() && reductions(shuffled) == 0u, "narrow vertex wave: without subgroup arithmetic, the scan did not keep lane 31 as a shuffle with lane 63 read as 0");
    require(prepares(patched({{5, 0x7da82aa0u}, {6, 0xbf800000u}}), 32u), "narrow vertex wave: the scan keyed on lanes below 32 did not prepare on 32 lanes");
    expectFailure([&] { static_cast<void>(prepares(patched({{5, 0x7da82aa1u}, {6, 0xbf800000u}}), 32u)); }, lane63, "narrow vertex wave: a scan keyed on lanes below 33 read lane 63");
    expectFailure([&] { static_cast<void>(prepares(culling, 32u, 0u)); }, lane63, "narrow vertex wave: a scan keyed on a count from user data read lane 63");
    expectFailure([&] { static_cast<void>(prepares(patched({{6, 0x81060005u}}), 32u)); }, lane63, "narrow vertex wave: a scan keyed on the vertex index instead of the lane read lane 63");
    auto otherRegister = patched({{21, 0x380200fau}, {24, 0x03058301u}, {25, 0x38001f01u}});
    otherRegister.insert(otherRegister.begin(), 0x7e020305u);
    require(reductions(prepared(otherRegister, 32u)) == 1u, "narrow vertex wave: a scan whose bound_ctrl:1 step is written to another register did not read lane 31 as a subgroup OR on 32 lanes");
    otherRegister[23] = 0xff011800u;
    expectFailure([&] { static_cast<void>(prepares(otherRegister, 32u)); }, lane63, "narrow vertex wave: a scan step written to another register without bound_ctrl:1, which keeps that register's lanes outside the row, read lane 63");
    auto threeSteps = culling;
    threeSteps.erase(threeSteps.begin() + 21, threeSteps.begin() + 23);
    const auto partialScan = prepared(threeSteps, 32u);
    require(!partialScan.empty() && reductions(partialScan) == 0u, "narrow vertex wave: lane 63 of row_shr 1/2/4 steps over keys of 0 in lanes 32-63 did not read 0, or lane 31 became a subgroup OR");
    threeSteps[5] = 0x7da82aa1u;
    threeSteps[6] = 0xbf800000u;
    expectFailure([&] { static_cast<void>(prepares(threeSteps, 32u)); }, lane63, "narrow vertex wave: lane 63 of row_shr 1/2/4 steps over keys of lanes below 33 read 0");

    std::array<std::vector<std::uint32_t>, 2> narrow;
    std::array<std::vector<std::uint32_t>, 2> wide;
    for (const bool narrowFirst : {false, true}) {
        const std::vector<std::uint32_t> code{0xbf800001u + static_cast<std::uint32_t>(narrowFirst), 0x7e020203u, 0xf80008cfu, 0x01010101u, 0xbf810000u};
        const auto narrowRequest = vertex(code, 64u, 32u, 8u);
        const auto wideRequest = vertex(code, 64u, 64u, 8u);
        ShaderPreparationContext preparation;
        const auto first = PrepareShader(narrowFirst ? narrowRequest : wideRequest, &preparation);
        const auto second = PrepareShader(narrowFirst ? wideRequest : narrowRequest, &preparation);
        const auto& narrowHandle = narrowFirst ? *first : *second;
        const auto& wideHandle = narrowFirst ? *second : *first;
        require(first->source != second->source, "narrow vertex wave: the program prepared for 32 and for 64 lanes shares one translated source");
        require(MatchesPreparedShader(narrowRequest, narrowHandle) && MatchesPreparedShader(wideRequest, wideHandle), "narrow vertex wave: a prepared program did not match its own request");
        require(!MatchesPreparedShader(narrowRequest, wideHandle) && !MatchesPreparedShader(wideRequest, narrowHandle), "narrow vertex wave: a program prepared for one subgroup width matched a request for the other");
        narrow[narrowFirst ? 1u : 0u] = GetPreparedArtifact(narrowHandle).spirv.Copy();
        wide[narrowFirst ? 1u : 0u] = GetPreparedArtifact(wideHandle).spirv.Copy();
    }
    require(narrow[0] == narrow[1] && wide[0] == wide[1], "narrow vertex wave: preparing for one subgroup width after the other gave another program than preparing it first");
    require(narrow[0] != wide[0], "narrow vertex wave: s3 of a wave64 vertex program was the same on 32 and 64 lanes");
    const std::vector<std::uint32_t> wave32{0x7e020203u, 0xf80008cfu, 0x01010101u, 0xbf810000u};
    require(GetPreparedArtifact(*PrepareShader(vertex(wave32, 32u, 32u, 8u))).spirv == GetPreparedArtifact(*PrepareShader(vertex(wave32, 32u, 64u, 8u))).spirv, "narrow vertex wave: s3 of a wave32 vertex program depended on the subgroup width");
}

void verifySingleLaneVertexWave() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic, spv::CapabilityClipDistance};
    const auto vertex = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize, bool vertexSubgroups) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x58000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 8;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.supportedCapabilities = capabilities;
        request.target.vertexSubgroups = vertexSubgroups;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return request;
    };
    const auto compiled = [&vertex](std::span<const std::uint32_t> code, std::uint32_t subgroupSize, bool vertexSubgroups) {
        return Recompile(vertex(code, subgroupSize, vertexSubgroups)).spirv;
    };
    const auto usesSubgroups = [](std::span<const std::uint32_t> spirv) {
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            const auto op = spirv[cursor] & 0xffffu;
            if (op == spv::OpCapability && spirv[cursor + 1u] >= spv::CapabilityGroupNonUniform && spirv[cursor + 1u] <= spv::CapabilityGroupNonUniformQuad) return true;
            if (op >= spv::OpGroupNonUniformElect && op <= spv::OpGroupNonUniformQuadSwap) return true;
            if (op == spv::OpDecorate && spirv[cursor + 2u] == spv::DecorationBuiltIn && (spirv[cursor + 3u] == spv::BuiltInSubgroupLocalInvocationId || spirv[cursor + 3u] == spv::BuiltInSubgroupSize)) return true;
        }
        return false;
    };
    const auto hasConstant = [](std::span<const std::uint32_t> spirv, std::uint32_t value) {
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            if ((spirv[cursor] & 0xffffu) == spv::OpConstant && (spirv[cursor] >> 16u) == 4u && spirv[cursor + 3u] == value) return true;
        }
        return false;
    };
    static constexpr std::array<std::uint32_t, 4> waveInfo{0x7e020203u, 0xf80008cfu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 12> laneOps{
        0xd7650015u, 0x000100c1u, 0xd7660015u, 0x00022ac1u, 0x7e0c0500u, 0xd7600007u, 0x00010100u, 0x4a022a06u,
        0x4a020207u, 0xf80008cfu, 0x01010101u, 0xbf810000u,
    };
    static constexpr std::array<std::uint32_t, 11> waterfall{0xbe80047eu, 0xbe821400u, 0xd7600003u, 0x00000500u, 0xbe801c02u, 0xbf138000u, 0xbf85fffau, 0x7e020203u, 0xf80008cfu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 6> lane31{0xd7600006u, 0x00013f00u, 0x7e020206u, 0xf80008cfu, 0x01010101u, 0xbf810000u};

    const auto single = compiled(waveInfo, 32u, false);
    const auto wide = compiled(waveInfo, 64u, true);
    require(hasConstant(single, 0x0101u) && !hasConstant(single, 0x4040u) && !hasConstant(single, 0x2020u), "single-lane vertex wave: s3 of a vertex program without vertex subgroups was not one vertex and one primitive");
    require(hasConstant(wide, 0x4040u) && !hasConstant(wide, 0x0101u), "single-lane vertex wave: s3 of a vertex program with vertex subgroups on 64 lanes changed");
    require(single == compiled(waveInfo, 4u, false), "single-lane vertex wave: the program without vertex subgroups depended on the device subgroup size");
    for (const auto code : {std::span<const std::uint32_t>(laneOps), std::span<const std::uint32_t>(waterfall)}) {
        const auto withSubgroups = compiled(code, 32u, true);
        const auto withoutSubgroups = compiled(code, 32u, false);
        require(usesSubgroups(withSubgroups), "single-lane vertex wave: the program did not use subgroup operations with vertex subgroups");
        require(!withoutSubgroups.empty() && !usesSubgroups(withoutSubgroups), "single-lane vertex wave: a vertex program without vertex subgroups declared a subgroup capability, built-in or instruction");
    }
    expectFailure([&] { static_cast<void>(compiled(lane31, 32u, false)); }, "v_readlane_b32 of lane 31 is outside the 1-lane host subgroup", "single-lane vertex wave: a vertex program without vertex subgroups read lane 31");
    require(!compiled(lane31, 32u, true).empty(), "single-lane vertex wave: lane 31 of a 32-lane vertex subgroup did not build");

    ShaderPixelStageInfo info{};
    info.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
    info.hasPerspectiveCenterVgpr = true;
    info.targetOutputMode[0] = 9u;
    info.targetExportMapping.fill(0xe4u);
    static constexpr std::array<std::uint32_t, 6> pixelLane31{0xd7600006u, 0x00013f00u, 0x7e020206u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    RecompileRequest pixel{};
    pixel.shader = {ShaderStage::Fragment, 0x30000u, pixelLane31, 0, {}};
    pixel.context.waveSize = 64;
    pixel.context.pixel = info;
    pixel.target.vulkanVersion = 0x00401000u;
    pixel.target.spirvVersion = 0x00010300u;
    pixel.target.subgroupSize = 32u;
    pixel.target.vertexSubgroups = false;
    pixel.layout.pushConstantSizeBytes = 128;
    pixel.useCache = false;
    require(usesSubgroups(Recompile(pixel).spirv), "single-lane vertex wave: a pixel program stopped using subgroups when only the vertex stage lacks them");

    auto request = vertex(waveInfo, 32u, false);
    const auto replay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(request));
    require(!replay.request.target.vertexSubgroups && replay.request.target.subgroupSize == 32u, "single-lane vertex wave: missing vertex subgroups were lost in serialization");
    require(Recompile(replay.request).spirv == single, "single-lane vertex wave: the replayed request built another program");
    std::vector<std::uint64_t> withoutKey;
    std::vector<std::uint64_t> withKey;
    RecompileCacheKey::Build(request, withoutKey);
    request.target.vertexSubgroups = true;
    RecompileCacheKey::Build(request, withKey);
    require(withoutKey != withKey, "single-lane vertex wave: vertex subgroup support did not change the cache key");
}

void verifyMeshConfiguration() {
    using namespace ShaderRecompiler;
    ShaderMeshInputInfo list;
    list.inputPrimitive = 4u;
    require(list.InputPrimitiveSize() == 3u && list.InputPrimitiveStep() == 3u && list.InputVertexCount(21u) == 63u && list.InputPrimitiveCount(63u) == 21u && list.InputPrimitiveCount(2u) == 0u && list.InputVertexCount(0u) == 0u, "triangle list subgroup sizes changed");
    ShaderMeshInputInfo strip;
    strip.inputPrimitive = 6u;
    require(strip.InputPrimitiveSize() == 3u && strip.InputPrimitiveStep() == 1u && strip.InputVertexCount(21u) == 23u && strip.InputPrimitiveCount(23u) == 21u, "triangle strip subgroup sizes changed");
    ShaderMeshInputInfo fan;
    fan.inputPrimitive = 5u;
    require(fan.InputPrimitiveSize() == 3u && fan.InputPrimitiveStep() == 1u && fan.InputVertexCount(30u) == 32u && fan.InputPrimitiveCount(32u) == 30u && fan.InputPrimitiveCount(2u) == 0u, "triangle fan subgroup sizes changed");
    ShaderMeshInputInfo lines;
    lines.inputPrimitive = 2u;
    ShaderMeshInputInfo points;
    points.inputPrimitive = 1u;
    require(lines.InputPrimitiveSize() == 2u && lines.InputPrimitiveStep() == 2u && points.InputPrimitiveSize() == 1u && points.InputVertexCount(5u) == 5u, "line or point subgroup sizes changed");

    static constexpr std::array<std::uint32_t, 1> code{0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Mesh, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    const MeshConfiguration mesh{4u, 21u, 63u, 64u, 21u, 64u, 256u, 0u, 12u};
    request.graphics = GraphicsCompileContext{0u, {}, mesh, std::nullopt, {}};
    const auto replay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(request));
    require(replay.request.graphics.has_value() && replay.request.graphics->mesh.has_value() && replay.request.graphics->mesh->esgsItemSize == 12u && replay.request.graphics->mesh->primitivesPerGroup == 21u, "mesh configuration was lost in serialization");
    {
        RecompileRequest vertex{};
        vertex.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
        vertex.context.waveSize = 64;
        ShaderVertexStageInfo info{};
        info.paClVsOutCntl = 0x0040000fu;
        vertex.context.vertex = info;
        const auto back = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(vertex));
        require(back.request.context.vertex.has_value() && back.request.context.vertex->paClVsOutCntl == 0x0040000fu, "PA_CL_VS_OUT_CNTL was lost in serialization");
        std::vector<std::uint64_t> clipped;
        RecompileCacheKey::Build(vertex, clipped);
        vertex.context.vertex->paClVsOutCntl = 0;
        std::vector<std::uint64_t> unclipped;
        RecompileCacheKey::Build(vertex, unclipped);
        require(clipped != unclipped, "PA_CL_VS_OUT_CNTL is not part of the recompile cache key");
    }
    {
        RecompileRequest fragment{};
        fragment.shader = {ShaderStage::Fragment, 0x10000u, code, 0, {}};
        fragment.context.waveSize = 64;
        ShaderPixelStageInfo info{};
        info.quadPixelMask = 0x2u;
        fragment.context.pixel = info;
        const auto back = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(fragment));
        require(back.request.context.pixel.has_value() && back.request.context.pixel->quadPixelMask == 0x2u, "the quad pixel mask was lost in serialization");
        std::vector<std::uint64_t> masked;
        RecompileCacheKey::Build(fragment, masked);
        fragment.context.pixel->quadPixelMask = 0xfu;
        std::vector<std::uint64_t> full;
        RecompileCacheKey::Build(fragment, full);
        require(masked != full, "the quad pixel mask is not part of the recompile cache key");
        fragment.context.pixel->quadSampleMask = {0x1u, 0x2u, 0x0u, 0xffffu};
        const auto sampled = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(fragment));
        require(sampled.request.context.pixel.has_value() && sampled.request.context.pixel->quadSampleMask == std::array<std::uint16_t, 4>{0x1u, 0x2u, 0x0u, 0xffffu}, "the quad sample masks were lost in serialization");
        std::vector<std::uint64_t> partial;
        RecompileCacheKey::Build(fragment, partial);
        require(partial != full, "the quad sample masks are not part of the recompile cache key");
    }
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    const auto first = key;
    auto other = request;
    auto otherMesh = mesh;
    otherMesh.esgsItemSize = 16u;
    other.graphics = GraphicsCompileContext{0u, {}, otherMesh, std::nullopt, {}};
    RecompileCacheKey::Build(other, key);
    require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "the cache keys ignore the mesh configuration");
    require(mesh.outputPrimitiveSize == 3u && replay.request.graphics->mesh->outputPrimitiveSize == 3u, "a mesh configuration does not output triangles by default");
    otherMesh = mesh;
    otherMesh.outputPrimitiveSize = 2u;
    other.graphics = GraphicsCompileContext{0u, {}, otherMesh, std::nullopt, {}};
    const auto lineReplay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(other));
    require(lineReplay.request.graphics.has_value() && lineReplay.request.graphics->mesh.has_value() && lineReplay.request.graphics->mesh->outputPrimitiveSize == 2u, "the mesh output primitive size was lost in serialization");
    RecompileCacheKey::Build(other, key);
    require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "the cache keys ignore the mesh output primitive size");
    RecompileRequest hull{};
    hull.shader = {ShaderStage::TessellationControl, 0x10000u, code, 0, {}};
    hull.context.waveSize = 64;
    hull.graphics = GraphicsCompileContext{0u, {}, std::nullopt, TessellationConfiguration{4u, 4u, 2u, 2u, 3u, 63u, 63u, true}, {}};
    const auto hullReplay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(hull));
    const auto* replayed = hullReplay.request.graphics && hullReplay.request.graphics->tessellation ? &*hullReplay.request.graphics->tessellation : nullptr;
    require(replayed != nullptr && replayed->domain == 2u && replayed->outputTopology == 3u && replayed->patchesPerGroup == 63u && replayed->primitiveGroupSize == 63u && replayed->breakAtInstance, "the quad tessellation configuration, its patches per group, its primitive group size or BREAK_WAVE_AT_EOI were lost in serialization");
    auto grouped = hull;
    grouped.graphics->tessellation->patchesPerGroup = 62u;
    for (const auto stage : {ShaderStage::Local, ShaderStage::TessellationControl, ShaderStage::TessellationEvaluation}) {
        hull.shader.stage = stage;
        grouped.shader.stage = stage;
        RecompileCacheKey::Build(hull, key);
        const auto patchKey = key;
        RecompileCacheKey::Build(grouped, key);
        require((key != patchKey) == (stage != ShaderStage::Local), "only the hull and domain cache keys may depend on the patches per group, which bound their relative patch index");
    }
    auto primitiveGrouped = hull;
    primitiveGrouped.graphics->tessellation->primitiveGroupSize = 126u;
    for (const auto stage : {ShaderStage::Local, ShaderStage::TessellationControl, ShaderStage::TessellationEvaluation}) {
        hull.shader.stage = stage;
        primitiveGrouped.shader.stage = stage;
        RecompileCacheKey::Build(hull, key);
        const auto patchKey = key;
        RecompileCacheKey::Build(primitiveGrouped, key);
        require((key != patchKey) == (stage != ShaderStage::Local), "only the hull and domain cache keys may depend on the primitive group size, which rebuilds their relative patch index");
    }
    auto instanced = hull;
    instanced.graphics->tessellation->breakAtInstance = false;
    instanced.graphics->draw.instanceCount = 2u;
    instanced.graphics->draw.indexCount = 12u;
    auto longer = instanced;
    longer.graphics->draw.indexCount = 16u;
    auto breaking = instanced;
    breaking.graphics->tessellation->breakAtInstance = true;
    for (const auto stage : {ShaderStage::Local, ShaderStage::TessellationControl, ShaderStage::TessellationEvaluation}) {
        hull.shader.stage = stage;
        instanced.shader.stage = stage;
        longer.shader.stage = stage;
        breaking.shader.stage = stage;
        RecompileCacheKey::Build(hull, key);
        const auto single = key;
        RecompileCacheKey::Build(instanced, key);
        const auto shared = key;
        RecompileCacheKey::Build(longer, key);
        require((shared != single && key != shared) == (stage != ShaderStage::Local), "only the hull and domain cache keys may depend on the patches per instance of an instanced draw");
        RecompileCacheKey::Build(breaking, key);
        require(key == single, "an instanced draw whose instances start hull threadgroups (BREAK_WAVE_AT_EOI) changed the cache key");
    }
    breaking.shader.stage = ShaderStage::TessellationControl;
    const auto breakingReplay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(breaking));
    require(breakingReplay.request.graphics && breakingReplay.request.graphics->tessellation && breakingReplay.request.graphics->tessellation->breakAtInstance, "BREAK_WAVE_AT_EOI was lost in serialization");
    const std::array<std::uint32_t, 8> users{};
    request.context.waveSize = 32u;
    request.context.userData = users;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.subgroupSize = 32u;
    request.graphics->mesh = MeshConfiguration{1u, 1u, 1u, 1u, 1u, 32u, 256u, 0u, 4u};
    for (const std::uint32_t threads : {32u, 64u, 128u, 480u}) {
        request.graphics->mesh->threadsPerGroup = threads;
        require(PrepareResourceProgram(request).WaveSize() == 32u, "a wave32 mesh program did not preserve its guest wave size");
    }
    for (const std::uint32_t threads : {31u, 512u}) {
        request.graphics->mesh->threadsPerGroup = threads;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "mesh shader translation configuration is not supported", "mesh translation accepted a partial wave or overflowing wave count");
    }
}

ShaderRecompiler::ShaderPixelStageInfo twoParameterPixel() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 2u;
    pixel.interpolatorSettings[1] = 1u;
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::LinearCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.noPerspective = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping[0] = 0xe4u;
    return pixel;
}

std::string requestPrefix(std::string_view text, std::size_t bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result(text.substr(0, ((bytes + 2u) / 3u) * 4u));
    if (bytes % 3u == 1u) {
        result[result.size() - 3u] = alphabet[alphabet.find(result[result.size() - 3u]) & 0x30u];
        result[result.size() - 2u] = '=';
        result.back() = '=';
    } else if (bytes % 3u == 2u) {
        result[result.size() - 2u] = alphabet[alphabet.find(result[result.size() - 2u]) & 0x3cu];
        result.back() = '=';
    }
    return result;
}

void verifyCanonicalVersion20And21Requests() {
    using namespace ShaderRecompiler;
    struct Fixture { ShaderStage stage; std::uint32_t selector; bool subgroups; std::string_view text; };
    static constexpr std::array<Fixture, 12> fixtures{{
        {ShaderStage::Fragment, 0u, false,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAA="},
        {ShaderStage::Fragment, 0u, true,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAE="},
        {ShaderStage::Fragment, 1u, false,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEBAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAA="},
        {ShaderStage::Fragment, 1u, true,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEBAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAE="},
        {ShaderStage::Fragment, 2u, false,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQECAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAA="},
        {ShaderStage::Fragment, 2u, true,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQECAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAE="},
        {ShaderStage::Fragment, 3u, false,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAA="},
        {ShaderStage::Fragment, 3u, true,
            "NVNQQRQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAA"
            "AAF4VjQSAQEBOTAAAAE="},
        {ShaderStage::Vertex, 330245u, false,
            "NVNQQRQAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAAAAF4VjQS"
            "AQEBOTAAAAA="},
        {ShaderStage::Vertex, 330245u, true,
            "NVNQQRQAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAAAAF4VjQS"
            "AQEBOTAAAAE="},
        {ShaderStage::Vertex, 656650u, false,
            "NVNQQRQAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABCgUKAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAAAAF4VjQS"
            "AQEBOTAAAAA="},
        {ShaderStage::Vertex, 656650u, true,
            "NVNQQRQAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t"
            "3gAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABCgUKAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAAAAF4VjQS"
            "AQEBOTAAAAE="},
    }};
    constexpr std::array<std::uint32_t, 3> userData{0x13579bdfu, 0x2468ace0u, 0xdeadbeefu};
    constexpr std::array<std::byte, 4> bytes{std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    const RequestSerializer serializer;
    // Original serializers at 4bb2c7f394 (20) and 075169f7b3 (21) emitted these
    // twelve payloads independently; every pair differs only in its version header.
    for (const auto version : {20u, 21u}) {
        for (const auto& fixture : fixtures) {
            auto text = std::string(fixture.text);
            if (version == 21u) {
                constexpr std::string_view prefix = "NVNQQRUAAAA";
                text.replace(0, prefix.size(), prefix);
            }
            const auto legacy = serializer.Deserialize(text);
            const auto& request = legacy.request;
            require(request.shader.stage == fixture.stage && request.shader.codeAddress == 0x30000u && request.shader.code.size() == 1u && request.shader.code[0] == 0xbf810000u, "canonical version20/21 shader identity was misread");
            require(request.context.waveSize == 64u && request.context.userDataBaseRegister == 8u && request.context.userData.size() == userData.size() && std::equal(request.context.userData.begin(), request.context.userData.end(), userData.begin()), "canonical version20/21 guest context was misread");
            require(request.context.memory.size() == 1u && request.context.memory[0].guestAddress == 0x50000u && request.context.memory[0].bytes.size() == bytes.size() && std::equal(request.context.memory[0].bytes.begin(), request.context.memory[0].bytes.end(), bytes.begin()), "canonical version20/21 trailing memory was misread");
            require(request.layout.descriptorSet == 2u && request.layout.firstBinding == 11u && request.layout.pushConstantOffsetBytes == 16u && request.layout.pushConstantSizeBytes == 64u && !request.useCache, "canonical version20/21 trailing layout or cache flag was misread");
            require(request.target.vertexSubgroups == fixture.subgroups && request.target.float64DenormPreserve && request.target.registrationModules && request.target.registrationWordLimit == 12345u && request.target.nonConstantImageOffsets && request.target.srgbDecodeFormats == 0x12345678u && request.target.narrowSubgroupClock, "canonical version20/21 target fields were misread");
            if (fixture.stage == ShaderStage::Fragment) {
                require(request.context.pixel.has_value() && !request.context.vertex, "canonical version20/21 pixel presence was misread");
                const auto& pixel = *request.context.pixel;
                require(pixel.rasterPrimitive == static_cast<RasterPrimitive>(fixture.selector) && !pixel.layerExported && !pixel.ditheredAlphaToCoverage && pixel.alphaToCoverage && pixel.stencilRefExportEnable && pixel.targetOutputMode[0] == 9u && pixel.targetExportMapping[0] == 0xe4u, "canonical version20/21 pixel fields were misread");
            } else {
                require(request.context.vertex.has_value() && !request.context.pixel, "canonical version20/21 vertex presence was misread");
                const auto& vertex = *request.context.vertex;
                require(vertex.paClVsOutCntl == fixture.selector && vertex.fetchAttribReg == 9u && vertex.fetchBufferReg == 13u && vertex.fetchEmbedded, "canonical version20/21 vertex fields were misread");
            }
            const auto upgraded = serializer.Deserialize(serializer.Serialize(request));
            require(RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(upgraded.request), "upgrading canonical version20/21 changed shader identity");
            require(serializer.Serialize(upgraded.request) == serializer.Serialize(request), "upgrading canonical version20/21 changed request fields");
        }
    }
}
void verifyPixelRequestSerialization() {
    using namespace ShaderRecompiler;
    const auto fields = [](const ShaderPixelStageInfo& value) {
        return std::tie(value.interpolatorCount, value.interpolatorSettings, value.wave32, value.inputAddr,
                        value.hasPerspectiveCenterVgpr, value.perspectiveCentroid, value.posX, value.posY,
                        value.posZ, value.posW, value.frontFace, value.frontFaceAllBits, value.ancillary, value.sampleShading,
                        value.noPerspective, value.linearCentroid, value.pixelKillEnable, value.depthExportEnable,
                        value.sampleMaskExportEnable, value.earlyZ, value.executeOnNoop, value.conservativeZExport, value.orderedPixelShader,
                        value.targetOutputMode, value.targetExportMapping, value.alphaToCoverage, value.stencilRefExportEnable, value.rasterPrimitive, value.layerExported, value.ditheredAlphaToCoverage, value.reversedBlend);
    };
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    const std::array<std::uint32_t, 3> userData{0x12345678u, 0xabcdef01u, 0x87654321u};
    const std::array<MemoryRegion, 1> memory{{{0x60000u, std::as_bytes(std::span(userData))}}};
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userData = userData;
    request.context.memory = memory;
    request.context.vertex = ShaderVertexStageInfo{};
    request.context.vertex->fetchAttribReg = 17u;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.nonConstantImageOffsets = true;
    request.layout = {0u, 11u, 16u, 128u};
    request.useCache = false;
    ShaderPixelStageInfo pixel{
        .interpolatorCount = 32u,
        .wave32 = true,
        .inputAddr = 0x7fffu,
        .hasPerspectiveCenterVgpr = true,
        .perspectiveCentroid = true,
        .posX = true,
        .posY = true,
        .posZ = true,
        .posW = true,
        .frontFace = true,
        .frontFaceAllBits = true,
        .ancillary = true,
        .sampleShading = true,
        .noPerspective = true,
        .linearCentroid = true,
        .pixelKillEnable = true,
        .depthExportEnable = true,
        .sampleMaskExportEnable = true,
        .earlyZ = true,
        .executeOnNoop = true,
        .conservativeZExport = ConservativeZExport::GreaterThanZ,
        .orderedPixelShader = true,
        .targetOutputMode = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}
    };
    for (std::uint32_t i = 0; i < pixel.interpolatorSettings.size(); ++i) pixel.interpolatorSettings[i] = 0x10101010u + i;
    pixel.alphaToCoverage = true;
    pixel.stencilRefExportEnable = true;
    pixel.ditheredAlphaToCoverage = true;
    pixel.reversedBlend = 0x570001u;
    const std::array<std::array<std::uint8_t, 8>, 3> mappings{{
        {0x00u, 0xe4u, 0xc6u, 0x1bu, 0xffu, 0x80u, 0x55u, 0xaau},
        {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u},
        {0xc6u, 0x1bu, 0x00u, 0xffu, 0x55u, 0xaau, 0x80u, 0x39u}
    }};
    const RequestSerializer serializer;
    for (const auto& mapping : mappings) {
        pixel.targetExportMapping = mapping;
        request.context.pixel = pixel;
        const auto replay = serializer.Deserialize(serializer.Serialize(request));
        require(replay.request.context.pixel.has_value() && fields(*replay.request.context.pixel) == fields(pixel), "pixel fields or export mappings were lost in serialization");
        require(replay.request.context.vertex.has_value() && replay.request.context.vertex->fetchAttribReg == 17u && replay.request.context.memory.size() == 1u && replay.request.context.memory[0].guestAddress == 0x60000u && replay.request.context.memory[0].bytes.size() == sizeof(userData), "pixel mappings displaced the following guest context");
        require(replay.request.target.subgroupSize == 64u && replay.request.target.nonConstantImageOffsets && replay.request.layout.firstBinding == 11u && replay.request.layout.pushConstantOffsetBytes == 16u && !replay.request.useCache, "pixel mappings displaced the following request fields");
        std::vector<std::uint64_t> key;
        RecompileCacheKey::Build(request, key);
        std::vector<std::uint64_t> replayKey;
        RecompileCacheKey::Build(replay.request, replayKey);
        require(key == replayKey && RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(replay.request), "pixel replay changed shader identity");
        for (std::size_t i = 0; i < mapping.size(); ++i) {
            auto changed = request;
            changed.context.pixel->targetExportMapping[i] ^= 1u;
            RecompileCacheKey::Build(changed, replayKey);
            require(key == replayKey && RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(changed), "runtime pixel export mapping changed the static shader identity");
        }
        auto undithered = request;
        undithered.context.pixel->ditheredAlphaToCoverage = false;
        RecompileCacheKey::Build(undithered, replayKey);
        require(key != replayKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(undithered), "dithered coverage did not change shader identity");
        auto withoutReversedBlend = request;
        withoutReversedBlend.context.pixel->reversedBlend = 0u;
        RecompileCacheKey::Build(withoutReversedBlend, replayKey);
        require(key == replayKey && RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(withoutReversedBlend), "the reversed-order blend code changed the static shader identity");
        auto withoutCoverage = request;
        withoutCoverage.context.pixel->alphaToCoverage = false;
        RecompileCacheKey::Build(withoutCoverage, replayKey);
        require(key != replayKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(withoutCoverage), "alpha-to-coverage did not change the static shader identity");
        auto withoutStencilRef = request;
        withoutStencilRef.context.pixel->stencilRefExportEnable = false;
        RecompileCacheKey::Build(withoutStencilRef, replayKey);
        require(key != replayKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(withoutStencilRef), "the stencil reference export did not change the static shader identity");
    }
    request.context.pixel.reset();
    request.shader.stage = ShaderStage::Vertex;
    const auto withoutPixel = serializer.Deserialize(serializer.Serialize(request));
    require(!withoutPixel.request.context.pixel.has_value() && withoutPixel.request.context.vertex.has_value() && withoutPixel.request.context.vertex->fetchAttribReg == 17u && withoutPixel.request.context.memory.size() == 1u && withoutPixel.request.context.memory[0].guestAddress == 0x60000u && withoutPixel.request.target.nonConstantImageOffsets && withoutPixel.request.layout.firstBinding == 11u && !withoutPixel.request.useCache, "a request without pixel state was misaligned");

    RecompileRequest minimal{};
    minimal.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    minimal.context.waveSize = 64;
    minimal.context.pixel = ShaderPixelStageInfo{};
    minimal.context.pixel->rasterPrimitive = RasterPrimitive::Rectangle;
    minimal.context.pixel->layerExported = true;
    minimal.context.pixel->ditheredAlphaToCoverage = true;
    const auto encoded = serializer.Serialize(minimal);
    require(requestPrefix(encoded, 8u) == "NVNQQSAAAAA=", "new requests did not use serialization version 32");
    const std::array<std::uint32_t, 2> fallbacks{0x40u, 0x1ac4u};
    auto withFallbacks = minimal;
    withFallbacks.context.linkedCallFallbacks = fallbacks;
    const auto fallbackRequest = serializer.Deserialize(serializer.Serialize(withFallbacks));
    require(std::ranges::equal(fallbackRequest.request.context.linkedCallFallbacks, fallbacks), "the linked call fallbacks did not round-trip");
    require(serializer.Deserialize(encoded).request.context.linkedCallFallbacks.empty(), "a request without linked call fallbacks gained some");
    require(serializer.Deserialize(encoded).request.context.pixel->rasterPrimitive == RasterPrimitive::Rectangle && serializer.Deserialize(encoded).request.context.pixel->ditheredAlphaToCoverage, "the pixel primitive class or dither mode did not round-trip");
    require(serializer.Deserialize(encoded).request.context.pixel->layerExported, "the pixel layer export did not round-trip");
    const auto legacy16 = serializer.Deserialize(
        "NVNQQRAAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAsAAAAQAAAAgAAAAAAAAAAAAAAAAQ==");
    require(legacy16.request.context.pixel->alphaToCoverage && !legacy16.request.context.pixel->ditheredAlphaToCoverage, "version16 coverage acquired a dither mode");
    require(legacy16.request.context.pixel->targetOutputMode[0] == 9u && legacy16.request.context.pixel->targetExportMapping[0] == 0xe4u && legacy16.request.target.float64DenormPreserve, "version16 export or FP64 fields were lost");
    require(legacy16.request.layout.firstBinding == 11u && legacy16.request.layout.pushConstantOffsetBytes == 16u && !legacy16.request.useCache, "version16 trailing fields were misaligned");
    const auto upgraded16 = serializer.Deserialize(serializer.Serialize(legacy16.request));
    require(RecompileCacheKey::ContextHash(legacy16.request) == RecompileCacheKey::ContextHash(upgraded16.request), "upgrading version16 changed shader identity");
    const auto legacy17 = serializer.Deserialize(
        "NVNQQREAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAEAAAAIAAAAAAAAAAAAAAAAEBOTAAAA==");
    require(legacy17.request.context.pixel->alphaToCoverage && legacy17.request.context.pixel->stencilRefExportEnable && !legacy17.request.context.pixel->ditheredAlphaToCoverage, "version17 coverage or stencil flags were lost");
    require(legacy17.request.target.float64DenormPreserve && legacy17.request.target.registrationModules && legacy17.request.target.registrationWordLimit == 12345u, "version17 registration or FP64 fields were lost");
    require(legacy17.request.context.pixel->targetOutputMode[0] == 9u && legacy17.request.context.pixel->targetExportMapping[0] == 0xe4u && legacy17.request.layout.firstBinding == 11u && legacy17.request.layout.pushConstantOffsetBytes == 16u && !legacy17.request.useCache, "version17 export or trailing fields were misaligned");
    const auto upgraded17 = serializer.Deserialize(serializer.Serialize(legacy17.request));
    require(RecompileCacheKey::ContextHash(legacy17.request) == RecompileCacheKey::ContextHash(upgraded17.request), "upgrading version17 changed shader identity");
    const std::array<std::pair<bool, std::string_view>, 2> legacy18Requests{{
        {false,
            "NVNQQRIAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAEAAAAAAAAAAAAAAAAAAAAAEBOTAAAAA="},
        {true,
            "NVNQQRIAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAEAAAAAAAAAAAAAAAAAAAAAEBOTAAAAE="}
    }};
    for (const auto& [subgroups, text] : legacy18Requests) {
        const auto legacy18 = serializer.Deserialize(text);
        require(legacy18.request.target.vertexSubgroups == subgroups, "version18 vertex-subgroup flag was lost");
        require(legacy18.request.context.pixel->alphaToCoverage && legacy18.request.context.pixel->stencilRefExportEnable && !legacy18.request.context.pixel->ditheredAlphaToCoverage, "version18 coverage or stencil flags were lost");
        require(legacy18.request.target.float64DenormPreserve && legacy18.request.target.registrationModules && legacy18.request.target.registrationWordLimit == 12345u, "version18 registration or FP64 fields were lost");
        require(legacy18.request.context.pixel->targetOutputMode[0] == 9u && legacy18.request.context.pixel->targetExportMapping[0] == 0xe4u && legacy18.request.layout.firstBinding == 11u && legacy18.request.layout.pushConstantOffsetBytes == 16u && !legacy18.request.useCache, "version18 export or trailing fields were misaligned");
        const auto upgraded18 = serializer.Deserialize(serializer.Serialize(legacy18.request));
        require(RecompileCacheKey::ContextHash(legacy18.request) == RecompileCacheKey::ContextHash(upgraded18.request), "upgrading version18 changed shader identity");
        require(upgraded18.request.target.vertexSubgroups == subgroups, "upgrading version18 changed vertex-subgroup support");
    }
    const std::array<std::pair<std::pair<RasterPrimitive, bool>, std::string_view>, 8> legacy19Requests{{
        {{static_cast<RasterPrimitive>(0u), false},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAA"},
        {{static_cast<RasterPrimitive>(0u), true},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAB"},
        {{static_cast<RasterPrimitive>(1u), false},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAA"},
        {{static_cast<RasterPrimitive>(1u), true},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAB"},
        {{static_cast<RasterPrimitive>(2u), false},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQECAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAA"},
        {{static_cast<RasterPrimitive>(2u), true},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQECAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAB"},
        {{static_cast<RasterPrimitive>(3u), false},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAA"},
        {{static_cast<RasterPrimitive>(3u), true},
            "NVNQQRMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAABAAAAAAAAAAAAAAAAAAAAABATkwAAAB"},
    }};
    for (const auto& [flags, text] : legacy19Requests) {
        const auto legacy19 = serializer.Deserialize(text);
        require(legacy19.request.context.pixel->rasterPrimitive == flags.first && !legacy19.request.context.pixel->ditheredAlphaToCoverage, "version19 primitive or dither mode was misread");
        require(legacy19.request.context.pixel->alphaToCoverage && legacy19.request.context.pixel->stencilRefExportEnable, "version19 coverage or stencil flags were lost");
        require(legacy19.request.target.vertexSubgroups == flags.second && legacy19.request.target.float64DenormPreserve && legacy19.request.target.registrationModules && legacy19.request.target.registrationWordLimit == 12345u, "version19 target fields were misaligned");
        require(legacy19.request.context.pixel->targetOutputMode[0] == 9u && legacy19.request.context.pixel->targetExportMapping[0] == 0xe4u && legacy19.request.layout.firstBinding == 11u && legacy19.request.layout.pushConstantOffsetBytes == 16u && !legacy19.request.useCache, "version19 export or trailing fields were misaligned");
        const auto upgraded19 = serializer.Deserialize(serializer.Serialize(legacy19.request));
        require(RecompileCacheKey::ContextHash(legacy19.request) == RecompileCacheKey::ContextHash(upgraded19.request), "upgrading version19 changed shader identity");
    }
    constexpr std::size_t mappingOffset = 8u + 37u + 18u + 163u;
    for (std::size_t bytes = 0; bytes < 45u; ++bytes) {
        expectFailure([&] { static_cast<void>(serializer.Deserialize(requestPrefix(encoded, mappingOffset + bytes))); }, "truncated data", "a truncated pixel mapping, packing, dual-source, alpha-to-coverage, stencil reference export flag, primitive class, layer export, dither or front-face flag, quad pixel mask or quad sample mask was accepted");
    }
    for (const auto unsupported : {"NVNQQQAAAAA=", "NVNQQSEAAAA="}) {
        expectFailure([&] { static_cast<void>(serializer.Deserialize(unsupported)); }, "serialization version", "an unsupported request version was accepted");
    }
    static constexpr std::string_view version18Request{
        "NVNQQRIAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAB0gQAAAE="};
    const auto version18 = serializer.Deserialize(version18Request);
    require(version18.request.context.pixel.has_value() && version18.request.context.pixel->rasterPrimitive == RasterPrimitive::Triangle, "a version 18 request read a primitive class");
    require(version18.request.context.pixel->reversedBlend == 0u, "a version 18 request read a reversed-order blend code");
    require(version18.request.target.registrationModules && version18.request.target.registrationWordLimit == 1234u && version18.request.target.vertexSubgroups, "a version 18 request trailer was misread");
}

void verifyPositionExportComponents() {
    constexpr std::uint32_t none = 0xffffffffu;
    struct Case {
        std::uint32_t control;
        std::array<std::uint32_t, 4> clip;
        std::array<std::uint32_t, 4> cull;
    };
    const std::array<Case, 6> cases{{
        {0x00400000u, {none, none, none, none}, {none, none, none, none}},
        {0x00400001u, {0u, none, none, none}, {none, none, none, none}},
        {0x00400002u, {none, 0u, none, none}, {none, none, none, none}},
        {0x00400005u, {0u, none, 1u, none}, {none, none, none, none}},
        {0x00400100u, {none, none, none, none}, {0u, none, none, none}},
        {0x00400400u, {none, none, none, none}, {none, none, 0u, none}},
    }};
    for (const auto& test : cases) {
        for (std::uint32_t component = 0; component < 4; ++component) {
            const auto output = ShaderRecompiler::DecodePositionExportComponent(test.control, 1u, component);
            require(output.clipDistance == test.clip[component] && output.cullDistance == test.cull[component], "PA_CL_VS_OUT_CNTL decoded a position component to the wrong clip or cull distance");
        }
    }
}

std::vector<std::uint8_t> decodeBase64(std::string_view text) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::uint8_t> bytes;
    std::uint32_t chunk = 0;
    std::size_t bits = 0;
    for (const char symbol : text) {
        if (symbol == '=') break;
        const auto value = alphabet.find(symbol);
        require(value != std::string_view::npos, "invalid base64 symbol");
        chunk = ((chunk << 6u) | static_cast<std::uint32_t>(value)) & 0xffffffu;
        bits += 6u;
        if (bits >= 8u) {
            bits -= 8u;
            bytes.push_back(static_cast<std::uint8_t>((chunk >> bits) & 0xffu));
        }
    }
    return bytes;
}

std::string encodeBase64(const std::vector<std::uint8_t>& bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    for (std::size_t index = 0; index < bytes.size(); index += 3u) {
        const std::size_t count = std::min<std::size_t>(3u, bytes.size() - index);
        std::uint32_t chunk = static_cast<std::uint32_t>(bytes[index]) << 16u;
        if (count > 1u) chunk |= static_cast<std::uint32_t>(bytes[index + 1u]) << 8u;
        if (count > 2u) chunk |= bytes[index + 2u];
        for (std::size_t symbol = 0; symbol < 4u; ++symbol) {
            text.push_back(symbol <= count ? alphabet[(chunk >> (18u - 6u * symbol)) & 0x3fu] : '=');
        }
    }
    return text;
}

void verifyVertexInfoVersion19() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    const RequestSerializer serializer;
    RecompileRequest vertex{};
    vertex.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
    vertex.context.waveSize = 64;
    vertex.context.vertex = ShaderVertexStageInfo{};
    const auto unmarked = serializer.Serialize(vertex);
    vertex.context.vertex->paClVsOutCntl = 0x89abcdefu;
    const auto marked = serializer.Serialize(vertex);
    const auto plain = decodeBase64(unmarked);
    const auto flagged = decodeBase64(marked);
    require(plain.size() == flagged.size(), "PA_CL_VS_OUT_CNTL changed the request size");
    std::size_t offset = 0;
    while (offset < plain.size() && plain[offset] == flagged[offset]) ++offset;
    require(offset + 4u <= plain.size() && flagged[offset] == 0xefu && flagged[offset + 1u] == 0xcdu && flagged[offset + 2u] == 0xabu && flagged[offset + 3u] == 0x89u, "PA_CL_VS_OUT_CNTL is not the four bytes the version 20 vertex info adds");
    require(std::equal(plain.begin() + offset + 4u, plain.end(), flagged.begin() + offset + 4u), "PA_CL_VS_OUT_CNTL moved the fields after it");
    auto version20Vertex = flagged;
    version20Vertex.pop_back();
    version20Vertex[4] = 20u;
    const auto replay20Vertex = serializer.Deserialize(encodeBase64(version20Vertex));
    require(replay20Vertex.request.context.vertex.has_value() && replay20Vertex.request.context.vertex->paClVsOutCntl == 0x89abcdefu, "a main version20 request lost PA_CL_VS_OUT_CNTL");
    require(serializer.Serialize(replay20Vertex.request) == marked, "a main version20 vertex request misread trailing fields");
    vertex.context.vertex->paClVsOutCntl = (1u << 18u) | (1u << 21u) | 0xfu;
    const auto layered = serializer.Deserialize(serializer.Serialize(vertex));
    require(layered.request.context.vertex.has_value() && layered.request.context.vertex->paClVsOutCntl == vertex.context.vertex->paClVsOutCntl, "a layered vertex info lost its clip distances or render target index");
    auto legacy = plain;
    legacy.pop_back();
    legacy.erase(legacy.begin() + offset, legacy.begin() + offset + 4u);
    for (const auto version : {16u, 17u, 18u, 19u}) {
        legacy[4] = static_cast<std::uint8_t>(version);
        const auto replay = serializer.Deserialize(encodeBase64(legacy));
        require(replay.request.context.vertex.has_value() && replay.request.context.vertex->paClVsOutCntl == 0u, "a legacy vertex info read a PA_CL_VS_OUT_CNTL word");
        require(serializer.Serialize(replay.request) == unmarked, "a legacy vertex info misread the fields after it");
    }
    RecompileRequest pixel{};
    pixel.shader = {ShaderStage::Fragment, 0x20000u, code, 0, {}};
    pixel.context.waveSize = 64;
    pixel.context.pixel = ShaderPixelStageInfo{};
    pixel.context.pixel->rasterPrimitive = RasterPrimitive::Rectangle;
    const auto encodedPixel = serializer.Serialize(pixel);
    auto exportedPixel = pixel;
    exportedPixel.context.pixel->layerExported = true;
    const auto exportedBytes = decodeBase64(serializer.Serialize(exportedPixel));
    auto blended = pixel;
    blended.context.pixel->reversedBlend = 0x89abcdefu;
    const auto plainPixel = decodeBase64(encodedPixel);
    const auto blendedPixel = decodeBase64(serializer.Serialize(blended));
    require(plainPixel.size() == blendedPixel.size() && plainPixel.size() == exportedBytes.size(), "the layer export flag or the reversed-order blend code changed the request size");
    std::size_t flagOffset = 0;
    while (flagOffset < plainPixel.size() && plainPixel[flagOffset] == exportedBytes[flagOffset]) ++flagOffset;
    require(flagOffset < plainPixel.size(), "the pixel layer export flag is not serialized");
    std::size_t blendOffset = 0;
    while (blendOffset < plainPixel.size() && plainPixel[blendOffset] == blendedPixel[blendOffset]) ++blendOffset;
    require(blendOffset + 4u <= plainPixel.size() && blendedPixel[blendOffset] == 0xefu && blendedPixel[blendOffset + 1u] == 0xcdu && blendedPixel[blendOffset + 2u] == 0xabu && blendedPixel[blendOffset + 3u] == 0x89u, "the reversed-order blend code is not the four bytes the version 30 pixel info adds");
    require(std::equal(plainPixel.begin() + static_cast<std::ptrdiff_t>(blendOffset + 4u), plainPixel.end(), blendedPixel.begin() + static_cast<std::ptrdiff_t>(blendOffset + 4u)), "the reversed-order blend code moved the fields after it");
    auto allBitsPixel = pixel;
    allBitsPixel.context.pixel->frontFaceAllBits = true;
    const auto allBitsBytes = decodeBase64(serializer.Serialize(allBitsPixel));
    std::size_t allBitsOffset = 0;
    while (allBitsOffset < plainPixel.size() && plainPixel[allBitsOffset] == allBitsBytes[allBitsOffset]) ++allBitsOffset;
    require(allBitsOffset == flagOffset + 2u, "the front-face all-bits flag does not follow the dithered coverage flag");
    require(blendOffset == allBitsOffset + 18u, "the reversed-order blend code is not serialized after the front-face all-bits flag, quad pixel mask and quad sample masks");
    constexpr std::size_t sampleMaskBytes = 16u;
    auto maskedPixel = pixel;
    maskedPixel.context.pixel->quadPixelMask = 0x2u;
    const auto maskedBytes = decodeBase64(serializer.Serialize(maskedPixel));
    std::size_t maskOffset = 0;
    while (maskOffset < plainPixel.size() && plainPixel[maskOffset] == maskedBytes[maskOffset]) ++maskOffset;
    require(maskOffset == allBitsOffset + 1u && maskOffset < plainPixel.size(), "the quad pixel mask is not the byte after the front-face all-bits flag");
    auto sampledPixel = maskedPixel;
    sampledPixel.context.pixel->quadSampleMask = {0x1u, 0x2u, 0x4u, 0x8u};
    const auto sampledBytes = decodeBase64(serializer.Serialize(sampledPixel));
    require(std::equal(maskedBytes.begin(), maskedBytes.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u), sampledBytes.begin()) && sampledBytes[maskOffset + 1u] == 0x1u && sampledBytes[maskOffset + 5u] == 0x2u && sampledBytes[maskOffset + 9u] == 0x4u && sampledBytes[maskOffset + 13u] == 0x8u, "the quad sample masks are not the four words after the quad pixel mask");
    auto version29 = sampledBytes;
    version29.pop_back();
    version29.erase(version29.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes), version29.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u));
    version29[4] = 29u;
    const auto replay29 = serializer.Deserialize(encodeBase64(version29));
    require(replay29.request.context.pixel.has_value() && replay29.request.context.pixel->quadSampleMask == std::array<std::uint16_t, 4>{0x1u, 0x2u, 0x4u, 0x8u} && replay29.request.context.pixel->reversedBlend == 0u && decodeBase64(serializer.Serialize(replay29.request)) == sampledBytes, "a version 29 request read a reversed-order blend code or misread the fields after the quad sample masks");
    auto version28 = sampledBytes;
    version28.pop_back();
    version28.erase(version28.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u), version28.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u));
    version28[4] = 28u;
    const auto replay28 = serializer.Deserialize(encodeBase64(version28));
    require(replay28.request.context.pixel.has_value() && replay28.request.context.pixel->quadPixelMask == 0x2u && replay28.request.context.pixel->quadSampleMask == std::array<std::uint16_t, 4>{0xffffu, 0xffffu, 0xffffu, 0xffffu} && replay28.request.context.pixel->reversedBlend == 0u && decodeBase64(serializer.Serialize(replay28.request)) == maskedBytes, "a version 28 request read quad sample masks or a reversed-order blend code, or misread the fields after them");
    auto version27 = maskedBytes;
    version27.pop_back();
    version27.erase(version27.begin() + static_cast<std::ptrdiff_t>(maskOffset), version27.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u));
    version27[4] = 27u;
    const auto replay27 = serializer.Deserialize(encodeBase64(version27));
    require(replay27.request.context.pixel.has_value() && replay27.request.context.pixel->quadPixelMask == 0xfu && replay27.request.context.pixel->reversedBlend == 0u && serializer.Serialize(replay27.request) == encodedPixel, "a version 27 request read a quad pixel mask or reversed-order blend code, or misread the fields after it");
    auto legacyPixel = plainPixel;
    legacyPixel.pop_back();
    legacyPixel.erase(legacyPixel.begin() + static_cast<std::ptrdiff_t>(allBitsOffset), legacyPixel.begin() + static_cast<std::ptrdiff_t>(allBitsOffset + 2u + sampleMaskBytes + 4u));
    legacyPixel[4] = 26u;
    const auto replay26 = serializer.Deserialize(encodeBase64(legacyPixel));
    require(replay26.request.context.pixel.has_value() && replay26.request.context.pixel->rasterPrimitive == RasterPrimitive::Rectangle && !replay26.request.context.pixel->frontFaceAllBits && replay26.request.context.pixel->reversedBlend == 0u, "a version 26 request lost its pixel primitive class or read a front-face all-bits flag");
    require(serializer.Serialize(replay26.request) == encodedPixel, "a version 26 request misread the fields after its dithered coverage flag");
    legacyPixel.resize(legacyPixel.size() - sizeof(std::uint64_t));
    legacyPixel[4] = 25u;
    const auto replay25 = serializer.Deserialize(encodeBase64(legacyPixel));
    require(replay25.request.context.pixel.has_value() && !replay25.request.context.pixel->layerExported && !replay25.request.context.pixel->ditheredAlphaToCoverage && replay25.request.context.pixel->reversedBlend == 0u, "a main version 25 request read a reversed-order blend code");
    require(serializer.Serialize(replay25.request) == encodedPixel, "a main version 25 request misread the fields after its dither flag");
    legacyPixel.erase(legacyPixel.begin() + static_cast<std::ptrdiff_t>(flagOffset + 1u));
    for (const auto version : {23u, 24u}) {
        legacyPixel[4] = static_cast<std::uint8_t>(version);
        const auto replayPixel = serializer.Deserialize(encodeBase64(legacyPixel));
        const auto what = "a main version " + std::to_string(version) + " request";
        require(replayPixel.request.context.pixel.has_value() && !replayPixel.request.context.pixel->layerExported && !replayPixel.request.context.pixel->ditheredAlphaToCoverage && replayPixel.request.context.pixel->reversedBlend == 0u, (what + " read a dither flag or reversed-order blend code").c_str());
        require(serializer.Serialize(replayPixel.request) == encodedPixel, (what + " misread the fields after its layer export flag").c_str());
    }
    legacyPixel.erase(legacyPixel.begin() + static_cast<std::ptrdiff_t>(flagOffset));
    for (const auto version : {19u, 20u, 21u, 22u}) {
        auto versionPixel = legacyPixel;
        versionPixel[4] = static_cast<std::uint8_t>(version);
        if (version < 22u) versionPixel.pop_back();
        const auto replayPixel = serializer.Deserialize(encodeBase64(versionPixel));
        const auto what = "a main version " + std::to_string(version) + " request";
        require(replayPixel.request.context.pixel.has_value() && replayPixel.request.context.pixel->rasterPrimitive == RasterPrimitive::Rectangle && !replayPixel.request.context.pixel->layerExported && !replayPixel.request.context.pixel->ditheredAlphaToCoverage && replayPixel.request.context.pixel->reversedBlend == 0u, (what + " lost its pixel primitive class or read a layer export, dither flag or reversed-order blend code").c_str());
        require(serializer.Serialize(replayPixel.request) == encodedPixel, (what + " misread the fields after its pixel primitive class").c_str());
    }
}

void verifyLegacyPixelRequests() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::string_view, 14> legacyPixelRequests{
        "NVNQQQEAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAA",
        "NVNQQQIAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAAAA==",
        "NVNQQQMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAAAA==",
        "NVNQQQQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAAAA==",
        "NVNQQQUAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAACQAAAAAAAAAA"
        "AAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAsAAAAAAAAA"
        "gAAAAAAA",
        "NVNQQQYAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAACQAAAAAAAAAA"
        "AAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAsAAAAAAAAA"
        "gAAAAAAAAQ==",
        "NVNQQQcAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAA"
        "AIAAAAAAAAE=",
        "NVNQQQgAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAsAAAAAAAAAgAAAAAAAAQ==",
        "NVNQQQkAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAsAAAAAAAAAgAAAAAAAAQAAAAA=",
        "NVNQQQoAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAsAAAAAAAAAgAAAAAAAAQAAAAA=",
        "NVNQQQsAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgAJAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAALAAAAAAAAAIAAAAAAAAEAAAAA",
        "NVNQQQwAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgAJAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAALAAAAAAAAAIAAAAAAAAEAAAAAAA==",
        "NVNQQQ0AAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgAJAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAQQAAAAwEAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAACwAAAAAAAACAAAAAAAABAAAAAAA=",
        "NVNQQQ4AAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgAJAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAALAAAAAAAAAIAAAAAAAAEAAAAAAA==",
    };
    const RequestSerializer serializer;
    for (std::size_t index = 0; index < legacyPixelRequests.size(); ++index) {
        const auto version = index + 1u;
        const auto replay = serializer.Deserialize(legacyPixelRequests[index]);
        const auto& request = replay.request;
        require(request.context.pixel.has_value(), "legacy pixel state was lost");
        const auto& pixel = *request.context.pixel;
        require(pixel.targetExportMapping == std::array<std::uint8_t, 8>{}, "legacy pixel mapping no longer defaults to zero");
        require(pixel.targetExportPacking == std::array<ColorExportPacking, 8>{}, "a legacy request packed a color export");
        require(!pixel.dualSourceBlend, "a legacy request enabled dual-source blending");
        require(!pixel.alphaToCoverage && !pixel.ditheredAlphaToCoverage, "a legacy request enabled alpha-to-coverage");
        require(!pixel.stencilRefExportEnable, "a legacy request exported a stencil reference");
        require(pixel.rasterPrimitive == RasterPrimitive::Triangle, "a legacy request drew something other than triangles");
        require(pixel.reversedBlend == 0u, "a legacy request blended into a reversed component order");
        require(pixel.inputAddr == 2u && pixel.hasPerspectiveCenterVgpr && pixel.targetOutputMode[0] == 9u, "legacy pixel layout was misread");
        require(pixel.conservativeZExport == (version >= 7u ? ConservativeZExport::GreaterThanZ : ConservativeZExport::AnyZ), "legacy conservative Z layout was misread");
        require(!pixel.orderedPixelShader, "a legacy request became a primitive-ordered pixel shader");
        require(request.shader.stage == ShaderStage::Fragment && request.shader.code.size() == 1u && request.shader.code[0] == 0xbf810000u && !request.context.vertex.has_value() && request.context.memory.empty(), "legacy guest context was misaligned");
        require(request.target.vulkanVersion == 0x00401000u && request.target.spirvVersion == 0x00010300u && request.target.subgroupSize == 64u && request.layout.firstBinding == 11u && request.layout.pushConstantSizeBytes == 128u, "legacy target or binding layout was misaligned");
        require(request.useCache == (version == 1u) && request.target.nonConstantImageOffsets == (version >= 6u) && request.target.srgbDecodeFormats == 0u && !request.target.narrowSubgroupClock && !request.target.float64DenormPreserve && !request.target.registrationModules && !request.target.float16DenormPreserve, "legacy request trailer was misread");
        const auto upgraded = serializer.Deserialize(serializer.Serialize(request));
        require(upgraded.request.context.pixel->targetExportMapping == pixel.targetExportMapping && RecompileCacheKey::ContextHash(upgraded.request) == RecompileCacheKey::ContextHash(request), "upgrading a legacy capture changed its pixel mapping");
    }
}

void verifyPixelExportReplay() {
    using namespace ShaderRecompiler;
    std::array<std::uint32_t, 11> code{
        0x7e0002ffu, 0x3e800000u, 0x7e0202ffu, 0x3f000000u,
        0x7e0402ffu, 0x3f400000u, 0x7e0602ffu, 0x3f800000u,
        0xf800180fu, 0x03020100u, 0xbf810000u
    };
    const RequestSerializer serializer;
    for (const auto target : {0u, 7u}) {
        code[8] = 0xf800180fu | (target << 4u);
        ShaderPixelStageInfo pixel{};
        pixel.targetOutputMode[target] = 9u;
        pixel.targetExportMapping.fill(0xe4u);
        pixel.targetExportMapping[target] = 0xc6u;
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64u;
        request.context.pixel = pixel;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64u;
        request.layout.pushConstantSizeBytes = 128u;
        request.useCache = false;
        const auto replay = serializer.Deserialize(serializer.Serialize(request));
        const auto original = Recompile(request);
        auto identity = request;
        identity.context.pixel->targetExportMapping[target] = 0xe4u;
        require(original.spirv != Recompile(identity).spirv, "the non-identity pixel export mapping did not affect the compiled shader");
        require(replay.request.context.pixel.has_value() && replay.request.context.pixel->targetExportMapping == pixel.targetExportMapping, "replay lost the non-identity pixel export mapping");
        const auto replayed = Recompile(replay.request);
        verifyResult(original, replayed);
        request.useCache = true;
        const auto cached = Recompile(request);
        const auto cachedReplay = serializer.Deserialize(serializer.Serialize(request));
        const auto hit = Recompile(cachedReplay.request);
        require(hit.cacheHit && hit.variantId == cached.variantId, "pixel replay did not reuse the original shader variant");
        verifyResult(cached, hit);
    }
}

std::vector<std::uint32_t> noPerspectiveLocations(std::span<const std::uint32_t> code) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = twoParameterPixel();
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = Recompile(request);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> locations;
    std::vector<std::uint32_t> decorated;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpDecorate) continue;
        if (words[at + 2] == spv::DecorationLocation) locations[words[at + 1]] = words[at + 3];
        if (words[at + 2] == spv::DecorationNoPerspective) decorated.push_back(words[at + 1]);
    }
    std::vector<std::uint32_t> result2;
    for (const auto id : decorated) result2.push_back(locations.count(id) != 0 ? locations.at(id) : 0xffffffffu);
    return result2;
}

void verifyPixelInputs() {
    using namespace ShaderRecompiler;
    require(PixelInputVgpr(0x326u, PixelInput::PerspectiveCentroid) == 2u && PixelInputVgpr(0x326u, PixelInput::LinearCenter) == 4u && PixelInputVgpr(0x326u, PixelInput::PositionX) == 6u, "the SPI_PS_INPUT_ADDR layout moved the inputs");
    require(PixelInputVgpr(0x7afu, PixelInput::PerspectiveCentroid) == 4u && PixelInputVgpr(0x7afu, PixelInput::PositionX) == 12u && PixelInputVgpr(0x7afu, PixelInput::PositionZ) == 14u, "ADDR-only inputs did not reserve their VGPRs");

    static constexpr std::array<std::uint32_t, 7> byPair{0xc8100000u, 0xc8110001u, 0xc8140402u, 0xc8150403u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    const auto linear = noPerspectiveLocations(byPair);
    require(linear.size() == 1u && linear[0] == 1u, "only the parameter interpolated through the linear pair must be NoPerspective");
    static constexpr std::array<std::uint32_t, 7> bothPairs{0xc8100000u, 0xc8110001u, 0xc8140002u, 0xc8150003u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    expectFailure([&] { static_cast<void>(noPerspectiveLocations(bothPairs)); }, "interpolated through both a perspective and a linear I/J pair", "a parameter read through both pairs was given one interpolation");

    static constexpr std::array<std::uint32_t, 1> code{0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    auto pixel = twoParameterPixel();
    pixel.inputAddr |= PixelInputBit(PixelInput::PerspectiveCentroid) | PixelInputBit(PixelInput::LinearCentroid);
    pixel.perspectiveCentroid = true;
    pixel.linearCentroid = true;
    request.context.pixel = pixel;
    const auto replay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(request));
    const auto& back = *replay.request.context.pixel;
    require(back.inputAddr == pixel.inputAddr && back.perspectiveCentroid && back.linearCentroid && back.noPerspective, "the pixel input layout did not survive serialization");
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    const auto first = key;
    for (const auto change : {0, 1, 2}) {
        auto other = request;
        auto changed = pixel;
        if (change == 0) changed.inputAddr |= PixelInputBit(PixelInput::PerspectiveSample);
        if (change == 1) changed.perspectiveCentroid = false;
        if (change == 2) changed.linearCentroid = false;
        other.context.pixel = changed;
        RecompileCacheKey::Build(other, key);
        require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "the cache keys ignore the pixel input layout");
    }
}

ShaderRecompiler::RecompileResult recompileSlots(std::initializer_list<std::uint32_t> controls, std::span<const std::uint32_t> code) {
    using namespace ShaderRecompiler;
    ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = static_cast<std::uint32_t>(controls.size());
    std::uint32_t index = 0;
    for (const auto control : controls) pixel.interpolatorSettings[index++] = control;
    pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping[0] = 0xe4u;
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = true;
    static constexpr std::array<std::uint32_t, 1> capabilities{spv::CapabilityFloat64};
    request.target.supportedCapabilities = capabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return Recompile(request);
}

std::vector<std::pair<std::uint32_t, bool>> slotInputs(std::initializer_list<std::uint32_t> controls, std::span<const std::uint32_t> code) {
    const auto result = recompileSlots(controls, code);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> locations;
    std::map<std::uint32_t, bool> perVertex;
    std::vector<std::uint32_t> inputs;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpVariable && words[at + 3] == spv::StorageClassInput) inputs.push_back(words[at + 2]);
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationLocation) locations[words[at + 1]] = words[at + 3];
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationPerVertexKHR) perVertex[words[at + 1]] = true;
    }
    std::vector<std::pair<std::uint32_t, bool>> located;
    for (const auto id : inputs) {
        if (locations.contains(id)) located.emplace_back(locations.at(id), perVertex.contains(id));
    }
    std::sort(located.begin(), located.end());
    return located;
}

void verifyPixelParameterSlots() {
    static constexpr std::array<std::uint32_t, 7> shared{0xc8100000u, 0xc8110001u, 0xc8140500u, 0xc8150501u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    auto inputs = slotInputs({0x3u, 0x3u}, shared);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "inputs reading one slot were not declared once at the slot");
    inputs = slotInputs({0x404u, 0x0u}, shared);
    require(inputs.size() == 2u && inputs[0].first == 0u && inputs[1].first == 4u, "inputs of different slots moved");
    require(slotInputs({0x20u, 0x2320u}, shared).empty(), "a defaulted input was declared as a parameter");
    static constexpr std::array<std::uint32_t, 8> mixed{0xc8100000u, 0xc8110001u, 0xc8160402u, 0xc81a0802u, 0xc81e0f02u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    inputs = slotInputs({0x0u, 0x400u, 0x22u, 0x320u}, mixed);
    require(inputs.size() == 1u && inputs[0].first == 0u && inputs[0].second, "a slot read flat and interpolated did not become one per-vertex input");
    static constexpr std::array<std::uint32_t, 7> vertices{0xc8120002u, 0xc8160000u, 0xc81a0001u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto subtracts = [](std::initializer_list<std::uint32_t> controls) {
        const auto result = recompileSlots(controls, vertices);
        const auto& words = result.spirv.Words();
        std::size_t count = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) count += (words[at] & 0xffffu) == spv::OpFSub;
        return count;
    };
    inputs = slotInputs({0x423u}, vertices);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "a pass-through input (OFFSET bit 5 with FLAT_SHADE) was not read per vertex at its slot");
    require(subtracts({0x423u}) == 0u, "v_interp_mov p10/p20 of a pass-through input subtracted vertex 0");
    inputs = slotInputs({0x403u}, vertices);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second && subtracts({0x403u}) == 2u, "v_interp_mov p10/p20 of a flat input did not read differences to vertex 0");
    require(slotInputs({0x23u}, vertices).empty(), "a defaulted input (OFFSET bit 5 without FLAT_SHADE) was declared as a parameter");
    expectFailure([] { static_cast<void>(recompileSlots({0x423u, 0x3u}, shared)); }, "passes its vertices through unchanged", "an interpolated pass-through input was accepted");
}

void verifyF16PixelParameterSlots() {
    static constexpr std::array<std::uint32_t, 7> high{0xd7420002u, 0x00020100u, 0xd75a0003u, 0x040a0300u, 0xf800180fu, 0x03030303u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 7> low{0xd7420002u, 0x00020000u, 0xd75a0003u, 0x040a0200u, 0xf800180fu, 0x03030303u, 0xbf810000u};
    auto inputs = slotInputs({0x03080003u}, high);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "a 16-bit interpolated input was not read per vertex at its slot");
    inputs = slotInputs({0x03080023u}, high);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "an input with a defaulted low half was not read per vertex for its high half");
    require(slotInputs({0x03180023u}, high).empty(), "an input whose high half is defaulted was declared for a high-half read");
    require(slotInputs({0x03080023u}, low).empty(), "an input whose low half is defaulted was declared for a low-half read");
    inputs = slotInputs({0x03180003u}, low);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "an input with a defaulted high half was not read per vertex for its low half");
}

}

void verifyComputedTexelOffsets() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    static std::array<std::uint32_t, 64> output{};
    const auto textureBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 16> userData{
        static_cast<std::uint32_t>(textureBase >> 8u), static_cast<std::uint32_t>((textureBase >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    const auto program = [](std::uint32_t offsetSource, std::uint32_t literal) {
        std::vector<std::uint32_t> code{0x7e020200u | offsetSource};
        if (offsetSource == 0xffu) code.push_back(literal);
        code.insert(code.end(), {0x7e040280u, 0x7e060280u, 0xf0dc0f08u, 0x00400401u, 0xe0700000u, 0x80030400u, 0xbf810000u});
        return code;
    };
    const auto computed = program(0x100u, 0u);
    const auto constant = program(0xffu, 0x3fu | (1u << 8u));
    const auto recompile = [&](std::span<const std::uint32_t> code, bool offsets) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x30000u, code, 0, {}};
        request.target = BufferTarget();
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.target.nonConstantImageOffsets = offsets;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        request.context.memory = memory.Regions();
        return Recompile(request).spirv;
    };
    const auto sampleOperands = [](std::span<const std::uint32_t> words) {
        std::uint32_t mask = 0;
        bool gatherExtended = false;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "texel offsets: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilityImageGatherExtended) gatherExtended = true;
            if (op == spv::OpImageSampleExplicitLod && count > 5u) mask |= words[cursor + 5];
            cursor += count;
        }
        return std::pair{mask, gatherExtended};
    };
    expectFailure([&] { static_cast<void>(recompile(computed, false)); }, "texel offset that is not a constant", "texel offsets: a computed offset compiled without maintenance8");
    const auto [computedMask, computedGather] = sampleOperands(recompile(computed, true));
    require((computedMask & spv::ImageOperandsOffsetMask) != 0u && (computedMask & spv::ImageOperandsConstOffsetMask) == 0u && computedGather, "texel offsets: a computed offset is not an Offset operand");
    for (const bool offsets : {false, true}) {
        const auto [constantMask, constantGather] = sampleOperands(recompile(constant, offsets));
        require((constantMask & spv::ImageOperandsConstOffsetMask) != 0u && (constantMask & spv::ImageOperandsOffsetMask) == 0u && !constantGather, "texel offsets: a constant offset is not a ConstOffset operand");
    }
}

void verifyShaderClockScopes() {
    using namespace ShaderRecompiler;
    static std::array<std::uint32_t, 64> output{};
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    const std::array<std::uint32_t, 2> capabilities{1u, static_cast<std::uint32_t>(spv::CapabilityShaderClockKHR)};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_shader_clock"};
    const auto scope = [&](std::uint32_t clockOpcode, bool narrow) {
        const std::vector<std::uint32_t> code{clockOpcode, 0x00000000u, 0xbf8cc07fu, 0x7e020204u, 0xe0700000u, 0x80000100u, 0xbf810000u};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x30000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.narrowSubgroupClock = narrow;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        request.context.memory = memory.Regions();
        const auto words = Recompile(request).spirv;
        std::map<std::uint32_t, std::uint32_t> constants;
        std::vector<std::uint32_t> scopeIds;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "shader clock: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            if (op == spv::OpConstant && count == 4u) constants[words[cursor + 2]] = words[cursor + 3];
            if (op == spv::OpReadClockKHR) scopeIds.push_back(words[cursor + 3]);
            cursor += count;
        }
        require(scopeIds.size() == 1u && constants.contains(scopeIds[0]), "shader clock: expected one OpReadClockKHR with a constant scope");
        return constants[scopeIds[0]];
    };
    constexpr std::uint32_t Memtime = 0xf4900100u;
    constexpr std::uint32_t Memrealtime = 0xf4940100u;
    require(scope(Memtime, false) == spv::ScopeSubgroup, "shader clock: s_memtime does not read the subgroup clock");
    require(scope(Memtime, true) == spv::ScopeDevice, "shader clock: s_memtime reads the narrow subgroup clock");
    require(scope(Memrealtime, false) == spv::ScopeDevice && scope(Memrealtime, true) == spv::ScopeDevice, "shader clock: s_memrealtime does not read the device clock");
}

void verifyInt64AtomicCapabilities() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format32_32UInt = 62;
    constexpr std::uint32_t Type2D = 9;
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    alignas(256) static std::array<std::uint32_t, 64> buffer{};
    alignas(4096) static std::array<std::uint32_t, 512> texels{};
    const auto bufferAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(buffer.data()));
    const auto texelAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels.data()));
    const std::array<std::uint32_t, 16> userData{
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(bufferAddress), static_cast<std::uint32_t>((bufferAddress >> 32u) & 0xffffu), 256u, 0x01016facu,
        static_cast<std::uint32_t>(texelAddress >> 8u), static_cast<std::uint32_t>((texelAddress >> 40u) & 0xffu) | (Format32_32UInt << 20u) | (3u << 30u), 7u | (7u << 14u), 0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u};
    const std::vector<std::uint32_t> bufferAtomic{0xe1705000u, 0x80012803u, 0xbf810000u};
    const std::vector<std::uint32_t> imageAtomic{0xf03c0308u, 0x00020a08u, 0xbf810000u};
    const auto compile = [&](std::span<const std::uint32_t> code, spv::Capability added) {
        const std::array<std::uint32_t, 4> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, static_cast<std::uint32_t>(added)};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x58000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    const auto declares = [](std::span<const std::uint32_t> words, spv::Capability capability) {
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "64-bit atomics: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == spv::OpCapability && words[cursor + 1] == static_cast<std::uint32_t>(capability)) return true;
            cursor += count;
        }
        return false;
    };
    expectFailure([&] { static_cast<void>(compile(bufferAtomic, spv::CapabilityShader)); }, "64-bit buffer atomics need shaderBufferInt64Atomics", "64-bit atomics: a buffer_atomic_inc_x2 compiled without shaderBufferInt64Atomics");
    require(declares(compile(bufferAtomic, spv::CapabilityInt64Atomics), spv::CapabilityInt64Atomics), "64-bit atomics: a buffer_atomic_inc_x2 does not declare Int64Atomics");
    expectFailure([&] { static_cast<void>(compile(imageAtomic, spv::CapabilityShader)); }, "64-bit image atomics need VK_EXT_shader_image_atomic_int64", "64-bit atomics: an image_atomic_swap on a 32_32 image compiled without shaderImageInt64Atomics");
    require(declares(compile(imageAtomic, spv::CapabilityInt64ImageEXT), spv::CapabilityInt64ImageEXT), "64-bit atomics: an image_atomic_swap on a 32_32 image does not declare Int64ImageEXT");
}

void verifySpecializedBufferAtomicBindings() {
    using namespace ShaderRecompiler;
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    alignas(256) static std::array<std::uint32_t, 64> buffer{};
    const auto bufferAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(buffer.data()));
    const std::array<std::uint32_t, 16> userData{
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(bufferAddress), static_cast<std::uint32_t>((bufferAddress >> 32u) & 0xffffu), 256u, 0x31016facu,
        0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u};
    const std::vector<std::uint32_t> code{0xe0f05000u, 0x80012803u, 0xbf810000u};
    const auto compile = [&](std::span<const std::uint32_t> supported) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x58800u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = supported;
        request.target.supportedExtensions = extensions;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request);
    };
    const auto result = compile(capabilities);
    std::size_t tables = 0;
    std::size_t faults = 0;
    std::size_t atomicElements = 0;
    for (const auto& binding : result.bindings) {
        if (binding.role == DescriptorRole::BdaPagetable) ++tables;
        if (binding.role == DescriptorRole::FaultBuffer) ++faults;
        atomicElements += static_cast<std::size_t>(std::count(binding.bufferAtomic.begin(), binding.bufferAtomic.end(), true));
    }
    require(tables == 0, "specialized buffer atomic: a buffer_atomic_inc through a V# the program specializes binds the address tables");
    require(faults == 1 && result.bdaAbiVersion == BdaAbi::Version, "specialized buffer atomic: the program has no fault buffer for its unaligned atomic");
    require(atomicElements == 1, "specialized buffer atomic: the V# element is not reported as atomic");
    const auto words = result.spirv.Words();
    std::uint32_t faultFunction = 0;
    std::size_t faultCalls = 0;
    bool physical = false;
    bool int64 = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto opcode = words[cursor] & 0xffffu;
        require(count != 0 && count <= words.size() - cursor, "specialized buffer atomic: truncated SPIR-V instruction");
        if (opcode == spv::OpCapability && words[cursor + 1] == spv::CapabilityPhysicalStorageBufferAddresses) physical = true;
        if (opcode == spv::OpCapability && words[cursor + 1] == spv::CapabilityInt64) int64 = true;
        if (opcode == spv::OpMemoryModel && words[cursor + 1] != spv::AddressingModelLogical) physical = true;
        if (opcode == spv::OpName && std::string_view(reinterpret_cast<const char*>(&words[cursor + 2])) == "record_bda_fault") faultFunction = words[cursor + 1];
        if (opcode == spv::OpFunctionCall && faultFunction != 0 && words[cursor + 3] == faultFunction) ++faultCalls;
        cursor += count;
    }
    require(!physical, "specialized buffer atomic: the module addresses physical storage without an address access");
    require(int64, "specialized buffer atomic: the fault address is 64-bit and the module does not declare Int64");
    require(faultFunction != 0 && faultCalls == 1, "specialized buffer atomic: an unaligned atomic does not record exactly one fault");
    const std::array<std::uint32_t, 1> shaderOnly{spv::CapabilityShader};
    expectFailure([&] { static_cast<void>(compile(shaderOnly)); }, "buffer atomic fault addresses require shaderInt64", "specialized buffer atomic: the program compiled without shaderInt64");
}

void verifyCoherentGds() {
    using namespace ShaderRecompiler;
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::vector<std::uint32_t> code{0x34000082u, 0xbefc03ffu, 0x0000ffffu, 0xd8da0000u, 0x01000000u, 0xbf8cc07fu, 0xd8360080u, 0x00000100u, 0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x5c000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto words = Recompile(request).spirv;
    std::uint32_t gds = 0u;
    std::map<std::uint32_t, std::vector<std::uint32_t>> decorations;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto opcode = words[cursor] & 0xffffu;
        require(count != 0 && count <= words.size() - cursor, "coherent GDS: truncated SPIR-V instruction");
        if (opcode == spv::OpName && std::string_view(reinterpret_cast<const char*>(&words[cursor + 2])) == "gds") gds = words[cursor + 1];
        if (opcode == spv::OpDecorate) decorations[words[cursor + 1]].push_back(words[cursor + 2]);
        cursor += count;
    }
    require(gds != 0u, "coherent GDS: a ds_read_b32 and ds_write_b32 on GDS declare no gds buffer");
    require(std::ranges::find(decorations[gds], static_cast<std::uint32_t>(spv::DecorationCoherent)) != decorations[gds].end(), "coherent GDS: the gds buffer is not Coherent");
}

void verifyCoherentBufferAccesses() {
    using namespace ShaderRecompiler;
    alignas(256) static std::array<std::uint32_t, 64> input{};
    alignas(256) static std::array<std::uint32_t, 64> output{};
    const auto inputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(input.data()));
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 8> userData{
        static_cast<std::uint32_t>(inputAddress), static_cast<std::uint32_t>((inputAddress >> 32u) & 0xffffu), 256u, 0x31016facu,
        static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 256u, 0x31016facu};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    struct Module {
        std::map<std::uint32_t, std::string> names;
        std::map<std::uint32_t, std::vector<std::uint32_t>> decorations;
        std::vector<std::string> loads;
        std::vector<std::string> stores;
    };
    const auto compile = [&](bool invalidate, bool glcLoad, bool glcStore) {
        std::vector<std::uint32_t> code{0x34020082u};
        if (invalidate) code.insert(code.end(), {0xe1c40000u, 0u});
        code.insert(code.end(), {0xe0301000u | (glcLoad ? 0x4000u : 0u), 0x80000201u, 0xbf8c3f70u, 0xe0701000u | (glcStore ? 0x4000u : 0u), 0x80010201u, 0xbf810000u});
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x5a000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        const auto words = Recompile(request).spirv;
        Module module;
        std::map<std::uint32_t, std::uint32_t> bases;
        const auto base = [&](std::uint32_t pointer) {
            const auto found = bases.find(pointer);
            const auto variable = found == bases.end() ? pointer : found->second;
            const auto name = module.names.find(variable);
            return name == module.names.end() ? std::string() : name->second;
        };
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            const auto opcode = words[cursor] & 0xffffu;
            require(count != 0 && count <= words.size() - cursor, "coherent buffers: truncated SPIR-V instruction");
            if (opcode == spv::OpName) module.names[words[cursor + 1]] = reinterpret_cast<const char*>(&words[cursor + 2]);
            if (opcode == spv::OpDecorate) module.decorations[words[cursor + 1]].push_back(words[cursor + 2]);
            if (opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain) {
                const auto found = bases.find(words[cursor + 3]);
                bases[words[cursor + 2]] = found == bases.end() ? words[cursor + 3] : found->second;
            }
            if (opcode == spv::OpLoad && base(words[cursor + 3]).starts_with("buffers")) module.loads.push_back(base(words[cursor + 3]));
            if (opcode == spv::OpStore && base(words[cursor + 1]).starts_with("buffers")) module.stores.push_back(base(words[cursor + 1]));
            cursor += count;
        }
        return module;
    };
    const auto decorated = [](const Module& module, std::string_view name, spv::Decoration decoration) {
        for (const auto& [id, variable] : module.names) {
            if (variable != name) continue;
            const auto found = module.decorations.find(id);
            return found != module.decorations.end() && std::ranges::find(found->second, static_cast<std::uint32_t>(decoration)) != found->second.end();
        }
        return false;
    };
    const auto declared = [](const Module& module, std::string_view name) {
        return std::ranges::any_of(module.names, [&](const auto& entry) { return entry.second == name; });
    };
    const auto only = [](const std::vector<std::string>& accesses, std::string_view name) {
        return !accesses.empty() && std::ranges::all_of(accesses, [&](const std::string& access) { return access == name; });
    };

    const auto plain = compile(false, false, false);
    require(!declared(plain, "buffers_coherent") && !decorated(plain, "buffers", spv::DecorationCoherent), "coherent buffers: a program without GLC or DLC accesses declares coherent buffers");
    require(only(plain.loads, "buffers") && only(plain.stores, "buffers"), "coherent buffers: a program without GLC or DLC accesses does not use the buffer array");

    const auto store = compile(false, false, true);
    require(declared(store, "buffers_coherent") && decorated(store, "buffers_coherent", spv::DecorationCoherent), "coherent buffers: a GLC store has no coherent buffer array");
    require(!decorated(store, "buffers", spv::DecorationCoherent), "coherent buffers: a GLC store makes the loads without GLC coherent");
    require(decorated(store, "buffers", spv::DecorationAliased) && decorated(store, "buffers_coherent", spv::DecorationAliased), "coherent buffers: the two buffer arrays are not decorated Aliased");
    require(only(store.loads, "buffers"), "coherent buffers: a load without GLC goes through the coherent buffer array");
    require(only(store.stores, "buffers_coherent"), "coherent buffers: a GLC store goes through the buffer array without Coherent");

    const auto load = compile(false, true, false);
    require(only(load.loads, "buffers_coherent"), "coherent buffers: a GLC load goes through the buffer array without Coherent");
    require(only(load.stores, "buffers_coherent"), "coherent buffers: a store in a program with GLC accesses goes through the buffer array without Coherent");

    const auto invalidated = compile(true, false, true);
    require(!declared(invalidated, "buffers_coherent") && decorated(invalidated, "buffers", spv::DecorationCoherent), "coherent buffers: a program that invalidates the vector caches does not keep every buffer access coherent");
    require(only(invalidated.loads, "buffers") && only(invalidated.stores, "buffers"), "coherent buffers: a program that invalidates the vector caches does not use the coherent buffer array");
}

void verifyUnnormalizedSamplers() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Format11_11_10UInt = 34;
    constexpr std::uint32_t Format32Float = 22;
    constexpr std::uint32_t Type1D = 8;
    constexpr std::uint32_t Type2D = 9;
    constexpr std::uint32_t Type3D = 10;
    constexpr std::uint32_t TypeCube = 11;
    constexpr std::uint32_t Type2DArray = 13;
    struct alignas(256) Texture { std::array<std::uint8_t, 4096> bytes{}; };
    static Texture texture;
    static std::array<std::uint32_t, 64> output{};
    const auto textureBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> unnormalized{0x00008092u, 0x00fff000u, 0x05500000u, 0u};
    const std::array<std::uint32_t, 4> normalized{0x00000092u, 0x00fff000u, 0x05500000u, 0u};
    const auto imageData = [&](const std::array<std::uint32_t, 4>& sampler, std::uint32_t type, std::uint32_t format, std::uint32_t depth) {
        return std::array<std::uint32_t, 16>{
            static_cast<std::uint32_t>(textureBase >> 8u), static_cast<std::uint32_t>((textureBase >> 40u) & 0xffu) | (format << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (type << 28u), depth, 0u, 0u, 0u,
            sampler[0], sampler[1], sampler[2], sampler[3],
            static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    };
    const auto userData = [&](const std::array<std::uint32_t, 4>& sampler) {
        return imageData(sampler, Type2D, Format8888UNorm, 0u);
    };
    const auto program = [](std::uint32_t mimg) {
        return std::vector<std::uint32_t>{0x7e020280u, 0x7e040280u, 0x7e060280u, 0x7e080280u, 0x7e0a0280u, 0x7e0c0280u, mimg, 0x00400801u, 0xe0700000u, 0x80030800u, 0xbf810000u};
    };
    const std::array<std::uint32_t, 3> capabilities{1u, static_cast<std::uint32_t>(spv::CapabilityImageGatherExtended), static_cast<std::uint32_t>(spv::CapabilityMinLod)};
    std::uint64_t nextAddress = 0x40000u;
    const auto recompile = [&](std::span<const std::uint32_t> code, const std::array<std::uint32_t, 16>& data, std::uint64_t address = 0u, bool offsets = false) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, address != 0u ? address : (nextAddress += 0x1000u), code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = data;
        request.context.compute = ShaderComputeStageInfo{{16u, 2u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.target.nonConstantImageOffsets = offsets;
        request.layout.pushConstantSizeBytes = 128;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        request.context.memory = memory.Regions();
        return Recompile(request);
    };
    const auto flags = [](const RecompileResult& result, DescriptorRole role) {
        for (const auto& binding : result.bindings) {
            if (binding.role == role) return role == DescriptorRole::GuestSamplers ? binding.samplerUnnormalized : binding.imageUnnormalized;
        }
        throw std::runtime_error("unnormalized samplers: the program has no sampler or image binding");
    };
    const auto proven = [&](const RecompileResult& result, bool expected) {
        return flags(result, DescriptorRole::GuestSamplers) == std::vector<bool>{expected} && flags(result, DescriptorRole::GuestImages) == std::vector<bool>{expected};
    };
    const auto auditSpirv = [](std::span<const std::uint32_t> words) {
        std::size_t samples = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "unnormalized samplers: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            require(op != spv::OpImageSampleImplicitLod && op != spv::OpImageSampleDrefImplicitLod && op != spv::OpImageSampleDrefExplicitLod && op != spv::OpImageGather && op != spv::OpImageDrefGather && op != spv::OpImageQueryLod, "unnormalized samplers: the SPIR-V samples in a form an unnormalized sampler does not allow");
            if (op == spv::OpImageSampleExplicitLod) {
                require(count == 7u && words[cursor + 5] == spv::ImageOperandsLodMask, "unnormalized samplers: an explicit-LOD sample has operands other than Lod");
                ++samples;
            }
            cursor += count;
        }
        require(samples != 0u, "unnormalized samplers: the SPIR-V does not sample");
    };

    const auto lz = program(0xf09c0f08u);
    const auto accepted = recompile(lz, userData(unnormalized), 0x40000u);
    require(proven(accepted, true), "unnormalized samplers: image_sample_lz through an unnormalized S# was not flagged");
    auditSpirv(accepted.spirv.Words());
    const auto plain = recompile(lz, userData(normalized), 0x40000u);
    require(proven(plain, false), "unnormalized samplers: a normalized S# was flagged");
    require(plain.variantId == accepted.variantId && plain.spirv == accepted.spirv, "unnormalized samplers: the unnormalized S# compiled another variant");

    for (const std::uint32_t mimg : {0xf0900f08u, 0xf0800f08u, 0xf0940f08u, 0xf0840f08u, 0xf0880f08u}) {
        const auto result = recompile(program(mimg), userData(unnormalized));
        require(proven(result, true), "unnormalized samplers: a compute image_sample_l, image_sample, image_sample_b, image_sample_cl or image_sample_d was not flagged");
        auditSpirv(result.spirv.Words());
    }

    const auto reject = [&](std::uint32_t mimg, const std::array<std::uint32_t, 16>& data, const char* reason, const char* what) {
        expectFailure([&] { static_cast<void>(recompile(program(mimg), data)); }, reason, what);
    };
    reject(0xf0bc0f08u, imageData(unnormalized, Type2D, Format32Float, 0u), "unnormalized guest sampler is used with depth comparison, which is not implemented", "unnormalized samplers: image_sample_c_lz was accepted");
    reject(0xf11c0108u, userData(unnormalized), "unnormalized guest sampler is used by a gather, which is not implemented", "unnormalized samplers: image_gather4_lz was accepted");
    reject(0xf1800308u, userData(unnormalized), "unnormalized guest sampler is used by image_get_lod, which is not implemented", "unnormalized samplers: image_get_lod was accepted");
    reject(0xf0800f09u, userData(unnormalized), "unnormalized guest sampler is used by an image_sample_*_a variant, which is not implemented", "unnormalized samplers: image_sample_a was accepted");
    reject(0xf09c0f18u, imageData(unnormalized, TypeCube, Format8888UNorm, 5u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a cube T# was accepted");
    reject(0xf09c0f10u, imageData(unnormalized, Type3D, Format8888UNorm, 3u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a 3D T# was accepted");
    reject(0xf09c0f28u, imageData(unnormalized, Type2DArray, Format8888UNorm, 5u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a 2D-array sample was accepted");
    reject(0xf09c0f08u, imageData(unnormalized, Type2D, Format11_11_10UInt, 0u), "unnormalized guest sampler samples an image that needs a format conversion or packed access", "unnormalized samplers: a T# with a format conversion was accepted");
    reject(0xf15c0108u, userData(unnormalized), "unnormalized guest sampler is used by a gather, which is not implemented", "unnormalized samplers: image_gather4_lz_o was accepted");
    reject(0xf0fc0f08u, imageData(unnormalized, Type2D, Format32Float, 0u), "unnormalized guest sampler is used with depth comparison, which is not implemented", "unnormalized samplers: image_sample_c_lz_o was accepted");
    reject(0xf0dc0f28u, imageData(unnormalized, Type2DArray, Format8888UNorm, 5u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a 2D-array image_sample_lz_o was accepted");
    for (const std::uint32_t mimg : {0xf0c00f08u, 0xf11c0108u, 0xf09c0f18u}) {
        static_cast<void>(recompile(program(mimg), imageData(normalized, mimg == 0xf09c0f18u ? TypeCube : Type2D, Format8888UNorm, mimg == 0xf09c0f18u ? 5u : 0u)));
    }
    const std::vector<std::uint32_t> latchSample{0xbe900380u, 0x7e020280u, 0x7e040280u, 0x7e060280u, 0x7e080280u, 0x7e0a0280u, 0x7e0c0280u, 0xf09c0f28u, 0x00400801u, 0x80108110u, 0xbf0a8410u, 0xbf85fffbu, 0xe0700000u, 0x80030800u, 0xbf810000u};
    static_cast<void>(recompile(latchSample, imageData(normalized, Type2DArray, Format8888UNorm, 5u)));

    const auto offsetProgram = [](std::uint32_t mimg, std::uint32_t offsetSource, std::uint32_t literal) {
        std::vector<std::uint32_t> code{0x7e020200u | offsetSource};
        if (offsetSource == 0xffu) code.push_back(literal);
        code.insert(code.end(), {0x7e040280u, 0x7e060280u, 0x7e080280u, 0x7e0a0280u, 0x7e0c0280u, mimg, 0x00400801u, 0xe0700000u, 0x80030800u, 0xbf810000u});
        return code;
    };
    const auto foldedSamples = [](std::span<const std::uint32_t> words) {
        std::vector<std::uint32_t> exact;
        std::vector<std::uint32_t> sums;
        std::vector<std::uint32_t> coordinates;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "unnormalized texel offsets: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            if (op == spv::OpDecorate && count == 3u && words[cursor + 2] == spv::DecorationNoContraction) exact.push_back(words[cursor + 1]);
            if (op == spv::OpFAdd) sums.push_back(words[cursor + 2]);
            if (op == spv::OpImageSampleExplicitLod) coordinates.push_back(words[cursor + 4]);
            cursor += count;
        }
        return !coordinates.empty() && std::ranges::all_of(coordinates, [&](std::uint32_t id) { return std::ranges::count(sums, id) != 0 && std::ranges::count(exact, id) != 0; });
    };
    const auto sampleMasks = [](std::span<const std::uint32_t> words) {
        std::vector<std::uint32_t> masks;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "unnormalized texel offsets: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == spv::OpImageSampleExplicitLod && count > 5u) masks.push_back(words[cursor + 5]);
            cursor += count;
        }
        return masks;
    };
    for (const std::uint32_t source : {0xffu, 0x100u}) {
        const auto oneD = recompile(offsetProgram(0xf0dc0f00u, source, 0x3du), imageData(unnormalized, Type1D, Format8888UNorm, 0u));
        require(proven(oneD, true), "unnormalized texel offsets: a 1D image_sample_lz_o through an unnormalized S# was not flagged");
        auditSpirv(oneD.spirv.Words());
        require(foldedSamples(oneD.spirv.Words()), "unnormalized texel offsets: a 1D offset was not added to the coordinate");
    }
    const auto constantLz = offsetProgram(0xf0dc0f08u, 0xffu, 0x3du | (2u << 8u));
    const auto foldedLz = recompile(constantLz, userData(unnormalized), 0x48000u);
    require(proven(foldedLz, true), "unnormalized texel offsets: image_sample_lz_o through an unnormalized S# was not flagged");
    auditSpirv(foldedLz.spirv.Words());
    require(foldedSamples(foldedLz.spirv.Words()), "unnormalized texel offsets: a constant offset was not added to the coordinates");
    const auto operandLz = recompile(constantLz, userData(normalized), 0x48000u);
    require(proven(operandLz, false) && operandLz.variantId == foldedLz.variantId && operandLz.PipelineVariantId() != foldedLz.PipelineVariantId(), "unnormalized texel offsets: a normalized S# did not get its own specialization of the shared artifact");
    require(sampleMasks(operandLz.spirv.Words()) == std::vector<std::uint32_t>{spv::ImageOperandsLodMask | spv::ImageOperandsConstOffsetMask} && !foldedSamples(operandLz.spirv.Words()), "unnormalized texel offsets: a normalized image_sample_lz_o lost its ConstOffset operand");
    require(recompile(constantLz, userData(unnormalized), 0x48000u).PipelineVariantId() == foldedLz.PipelineVariantId(), "unnormalized texel offsets: the unnormalized S# did not reuse its specialization");
    for (const std::uint32_t mimg : {0xf0dc0f08u, 0xf0d00f08u, 0xf0c00f08u, 0xf0d40f08u, 0xf0c40f08u}) {
        const auto computed = offsetProgram(mimg, 0x100u, 0u);
        for (const bool offsets : {false, true}) {
            const auto result = recompile(computed, userData(unnormalized), 0u, offsets);
            require(proven(result, true), "unnormalized texel offsets: a compute image_sample_lz_o, _l_o, _o, _b_o or _cl_o with a computed offset was not flagged");
            auditSpirv(result.spirv.Words());
            require(foldedSamples(result.spirv.Words()), "unnormalized texel offsets: a computed offset was not added to the coordinates");
        }
        expectFailure([&] { static_cast<void>(recompile(computed, userData(normalized))); }, "normalized guest sampler is used with a texel offset that is not a constant", "unnormalized texel offsets: a normalized computed offset was accepted without maintenance8");
        expectFailure([&] { static_cast<void>(recompile(computed, imageData(unnormalized, Type2D, Format11_11_10UInt, 0u))); }, "takes only images an unnormalized S# can sample", "unnormalized texel offsets: a computed offset sampled a converted image without maintenance8");
        const auto masks = sampleMasks(recompile(computed, userData(normalized), 0u, true).spirv.Words());
        require(!masks.empty() && std::ranges::all_of(masks, [](std::uint32_t mask) { return (mask & spv::ImageOperandsOffsetMask) != 0u; }), "unnormalized texel offsets: a normalized computed offset lost its Offset operand");
    }

    const auto gradient = program(0xf0880f08u);
    const auto gradientAddress = nextAddress += 0x1000u;
    const auto gradientUnnormalized = recompile(gradient, userData(unnormalized), gradientAddress);
    const auto gradientNormalized = recompile(gradient, userData(normalized), gradientAddress);
    require(proven(gradientNormalized, false) && gradientNormalized.variantId == gradientUnnormalized.variantId && gradientNormalized.PipelineVariantId() != gradientUnnormalized.PipelineVariantId(), "unnormalized samplers: a normalized image_sample_d did not get its own specialization of the shared artifact");
    require(sampleMasks(gradientNormalized.spirv.Words()) == std::vector<std::uint32_t>{spv::ImageOperandsGradMask}, "unnormalized samplers: a normalized image_sample_d lost its derivatives");
    require(sampleMasks(gradientUnnormalized.spirv.Words()) == std::vector<std::uint32_t>{spv::ImageOperandsLodMask}, "unnormalized samplers: an unnormalized image_sample_d kept its derivatives");

    const std::array<std::uint32_t, 5> pixelCode{0xf0800f08u, 0x00400801u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};
    const auto pixelData = userData(unnormalized);
    RecompileRequest pixel{};
    pixel.shader = {ShaderStage::Fragment, 0x4f000u, pixelCode, 0, {}};
    pixel.context.waveSize = 64;
    pixel.context.userDataBaseRegister = 0;
    pixel.context.userData = pixelData;
    pixel.context.pixel = twoParameterPixel();
    pixel.target.vulkanVersion = 0x00401000u;
    pixel.target.spirvVersion = 0x00010300u;
    pixel.target.subgroupSize = 64;
    pixel.layout.pushConstantSizeBytes = 128;
    pixel.useCache = false;
    const auto implicitSamples = [](std::span<const std::uint32_t> words) {
        std::size_t samples = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "unnormalized samplers: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == spv::OpImageSampleImplicitLod) ++samples;
            cursor += count;
        }
        return samples;
    };
    const auto pixelUnnormalized = Recompile(pixel);
    require(proven(pixelUnnormalized, true), "unnormalized samplers: a pixel image_sample through an unnormalized S# was not flagged");
    auditSpirv(pixelUnnormalized.spirv.Words());
    const auto normalizedPixelData = userData(normalized);
    pixel.context.userData = normalizedPixelData;
    const auto pixelNormalized = Recompile(pixel);
    require(proven(pixelNormalized, false) && implicitSamples(pixelNormalized.spirv.Words()) == 1u, "unnormalized samplers: a pixel image_sample through a normalized S# is not an implicit-LOD sample");
}

void verifyUnusedUnnormalizedSampler() {
    using namespace ShaderRecompiler;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.numericClass = IrTextureNumericClass::Float;
    image.dimension = RdnaImageDimension::Dim2D;
    image.read = true;
    ShaderInfo info;
    info.images = {image};
    info.samplers = {SamplerResource{}};
    info.sampledPairs = {{0u, 0u, 0u}};
    ResourceSnapshot snapshot;
    snapshot.images = {DescriptorValue{{0x00001000u, 0x03800000u, 0x0000c000u, 0x90000facu, 0u, 0u, 0u, 0u}, 8u}};
    snapshot.samplers = {DescriptorValue{{0x00008092u, 0x00fff000u, 0x05500000u, 0u}, 4u}};
    const auto populate = [&](ShaderInfo shader) {
        shader.runtimeImageModes = {ResourceMaterializer::RuntimeImageModes(shader.images[0])};
        BindingAllocationResult allocation;
        allocation.layout.descriptors = {{DescriptorBindingForImage(shader.images[0]), {0u}}, {DescriptorBindingKind::Samplers, {0u}}};
        DescriptorBindingBuilder{}.Populate(allocation, shader, IrShaderStage::Compute, 0u, snapshot, {});
        return allocation.bindings;
    };
    auto constantModes = ResourceMaterializer::RuntimeImageModes(image);
    std::erase_if(constantModes, [](const auto& mode) { return !mode.constantSwizzle; });
    require(!constantModes.empty(), "constant image selectors: no dedicated runtime modes were prepared");
    require(!ResourceMaterializer::TryRuntimeImageMode(image, snapshot.images[0], constantModes).mode.has_value(), "constant image selectors: ordinary selectors selected a constant-only mode");
    auto constantDescriptor = snapshot.images[0];
    constantDescriptor.dwords[3] = 0xa0000041u;
    require(ResourceMaterializer::TryRuntimeImageMode(image, constantDescriptor, constantModes).mode.has_value(), "constant image selectors: constant selectors did not select their dedicated mode");
    auto tableImage = image;
    tableImage.table = 0u;
    const auto tableModes = ResourceMaterializer::RuntimeImageModes(tableImage);
    require(std::none_of(tableModes.begin(), tableModes.end(), [](const auto& mode) { return mode.constantSwizzle; }), "constant image selectors: a table gained a direct-only constant mode");
    const auto bindings = populate(info);
    require(bindings.size() == 2u && bindings[0].imageUnnormalized == std::vector<bool>{true} && bindings[1].samplerUnnormalized == std::vector<bool>{true}, "unnormalized samplers: an S# without live uses was not flagged");
    auto selected = info;
    selected.images[0].table = 0u;
    expectFailure([&] { static_cast<void>(populate(selected)); }, "unnormalized guest sampler samples an image selected at run time, which is not implemented", "unnormalized samplers: an image table root was accepted");
    snapshot.images[0].dwords[3] = 0xa0000041u;
    const auto constant = populate(info);
    require(constant.size() == 1u && constant[0].samplerUnnormalized == std::vector<bool>{true}, "unnormalized samplers: a 3D view whose channels select constants was bound or refused");
    expectFailure([&] { static_cast<void>(populate(selected)); }, "unnormalized guest sampler samples an image selected at run time, which is not implemented", "unnormalized samplers: an image table root whose channels select constants was accepted");
    snapshot.images[0].dwords[3] = 0x90000facu;
    auto compared = info;
    compared.samplers[0].depthCompare = true;
    expectFailure([&] { static_cast<void>(populate(compared)); }, "unnormalized guest sampler is used with depth comparison, which is not implemented", "unnormalized samplers: a depth-compare S# without live uses was accepted");
    auto offset = info;
    offset.samplers[0].uses = SamplerUseExplicitLod | SamplerUseOffset;
    const auto folded = populate(offset);
    require(folded[0].imageUnnormalized == std::vector<bool>{true} && folded[1].samplerUnnormalized == std::vector<bool>{true}, "unnormalized samplers: an offset use was not flagged");
    auto gathered = offset;
    gathered.samplers[0].uses |= SamplerUseGather;
    expectFailure([&] { static_cast<void>(populate(gathered)); }, "unnormalized guest sampler is used by a gather, which is not implemented", "unnormalized samplers: an offset gather was accepted");
    auto foldOnly = offset;
    foldOnly.samplers[0].foldTexelOffsetsOnly = true;
    static_cast<void>(populate(foldOnly));
    auto foldVolume = foldOnly;
    foldVolume.images[0].dimension = RdnaImageDimension::Dim3D;
    foldVolume.images[0].foldTexelOffsetsOnly = true;
    snapshot.images[0].dwords[3] = 0xa0000041u;
    const auto foldedConstant = populate(foldVolume);
    require(foldedConstant.size() == 1u && foldedConstant[0].samplerUnnormalized == std::vector<bool>{true}, "unnormalized texel offsets: a 3D view whose channels select constants was bound or refused");
    snapshot.images[0].dwords[3] = 0xa0000facu;
    expectFailure([&] { static_cast<void>(populate(foldVolume)); }, "takes only images an unnormalized S# can sample", "unnormalized texel offsets: an ordinary 3D view matched a constant-only mode");
    snapshot.images[0].dwords[3] = 0x90000facu;
    const auto atBaseLevel = [&](const ShaderInfo& shader) { return populate(shader)[0].imageUnnormalizedBaseLevel; };
    auto levelZero = info;
    levelZero.samplers[0].uses = SamplerUseExplicitLod;
    require(atBaseLevel(levelZero) == std::vector<bool>{true}, "unnormalized samplers: an image read only by image_sample_lz was not flagged as exact at its base level");
    require(atBaseLevel(info) == std::vector<bool>{false}, "unnormalized samplers: an image without live uses was flagged as exact at its base level");
    require(atBaseLevel(offset) == std::vector<bool>{false}, "unnormalized samplers: an image read with a texel offset was flagged as exact at its base level");
    auto selectedLod = levelZero;
    selectedLod.samplers[0].uses |= SamplerUseNonZeroLod;
    require(atBaseLevel(selectedLod) == std::vector<bool>{false}, "unnormalized samplers: an image also read at a selected LOD was flagged as exact at its base level");
    snapshot.samplers[0].dwords[1] = 0x00fff100u;
    require(atBaseLevel(levelZero) == std::vector<bool>{false}, "unnormalized samplers: an S# with a minimum LOD was flagged as exact at the base level");
    snapshot.samplers[0].dwords[1] = 0x00fff000u;
    snapshot.samplers[0].dwords[2] = 0x05500180u;
    require(atBaseLevel(levelZero) == std::vector<bool>{false}, "unnormalized samplers: an S# with a LOD bias was flagged as exact at the base level");
    snapshot.samplers[0].dwords[2] = 0x05500000u;
    auto implicitUse = levelZero;
    implicitUse.samplers[0].uses = SamplerUseImplicitLod;
    snapshot.samplers[0].dwords[1] = 0x00fff100u;
    snapshot.samplers[0].dwords[2] = 0x01500180u;
    require(atBaseLevel(implicitUse) == std::vector<bool>{true}, "unnormalized samplers: an image read through an S# without a mip filter was not flagged as exact at its base level");
    require(atBaseLevel(selectedLod) == std::vector<bool>{true}, "unnormalized samplers: an image read at a selected LOD through an S# without a mip filter was not flagged as exact at its base level");
    snapshot.samplers[0].dwords[2] = 0x05500180u;
    require(atBaseLevel(implicitUse) == std::vector<bool>{false}, "unnormalized samplers: an image read through an S# with a point mip filter was flagged as exact at its base level");
    snapshot.samplers[0].dwords[1] = 0x00fff000u;
    snapshot.samplers[0].dwords[2] = 0x05500000u;
    snapshot.samplers[0].dwords[0] = 0x00000092u;
    const auto normalized = populate(info);
    require(normalized[0].imageUnnormalized == std::vector<bool>{false} && normalized[1].samplerUnnormalized == std::vector<bool>{false}, "unnormalized samplers: a normalized S# was flagged");
    static_cast<void>(populate(offset));
    expectFailure([&] { static_cast<void>(populate(foldOnly)); }, "normalized guest sampler is used with a texel offset that is not a constant", "unnormalized samplers: a normalized S# was accepted for an offset that only folds");
}

void verifyWaveUniformValues() {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    MemoryInfo scalar;
    scalar.kind = ResourceKind::ScalarAddress;
    MemoryInfo global;
    global.kind = ResourceKind::Global;
    program.Resources().memoryInfo = {scalar, global};
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint64_t flags = 0) -> IrValue& {
        auto& value = program.CreateValue(opcode, type, flags);
        for (auto* argument : arguments) value.AddArgument(argument);
        block.AppendInstruction(&value);
        return value;
    };
    const auto memory = [](std::uint32_t index) {
        MemoryFlags flags{index, 0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        return bits;
    };
    auto& zero = program.CreateValue(IrOpcode::Void, IrType::U32);
    zero.SetImmediateU32(0u);
    auto& active = program.CreateValue(IrOpcode::Void, IrType::U1);
    active.SetImmediateBool(true);
    auto& userData = emit(IrOpcode::GetUserData, IrType::U32, {&zero});
    auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
    auto& uniformSum = emit(IrOpcode::IAdd32, IrType::U32, {&userData, &userData});
    auto& laneSum = emit(IrOpcode::IAdd32, IrType::U32, {&userData, &lane});
    auto& address = emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&userData, &userData});
    auto& scalarLoad = emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &uniformSum, &zero, &active}, memory(0u));
    auto& laneOffsetLoad = emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &laneSum, &zero, &active}, memory(0u));
    auto& globalLoad = emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &uniformSum, &zero, &active}, memory(1u));
    auto& fromScalarLoad = emit(IrOpcode::IAdd32, IrType::U32, {&scalarLoad, &uniformSum});
    auto& fromGlobalLoad = emit(IrOpcode::IAdd32, IrType::U32, {&globalLoad, &uniformSum});
    auto& compare = emit(IrOpcode::ULessThan32, IrType::U1, {&laneSum, &userData});
    auto& ballot = emit(IrOpcode::Ballot, IrType::U32x4, {&compare});
    const auto uniform = WaveUniformValues(program);
    for (const auto* value : {&userData, &uniformSum, &address, &scalarLoad, &fromScalarLoad, &ballot}) {
        require(uniform.contains(value), "wave-uniform values: a value every lane of the wave computes alike was not found uniform");
    }
    for (const auto* value : {&lane, &laneSum, &laneOffsetLoad, &globalLoad, &fromGlobalLoad, &compare}) {
        require(!uniform.contains(value), "wave-uniform values: a value that may differ between lanes was found uniform");
    }
}

void verifyTwoLaneUniformValues() {
    using namespace ShaderRecompiler;
    struct alignas(4096) GuestTables {
        std::array<std::uint32_t, 64> output{};
        std::array<std::uint32_t, 8> srt{};
    };
    static GuestTables guest;
    auto& output = guest.output;
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    auto& srt = guest.srt;
    srt = {6u, 7u, 0u, 0u, static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 10> code{0xf4040080u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xbf8cc07fu, 0x93040302u, 0x4a020004u, 0xe0700000u, 0x80020100u, 0xbf810000u};
    const auto multiplies = [&](std::uint32_t subgroupSize, bool multiply) {
        auto shaderCode = code;
        if (!multiply) shaderCode[5] = 0x80040302u;
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x40000u, shaderCode, 0, {}};
        request.target = BufferTarget();
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        request.context.memory = memory.Regions();
        const auto words = Recompile(request, *capture)->spirv;
        std::size_t count = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto length = words[cursor] >> 16u;
            require(length != 0 && length <= words.size() - cursor, "two-lane uniform values: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == spv::OpIMul) ++count;
            cursor += length;
        }
        return count;
    };
    for (const auto subgroupSize : {32u, 64u}) {
        require(multiplies(subgroupSize, true) == multiplies(subgroupSize, false) + 1, "two-lane uniform values: the scalar multiply must be emitted once per invocation");
    }
}

void verifyBdaReadFallbackFunctions() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    alignas(256) static std::array<std::uint32_t, 32> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 8> userData{0x10000000u, 0u, 0u, 0u, static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 128u, 0x01016facu};
    const auto compile = [&](std::uint32_t loads, bool barrier, bool coherent, std::uint32_t stride) {
        std::vector<std::uint32_t> code{0x7e020200u, 0x7e040201u};
        for (std::uint32_t load = 0; load < loads; ++load) {
            code.push_back(0xdc308000u | (coherent ? 0x10000u : 0u) | (stride * load + 4u));
            code.push_back(((3u + load) << 24u) | 0x007d0001u);
        }
        code.push_back(0xbf8c3f70u);
        for (std::uint32_t load = 1; load < loads; ++load) code.push_back(0x4a060103u | ((3u + load) << 9u));
        if (barrier) code.push_back(0xbf8a0000u);
        code.insert(code.end(), {0xe0700000u, 0x80010300u, 0xbf810000u});
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x50000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    for (const bool barrier : {false, true}) {
        for (const bool coherent : {false, true}) {
            const auto one = compile(1u, barrier, coherent, 32u);
            const auto five = compile(5u, barrier, coherent, 32u);
            const auto adjacent = compile(5u, barrier, coherent, 4u);
            std::map<std::uint32_t, std::string> names;
            std::map<std::string, std::uint32_t> definitions;
            const auto reader = std::string("read_bda_dword_bytes") + (barrier ? "" : "_stop") + (coherent ? "_coherent" : "");
            const auto span = std::string("read_bda_span") + (barrier ? "" : "_stop") + (coherent ? "_coherent" : "");
            const auto loader = std::string("load_bda_span") + (coherent ? "_coherent" : "");
            std::string function;
            std::size_t compareExchanges = 0;
            std::size_t mainLookups = 0;
            std::size_t mainProbes = 0;
            std::size_t spanCalls = 0;
            std::size_t spanLoads = 0;
            std::size_t spanLoaderCalls = 0;
            std::size_t loaderLoads = 0;
            std::size_t loaderCalls = 0;
            std::array<std::size_t, 2> readerLoads{};
            std::size_t lookupFaults = 0;
            std::size_t lookupVariables = 0;
            std::size_t readerLookups = 0;
            for (std::size_t cursor = 5; cursor < five.size();) {
                const auto length = five[cursor] >> 16u;
                require(length != 0 && length <= five.size() - cursor, "BDA read functions: truncated SPIR-V instruction");
                const auto op = five[cursor] & 0xffffu;
                if (op == spv::OpName) names[five[cursor + 1]] = reinterpret_cast<const char*>(&five[cursor + 2]);
                if (op == spv::OpFunction) {
                    function = names[five[cursor + 2]];
                    if (function == "record_bda_fault" || function.starts_with("read_bda_dword_bytes") || function.starts_with("read_bda_span") || function.starts_with("load_bda_span")) {
                        require((five[cursor + 3] & spv::FunctionControlDontInlineMask) != 0u, "BDA read functions: a fault, byte read or span read function may be inlined");
                        ++definitions[function];
                    }
                }
                if (op == spv::OpAtomicCompareExchange) ++compareExchanges;
                if (op == spv::OpFunctionCall && function == "main" && names[five[cursor + 3]] == "get_bda_pointer") ++mainLookups;
                if (op == spv::OpFunctionCall && function == "main") {
                    const auto& callee = names[five[cursor + 3]];
                    if (callee == "probe_bda_pointer" || callee.starts_with("read_bda_dword_bytes")) ++mainProbes;
                    if (callee == span) ++spanCalls;
                }
                if ((op == spv::OpLoad || op == spv::OpAtomicLoad) && function == span) ++spanLoads;
                if (op == spv::OpFunctionCall && function == span && names[five[cursor + 3]] == loader) ++spanLoaderCalls;
                if ((op == spv::OpLoad || op == spv::OpAtomicLoad) && function == loader) ++loaderLoads;
                if (op == spv::OpFunctionCall && function == loader) ++loaderCalls;
                if (op == spv::OpLoad && function == reader) ++readerLoads[length > 4u && (five[cursor + 4] & spv::MemoryAccessVolatileMask) != 0u];
                if (op == spv::OpFunctionCall && function == "get_bda_pointer" && names[five[cursor + 3]] == "record_bda_fault") ++lookupFaults;
                if (op == spv::OpVariable && function == "get_bda_pointer") ++lookupVariables;
                if (op == spv::OpFunctionCall && function == reader && names[five[cursor + 3]] == "get_bda_pointer") ++readerLookups;
                cursor += length;
            }
            require(definitions["record_bda_fault"] == 1u && definitions[reader] == 1u && definitions[span] == 1u, "BDA read functions: the fault, byte read and span read functions are not defined");
            require(definitions[loader] == 1u && loaderLoads == 4u && loaderCalls == 0u, "BDA read functions: the span load function does not hold the four in-place loads and nothing else");
            require(spanLoads == 0u && spanLoaderCalls == 1u, "BDA read functions: a span read function loads in place itself");
            for (const auto& [name, count] : definitions) require(count == 1u, "BDA read functions: a function is defined twice");
            require(compareExchanges == 1u, "BDA read functions: a fault is recorded outside record_bda_fault");
            require(mainLookups == 0u, "BDA read functions: a read site looks up its bytes inline");
            require(readerLoads[coherent] == 1u && readerLoads[!coherent] == 0u, "BDA read functions: the byte loads do not keep the access's coherence");
            require(lookupFaults <= 4u, "BDA read functions: the page-table lookup records its faults from more than 4 call sites");
            require(lookupVariables == 0u, "BDA read functions: the page-table lookup keeps its search bounds in function variables");
            require(readerLookups == 1u, "BDA read functions: the byte read does not look up its bytes once, in a loop");
            require(mainProbes == 0u, "BDA read functions: a read site probes or reads bytes outside its span read function");
            require(spanCalls == 5u, "BDA read functions: a read site does not call its span read function once");
            require(five.size() - one.size() < 4u * 150u, "BDA read functions: a read site takes 150 SPIR-V words or more");
            std::size_t adjacentSpanCalls = 0;
            names.clear();
            function.clear();
            for (std::size_t cursor = 5; cursor < adjacent.size();) {
                const auto length = adjacent[cursor] >> 16u;
                require(length != 0 && length <= adjacent.size() - cursor, "BDA read functions: truncated SPIR-V instruction");
                const auto op = adjacent[cursor] & 0xffffu;
                if (op == spv::OpName) names[adjacent[cursor + 1]] = reinterpret_cast<const char*>(&adjacent[cursor + 2]);
                if (op == spv::OpFunction) function = names[adjacent[cursor + 2]];
                if (op == spv::OpFunctionCall && function == "main" && names[adjacent[cursor + 3]] == span) ++adjacentSpanCalls;
                cursor += length;
            }
            require(adjacentSpanCalls == 2u, "BDA read functions: five adjacent dwords of one address do not share two span reads");
        }
    }
}

void verifyFunctionLdsBound() {
    using namespace ShaderRecompiler;
    const auto build = [](const auto& body) {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Pixel;
        for (const std::uint32_t offset : {0u, 256u, 512u, 0xfffffff0u}) {
            MemoryInfo lds;
            lds.kind = ResourceKind::Lds;
            lds.offset = offset;
            program.Resources().memoryInfo.push_back(lds);
        }
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        program.BlockOrder().push_back(&block);
        const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint32_t memory = ~0u) -> IrValue& {
            std::uint64_t bits = 0;
            if (memory != ~0u) {
                MemoryFlags flags{memory, 0u};
                std::memcpy(&bits, &flags, sizeof(flags));
            }
            auto& value = program.CreateValue(opcode, type, bits);
            for (auto* argument : arguments) value.AddArgument(argument);
            block.AppendInstruction(&value);
            return value;
        };
        const auto constant = [&](std::uint32_t immediate) -> IrValue& {
            auto& value = program.CreateValue(IrOpcode::Void, IrType::U32);
            value.SetImmediateU32(immediate);
            return value;
        };
        auto& active = program.CreateValue(IrOpcode::Void, IrType::U1);
        active.SetImmediateBool(true);
        body(program, block, emit, constant, active);
        return std::pair{FunctionLdsDwords(program), AnalyzeProgramRequirements(program)};
    };

    const auto [laneSlots, laneRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& address = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(2u)});
        for (const std::uint32_t memory : {0u, 1u, 2u}) emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &lane, &active}, memory);
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&address, &active}, 2u);
    });
    require(laneSlots == 192u, "function LDS: lane-strided slots up to byte 767 must take 192 dwords");
    require(laneRequirements.functionLds && laneRequirements.functionLdsDwords == 192u, "function LDS: the requirements do not carry the bounded size");
    {
        std::vector<std::uint32_t> rebased;
        for (const auto& [inst, address] : laneRequirements.functionLdsAddresses) rebased.push_back(address);
        std::sort(rebased.begin(), rebased.end());
        require(rebased == std::vector<std::uint32_t>{0u, 0u, 0u, 0u}, "function LDS: lane-strided slots must be addressed without the lane term");
    }

    const auto [mixed, mixedRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& address = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(2u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &lane, &active}, 0u);
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&constant(8u), &active}, 0u);
    });
    require(mixed == 64u && mixedRequirements.functionLdsAddresses.empty(), "function LDS: a constant address beside lane-strided ones must keep the lane term");

    const auto [strides, stridesRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& four = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(2u)});
        auto& eight = emit(IrOpcode::IMul32, IrType::U32, {&lane, &constant(8u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&four, &lane, &active}, 0u);
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&eight, &active}, 0u);
    });
    require(strides == 128u && stridesRequirements.functionLdsAddresses.empty(), "function LDS: two lane strides must keep the lane term");

    const auto [halfWord, halfWordRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& address = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(1u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &lane, &active}, 0u);
    });
    require(halfWord == 64u && halfWordRequirements.functionLdsAddresses.empty(), "function LDS: a lane stride that is not whole dwords must keep the lane term");

    const auto [summed, summedRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& scaled = emit(IrOpcode::IMul32, IrType::U32, {&constant(16u), &lane});
        auto& low = emit(IrOpcode::IAdd32, IrType::U32, {&scaled, &constant(4u)});
        auto& other = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& shifted = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&other, &constant(4u)});
        auto& high = emit(IrOpcode::IAdd32, IrType::U32, {&constant(1024u), &shifted});
        emit(IrOpcode::WriteSharedU32x2, IrType::Void, {&low, &lane, &lane, &active}, 0u);
        emit(IrOpcode::LoadSharedU32x4, IrType::U32x4, {&high, &active}, 1u);
    });
    {
        std::vector<std::uint32_t> rebased;
        for (const auto& [inst, address] : summedRequirements.functionLdsAddresses) rebased.push_back(address);
        std::sort(rebased.begin(), rebased.end());
        require(rebased == std::vector<std::uint32_t>{4u, 1024u}, "function LDS: lane ids of separate instructions with one stride must share the rebase");
        require(summedRequirements.functionLdsDwords == 384u && summed == 576u, "function LDS: the rebased array must hold the rebased addresses");
    }

    const auto [wide, wideRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& masked = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lane, &constant(7u)});
        auto& scaled = emit(IrOpcode::IMul32, IrType::U32, {&masked, &constant(16u)});
        auto& chosen = emit(IrOpcode::SelectU32, IrType::U32, {&active, &scaled, &constant(0x100u)});
        emit(IrOpcode::LoadSharedU32x4, IrType::U32x4, {&chosen, &active}, 0u);
    });
    require(wide == 128u, "function LDS: a 4-dword access at byte 0x100 must take 68 dwords, rounded to 128");
    require(wideRequirements.functionLdsDwords == 128u, "function LDS: the requirements do not carry the wide access's size");
    require(wideRequirements.functionLdsAddresses.empty(), "function LDS: a selected address must keep the lane term");

    const auto [unbounded, unboundedRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& user = emit(IrOpcode::GetUserData, IrType::U32, {&constant(0u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&user, &user, &active}, 0u);
    });
    require(unbounded == FunctionLdsDwordLimit && unboundedRequirements.functionLdsDwords == FunctionLdsDwordLimit, "function LDS: an address without a bound must keep the full array");

    const auto [counter, counterRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& m0 = emit(IrOpcode::GetUserData, IrType::U32, {&constant(0u)});
        emit(IrOpcode::DataAppend, IrType::U32, {&m0, &active}, 2u);
        emit(IrOpcode::DataConsume, IrType::U32, {&m0, &active}, 1u);
    });
    require(counter == 192u && counterRequirements.functionLdsDwords == 192u, "function LDS: an LDS append or consume must size the array by its offset, not by M0");

    const auto wrapping = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&constant(0x20u), &active}, 3u);
    }).first;
    require(wrapping == FunctionLdsDwordLimit, "function LDS: an offset that can wrap the address must keep the full array");

    const auto loop = build([](IrProgram& program, IrBlock& block, auto& emit, auto& constant, IrValue& active) {
        auto& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
        block.AppendInstruction(&phi);
        auto& next = emit(IrOpcode::IAdd32, IrType::U32, {&phi, &constant(4u)});
        phi.AddPhiOperand(&block, &constant(0u));
        phi.AddPhiOperand(&block, &next);
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&phi, &next, &active}, 0u);
    }).first;
    require(loop == FunctionLdsDwordLimit, "function LDS: a loop-carried address must keep the full array");

    const auto joined = build([](IrProgram& program, IrBlock& block, auto& emit, auto& constant, IrValue& active) {
        auto& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
        block.AppendInstruction(&phi);
        phi.AddPhiOperand(&block, &constant(0x40u));
        phi.AddPhiOperand(&block, &constant(0x3fcu));
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&phi, &constant(1u), &active}, 0u);
    }).first;
    require(joined == 256u, "function LDS: a phi of bounded addresses must take its largest");

    const auto unsized = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        emit(IrOpcode::LoadShared, IrType::U32, {&constant(0u), &active}, 0u);
    }).first;
    require(unsized == FunctionLdsDwordLimit, "function LDS: an access without a known width must keep the full array");
}

void verifyFunctionLdsUpdates() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityClipDistance};
    const auto compile = [](ShaderStage stage, std::uint32_t waveSize, std::span<const std::uint32_t> body) {
        std::vector<std::uint32_t> code{0x7e000280u, 0x7e020292u, 0xbefc03ffu, 0x0000ffffu};
        code.insert(code.end(), body.begin(), body.end());
        code.insert(code.end(), {0xbf8cc07fu, 0xd8d80000u, 0x02000000u, 0xbf8cc07fu, stage == ShaderStage::Fragment ? 0xf800180fu : 0xf80008cfu, 0x02020202u, 0xbf810000u});
        RecompileRequest request{};
        request.shader = {stage, 0x40000u, code, 0, {}};
        request.context.waveSize = waveSize;
        if (stage == ShaderStage::Fragment) {
            ShaderPixelStageInfo info{};
            info.wave32 = waveSize == 32u;
            info.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
            info.hasPerspectiveCenterVgpr = true;
            info.targetOutputMode[0] = 9u;
            info.targetExportMapping.fill(0xe4u);
            request.context.pixel = info;
        } else {
            request.context.vertex = ShaderVertexStageInfo{};
        }
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32u;
        request.target.supportedCapabilities = capabilities;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request);
    };
    const auto violations = [](std::span<const std::uint32_t> words) {
        std::map<std::uint32_t, std::uint32_t> storage;
        std::map<std::uint32_t, std::uint32_t> constants;
        std::map<std::uint32_t, std::uint32_t> types;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
            if (op == spv::OpTypePointer) storage[words[at + 1]] = words[at + 2];
            if (op == spv::OpConstant && (words[at] >> 16u) == 4u) constants[words[at + 2]] = words[at + 3];
            if (op == spv::OpVariable || op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain) types[words[at + 2]] = words[at + 1];
        }
        const auto workgroupScope = [&](std::uint32_t id) { return constants.count(id) != 0 && constants.at(id) == spv::ScopeWorkgroup; };
        std::size_t found = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
            if (op == spv::OpMemoryBarrier && workgroupScope(words[at + 1])) ++found;
            if ((op < spv::OpAtomicLoad || op > spv::OpAtomicXor) && op != spv::OpAtomicFMinEXT && op != spv::OpAtomicFMaxEXT && op != spv::OpAtomicFAddEXT) continue;
            const auto pointer = words[at + (op == spv::OpAtomicStore ? 1u : 3u)];
            const auto type = types.find(pointer);
            if (type != types.end() && storage.count(type->second) != 0 && storage.at(type->second) == spv::StorageClassFunction) ++found;
            if (workgroupScope(words[at + (op == spv::OpAtomicStore ? 2u : 4u)])) ++found;
        }
        return found;
    };
    using Program = std::pair<const char*, std::array<std::uint32_t, 2>>;
    static constexpr std::array<Program, 8> programs{{
        {"ds_write_b8", {0xd8780000u, 0x00000100u}},
        {"ds_write_b16", {0xd87c0000u, 0x00000100u}},
        {"ds_write_b8_d16_hi", {0xda800000u, 0x00000100u}},
        {"ds_write_b16_d16_hi", {0xda840000u, 0x00000100u}},
        {"ds_add_u32", {0xd8000000u, 0x00000100u}},
        {"ds_inc_rtn_u32", {0xd88c0000u, 0x03000100u}},
        {"ds_mskor_b32", {0xd8300000u, 0x00010100u}},
        {"ds_append", {0xd8f80000u, 0x03000000u}},
    }};
    for (const auto& [name, body] : programs) {
        for (const auto stage : {ShaderStage::Fragment, ShaderStage::Vertex}) {
            for (const auto waveSize : {64u, 32u}) {
                const auto what = std::string("function LDS: ") + name + (stage == ShaderStage::Fragment ? " in a pixel" : " in a vertex") + " wave" + std::to_string(waveSize) + " program";
                const auto result = compile(stage, waveSize, body);
                require(!result.spirv.empty(), (what + " did not compile").c_str());
                require(violations(result.spirv.Words()) == 0, (what + " uses an atomic on a Function pointer or a Workgroup memory scope").c_str());
            }
        }
    }
}

void verifySrtReadPatching() {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    MemoryInfo memory;
    memory.kind = ResourceKind::ScalarAddress;
    program.Resources().memoryInfo.push_back(memory);

    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& address = builder.Emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&builder.Constant(0u), &builder.Constant(0u)});
    auto& active = program.CreateValue(IrOpcode::Void, IrType::U1);
    active.SetImmediateBool(true);
    auto& read = builder.Emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &builder.Constant(0u), &builder.Constant(0u), &active}, MemoryFlags{0u, 0u});
    auto& repeatedUse = builder.Emit(IrOpcode::IAdd32, IrType::U32, {&read, &read});
    auto& secondUser = builder.Emit(IrOpcode::IMul32, IrType::U32, {&read, &builder.Constant(2u)});

    SrtWalker{}.BuildPlan(program);

    require(program.Resources().srtReads.size() == 1u, "SRT read patching: the scalar read was not planned");
    const auto* replacement = repeatedUse.Argument(0);
    require(replacement->Opcode() == IrOpcode::ReadConst, "SRT read patching: the uses do not reference a flat read");
    require(repeatedUse.Argument(1) == replacement && secondUser.Argument(0) == replacement, "SRT read patching: not every operand uses the same replacement");
    const auto& remainingUses = read.OperandUses();
    require(remainingUses.size() == 1u && remainingUses.front().user->Opcode() == IrOpcode::ReferenceU32, "SRT read patching: the original read retains non-planning uses");
}

struct NestedRequest {
    std::vector<std::uint32_t> code;
    std::array<std::uint32_t, 2> userData{};
    ShaderRecompiler::RecompileRequest request{};

    NestedRequest(std::vector<std::uint32_t> words, const void* srt) : code(std::move(words)) {
        using namespace ShaderRecompiler;
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt));
        userData = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
        request.shader = {ShaderStage::Compute, 0x60000u + static_cast<std::uint64_t>(code.size()) * 4u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target = BufferTarget();
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
    }
};

bool RegionsCover(const std::vector<ShaderRecompiler::MemoryRegion>& regions, const void* pointer, std::size_t bytes) {
    auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    const auto end = address + bytes;
    while (address < end) {
        const auto region = std::find_if(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& candidate) { return address >= candidate.guestAddress && address < candidate.guestAddress + candidate.bytes.size(); });
        if (region == regions.end()) return false;
        address = region->guestAddress + region->bytes.size();
    }
    return true;
}

bool RegionsTouch(const std::vector<ShaderRecompiler::MemoryRegion>& regions, std::uint64_t address, std::size_t bytes) {
    return std::any_of(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& region) { return region.guestAddress < address + bytes && address < region.guestAddress + region.bytes.size(); });
}

bool GuardRecords(const ShaderRecompiler::ResourceCapture& capture) {
    const auto& plan = *capture.plan;
    const auto& flat = capture.snapshot.flattenedSrt;
    const auto& poison = capture.snapshot.srtPoison;
    const auto flags = plan.srtReads.size();
    const auto guards = plan.guardedSrtSlots.size();
    if (flat.size() != flags + guards + 3u * poison.size()) return false;
    for (std::size_t guard = 0; guard < guards; ++guard) {
        const auto record = std::find_if(poison.begin(), poison.end(), [&](const ShaderRecompiler::SrtReadPoison& entry) { return entry.slot == plan.guardedSrtSlots[guard]; });
        const auto expected = record == poison.end() ? 0u : static_cast<std::uint32_t>(record - poison.begin()) + 1u;
        if (flat[flags + guard] != expected) return false;
    }
    for (std::size_t record = 0; record < poison.size(); ++record) {
        const auto* words = flat.data() + flags + guards + 3u * record;
        if (words[0] != poison[record].pc || words[1] != static_cast<std::uint32_t>(poison[record].address) || words[2] != static_cast<std::uint32_t>(poison[record].address >> 32u) || flat.at(poison[record].slot) != 0u) return false;
    }
    return true;
}

void verifyGuardedNullPointers() {
    using namespace ShaderRecompiler;
    constexpr std::size_t Reserved = 0x10000;
    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, Reserved, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, Reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    require(reserved != nullptr, "guarded pointer: cannot reserve an inaccessible range");
    const auto unmapped = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(reserved));
    alignas(256) static std::array<std::uint32_t, 64> output{};
    alignas(256) static std::array<std::uint32_t, 16> root{};
    static std::uint32_t payload = 0x3f800000u;
    static std::uint64_t indirect = 0;
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    root = {static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(output)), 0x30027facu, 1u};
    const auto point = [&](std::uint64_t address) {
        root[6] = static_cast<std::uint32_t>(address);
        root[7] = static_cast<std::uint32_t>(address >> 32u);
    };
    const auto payloadAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&payload));
    const std::vector<std::uint32_t> branchCode{0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
        0xbf8cc07fu, 0xbf068008u, 0xbf850003u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu, 0x7e02020cu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u};
    const std::vector<std::uint32_t> chainCode{0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
        0xbf8cc07fu, 0xbf068008u, 0xbf850006u, 0xf4040385u, 0xfa000000u, 0xbf8cc07fu, 0xf4000307u, 0xfa000000u, 0xbf8cc07fu, 0x7e02020cu, 0x34040082u,
        0xe0701000u, 0x80010102u, 0xbf810000u};
    const std::vector<std::uint32_t> descriptorCode{0xf4040280u, 0xfa000018u, 0xbf8cc07fu, 0xf4080105u, 0xfa000000u, 0xbf8cc07fu, 0x34040082u, 0xe0701000u,
        0x80010002u, 0xbf810000u};
    const auto slotPc = [](const IrResourcePlan& plan, std::uint32_t slot) { return plan.srtReads.at(slot).value->Flags<MemoryFlags>().pc; };
    const auto faultBinding = [](const RecompileResult& result) {
        return std::any_of(result.bindings.begin(), result.bindings.end(), [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::FaultBuffer; });
    };
    const auto samePipeline = [](const RecompileResult& left, const RecompileResult& right) {
        return left.variantId == right.variantId && left.specializationId == right.specializationId && left.PipelineVariantId() == right.PipelineVariantId() && left.spirv == right.spirv;
    };

    NestedRequest branch(branchCode, root.data());
    point(payloadAddress);
    AgcDriver::ShaderMemory validMemory({});
    const auto valid = validMemory.Capture(branch.request);
    require(valid->snapshot.srtPoison.empty() && valid->plan->guardedSrtSlots.size() == 1u && slotPc(*valid->plan, valid->plan->guardedSrtSlots[0]) == 0x2cu && GuardRecords(*valid), "guarded pointer: a mapped nested pointer was poisoned, or its read has no guard");
    require(valid->snapshot.flattenedSrt.at(valid->plan->guardedSrtSlots[0]) == payload, "guarded pointer: a mapped nested read does not hold its word");
    branch.request.context.memory = validMemory.Regions();
    const auto validResult = Recompile(branch.request, *valid);
    require(validResult->poisonedSrtReads == 0u && faultBinding(*validResult) && validResult->bdaAbiVersion == BdaAbi::Version, "guarded pointer: a guarded program has no fault buffer");

    for (const auto pointer : {std::uint64_t{0}, unmapped}) {
        for (const auto condition : {1u, 0u}) {
            point(pointer);
            root[4] = condition;
            branch.request.context.memory = {};
            AgcDriver::ShaderMemory memory({});
            const auto capture = memory.Capture(branch.request);
            const auto& poison = capture->snapshot.srtPoison;
            require(poison.size() == 1u && poison[0].pc == 0x2cu && poison[0].address == pointer && slotPc(*capture->plan, poison[0].slot) == 0x2cu, "guarded pointer: the nested read was not poisoned at its pc and address");
            require(GuardRecords(*capture), "guarded pointer: the poison record is not where the guard reads it");
            const auto regions = memory.Regions();
            require(!RegionsTouch(regions, pointer, 4u) && RegionsCover(regions, root.data() + 6, 8u), "guarded pointer: the captured regions are wrong");
            branch.request.context.memory = regions;
            const auto result = Recompile(branch.request, *capture);
            require(result->poisonedSrtReads == 1u && faultBinding(*result) && result->bdaAbiVersion == BdaAbi::Version, "guarded pointer: the poisoned capture does not report through the fault buffer");
            require(samePipeline(*result, *validResult), "guarded pointer: poison changed the variant, the specialization or the module");
            const auto replay = Recompile(branch.request);
            require(replay.poisonedSrtReads == 1u && samePipeline(replay, *result), "guarded pointer: a replay of the captured regions did not reproduce the poison");
            verifyResult(*result, replay);
#if ANYPS5_ENABLE_SPIRV_TOOLS
            static_cast<void>(ValidateAndOptimizeSpirv(result->spirv, branch.request.target.vulkanVersion, branch.request.target.spirvVersion));
#endif
        }
    }
    root[4] = 1u;

    NestedRequest chain(chainCode, root.data());
    point(unmapped);
    AgcDriver::ShaderMemory chainMemory({});
    const auto chained = chainMemory.Capture(chain.request);
    const auto& chainPoison = chained->snapshot.srtPoison;
    require(chainPoison.size() == 3u && GuardRecords(*chained), "guarded pointer: a pointer read through an unmapped pointer did not poison its two words and the read through it");
    for (const auto& poison : chainPoison) {
        const auto own = slotPc(*chained->plan, poison.slot);
        const bool pointerWord = own == 0x2cu && (poison.address == unmapped || poison.address == unmapped + 4u);
        const bool throughPointer = own == 0x38u && poison.address == unmapped;
        require(poison.pc == 0x2cu && (pointerWord || throughPointer), "guarded pointer: a derived poison does not name the inaccessible read");
    }
    chain.request.context.memory = chainMemory.Regions();
    const auto chainResult = Recompile(chain.request, *chained);
    indirect = 0;
    point(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&indirect)));
    AgcDriver::ShaderMemory nullChainMemory({});
    const auto nullChain = nullChainMemory.Capture(chain.request);
    require(nullChain->snapshot.srtPoison.size() == 1u && nullChain->snapshot.srtPoison[0].pc == 0x38u && nullChain->snapshot.srtPoison[0].address == 0u && GuardRecords(*nullChain), "guarded pointer: a null pointer read from memory did not poison the read through it");
    chain.request.context.memory = nullChainMemory.Regions();
    const auto nullChainResult = Recompile(chain.request, *nullChain);
    indirect = payloadAddress;
    AgcDriver::ShaderMemory mappedChainMemory({});
    const auto mappedChain = mappedChainMemory.Capture(chain.request);
    require(mappedChain->snapshot.srtPoison.empty() && mappedChain->plan->guardedSrtSlots.size() == 3u && GuardRecords(*mappedChain), "guarded pointer: a mapped pointer chain was poisoned, or its reads have no guards");
    chain.request.context.memory = mappedChainMemory.Regions();
    const auto mappedChainResult = Recompile(chain.request, *mappedChain);
    require(chainResult->poisonedSrtReads == 3u && nullChainResult->poisonedSrtReads == 1u && mappedChainResult->poisonedSrtReads == 0u, "guarded pointer: a chain result does not count its poisoned reads");
    require(samePipeline(*chainResult, *mappedChainResult) && samePipeline(*nullChainResult, *mappedChainResult), "guarded pointer: a chain's poison changed the variant, the specialization or the module");

    const auto poisonedWords = [](const ResourceCapture& capture, std::uint32_t pc, std::uint64_t base) {
        std::vector<std::uint64_t> addresses;
        for (const auto& entry : capture.snapshot.srtPoison) {
            if (entry.pc != pc || capture.snapshot.flattenedSrt.at(entry.slot) != 0u) return false;
            addresses.push_back(entry.address);
        }
        std::sort(addresses.begin(), addresses.end());
        return addresses == std::vector<std::uint64_t>{base, base + 4u, base + 8u, base + 12u} && GuardRecords(capture);
    };
    const auto zeroBuffer = [](const ResourceCapture& capture) {
        return capture.snapshot.buffers.size() == 1u && std::all_of(capture.snapshot.buffers[0].dwords.begin(), capture.snapshot.buffers[0].dwords.end(), [](std::uint32_t word) { return word == 0u; });
    };
    alignas(256) static std::array<std::uint32_t, 4> zeroTable{};
    NestedRequest descriptor(descriptorCode, root.data());
    point(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(zeroTable.data())));
    AgcDriver::ShaderMemory zeroMemory({});
    const auto zeroCapture = zeroMemory.Capture(descriptor.request);
    require(zeroCapture->snapshot.srtPoison.empty() && zeroBuffer(*zeroCapture) && GuardRecords(*zeroCapture), "guarded pointer: a zero V# read from mapped memory was poisoned or not read");
    descriptor.request.context.memory = zeroMemory.Regions();
    const auto zeroResult = Recompile(descriptor.request, *zeroCapture);
    for (const auto pointer : {std::uint64_t{0}, unmapped}) {
        point(pointer);
        descriptor.request.context.memory = {};
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(descriptor.request);
        require(poisonedWords(*capture, 0xcu, pointer), "guarded pointer: a V# loaded through a null or unmapped nested pointer did not poison its four words at its pc");
        require(zeroBuffer(*capture), "guarded pointer: a V# loaded through an inaccessible pointer is not bound as zero words");
        const auto regions = memory.Regions();
        descriptor.request.context.memory = regions;
        const auto result = Recompile(descriptor.request, *capture);
        require(result->poisonedSrtReads == 4u && faultBinding(*result) && result->bdaAbiVersion == BdaAbi::Version, "guarded pointer: a capture with a poisoned V# does not report through the fault buffer");
        require(samePipeline(*result, *zeroResult), "guarded pointer: a V# zeroed by poison and a V# read as zeros take different variants, specializations or modules");
        const auto replay = Recompile(descriptor.request);
        require(replay.poisonedSrtReads == 4u && samePipeline(replay, *result), "guarded pointer: a replay did not reproduce the poisoned V#");
    }

    alignas(256) static std::array<std::uint32_t, 8> table{};
    alignas(256) static std::array<std::uint32_t, 4> vertexRoot{};
    table = {0u, 0u, 0u, 0u, static_cast<std::uint32_t>(payloadAddress), static_cast<std::uint32_t>((payloadAddress >> 32u) & 0xffffu), 4u, 0x30027facu};
    const std::vector<std::uint32_t> vertexDescriptorCode{0xf4000084u, 0xfa000000u, 0xf4040104u, 0xfa000008u, 0x7e020280u, 0xbf8cc07fu, 0xbf068002u, 0xbf850006u,
        0xf4080302u, 0xfa000010u, 0xbf8cc07fu, 0xe0300000u, 0x80030100u, 0xbf8c3f70u, 0xf80008cfu, 0x01010101u, 0xbf810000u};
    NestedRequest vertex(vertexDescriptorCode, vertexRoot.data());
    vertex.request.shader.stage = ShaderStage::Vertex;
    vertex.request.context.compute.reset();
    vertex.request.context.vertex = ShaderVertexStageInfo{};
    vertex.request.context.userDataBaseRegister = 8;
    const auto tableAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(table.data()));
    vertexRoot[2] = static_cast<std::uint32_t>(tableAddress);
    vertexRoot[3] = static_cast<std::uint32_t>(tableAddress >> 32u);
    vertexRoot[0] = 1u;
    AgcDriver::ShaderMemory mappedVertexMemory({});
    const auto mappedVertex = mappedVertexMemory.Capture(vertex.request);
    require(mappedVertex->snapshot.srtPoison.empty() && mappedVertex->snapshot.buffers.size() == 1u && mappedVertex->snapshot.buffers[0].dwords[0] == table[4], "guarded pointer: a vertex V# behind a mapped pointer was poisoned or not read");
    vertexRoot[2] = 0u;
    vertexRoot[3] = 0u;
    for (const auto guard : {0u, 1u}) {
        vertexRoot[0] = guard;
        vertex.request.context.memory = {};
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(vertex.request);
        require(poisonedWords(*capture, 0x20u, 0x10u), "guarded pointer: a vertex V# behind a branch and a null pointer did not poison its four words at its pc");
        const auto regions = memory.Regions();
        vertex.request.context.memory = regions;
        const auto result = Recompile(vertex.request, *capture);
        require(result->poisonedSrtReads == 4u && faultBinding(*result) && !result->spirv.empty(), "guarded pointer: a vertex capture with a poisoned V# has no fault buffer");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        static_cast<void>(ValidateAndOptimizeSpirv(result->spirv, vertex.request.target.vulkanVersion, vertex.request.target.spirvVersion));
#endif
    }
    NestedRequest rootless(branchCode, nullptr);
    AgcDriver::ShaderMemory rootlessMemory({});
    const auto rootlessCapture = rootlessMemory.Capture(rootless.request);
    require(rootlessCapture->snapshot.nullRootReads != 0u && GuardRecords(*rootlessCapture), "guarded pointer: reads through a null user-data pointer were not counted");
    require(std::all_of(rootlessCapture->snapshot.buffers.begin(), rootlessCapture->snapshot.buffers.end(), [](const DescriptorValue& value) { return std::all_of(value.dwords.begin(), value.dwords.end(), [](std::uint32_t word) { return word == 0u; }); }), "guarded pointer: a V# read through a null user-data pointer is not zero");
    rootless.request.context.memory = rootlessMemory.Regions();
    const auto rootlessResult = Recompile(rootless.request, *rootlessCapture);
    require(rootlessResult->poisonedSrtReads >= rootlessCapture->snapshot.nullRootReads, "guarded pointer: a capture through a null user-data pointer is cacheable");
    point(0u);
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, Reserved);
#endif
}

void verifyQuadMaskNeighbourReads() {
    using namespace ShaderRecompiler;
    constexpr std::array<std::uint32_t, 6> capabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic};
    constexpr std::array<std::uint32_t, 7> demoteCapabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic, spv::CapabilityDemoteToHelperInvocation};
    constexpr std::array<std::string_view, 1> demoteExtensions{"SPV_EXT_demote_to_helper_invocation"};
    constexpr std::array<std::uint32_t, 4> constant{0x7e0202f2u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 6> explicitExec{0xbefe04c1u, 0x7e000500u, 0x7e020200u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 6> constantWqm{0xbefe0ac1u, 0x7e000500u, 0x7e020200u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 7> permute{0xbefe04c1u, 0x7e040284u, 0xdac80000u, 0x01000002u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 9> comparisonBallot{0xbe8003ffu, 0x3f000000u, 0xbefe04c1u, 0x7d840000u, 0xbe80046au, 0x7e020200u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    const auto compile = [&](std::span<const std::uint32_t> code, bool masked, bool demote) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x58000u, code, 0, {}};
        request.context.waveSize = 32u;
        request.context.pixel = ShaderPixelStageInfo{};
        request.context.pixel->inputAddr = PixelInputBit(PixelInput::PositionX);
        request.context.pixel->posX = true;
        request.context.pixel->quadPixelMask = masked ? 0x2u : 0xfu;
        request.context.pixel->targetOutputMode[0] = 9u;
        request.context.pixel->targetExportMapping.fill(0xe4u);
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32u;
        request.target.supportedCapabilities = demote ? std::span<const std::uint32_t>(demoteCapabilities) : std::span<const std::uint32_t>(capabilities);
        if (demote) request.target.supportedExtensions = demoteExtensions;
        request.layout.pushConstantSizeBytes = 128u;
        request.useCache = false;
        return Recompile(request);
    };
    const auto declares = [](const RecompileResult& result, spv::Op opcode) {
        const auto& words = result.spirv.Words();
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "quad mask: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == static_cast<std::uint32_t>(opcode)) return true;
            cursor += count;
        }
        return false;
    };
    require(declares(compile(constant, true, false), spv::OpKill), "a quad mask without neighbour reads did not retain its kill fallback");
    for (const auto code : {std::span<const std::uint32_t>(explicitExec), std::span<const std::uint32_t>(constantWqm), std::span<const std::uint32_t>(permute)}) {
        const auto unmasked = compile(code, false, false);
        require(declares(unmasked, spv::OpGroupNonUniformShuffle), "the neighbour-read test did not retain its cross-lane shuffle");
        expectFailure([&] { static_cast<void>(compile(code, true, false)); }, "shaderDemoteToHelperInvocation", "a quad mask killed helper lanes before a cross-lane read with EXEC enabled for those lanes");
        const auto masked = compile(code, true, true);
        require(declares(masked, spv::OpDemoteToHelperInvocation) && declares(masked, spv::OpGroupNonUniformShuffle), "a quad mask with demote did not preserve its cross-lane shuffle path");
    }
    require(declares(compile(comparisonBallot, false, false), spv::OpGroupNonUniformBallot), "the comparison test did not retain its subgroup ballot");
    expectFailure([&] { static_cast<void>(compile(comparisonBallot, true, false)); }, "shaderDemoteToHelperInvocation", "a quad mask killed helper lanes before a ballot with EXEC enabled for those lanes");
    const auto maskedBallot = compile(comparisonBallot, true, true);
    require(declares(maskedBallot, spv::OpDemoteToHelperInvocation) && declares(maskedBallot, spv::OpGroupNonUniformBallot), "a quad mask with demote did not preserve its ballot path");
}

void verifyCubeDepthCompareMode() {
    using namespace ShaderRecompiler;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2DArray;
    image.depthCompare = true;
    const DescriptorValue descriptor{{0x02143930u, 0x08200000u, 0u, 0xb0500facu, 5u, 0x00700000u, 0u, 0u}, 8u};
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const auto selected = ResourceMaterializer::RuntimeImageMode(image, descriptor, modes);
    require(modes[selected].cube && (modes[selected].emulatedCompare & EmulatedCompare::Enabled) != 0u, "cube color comparison did not select its emulated runtime mode");
}

void verifyVolumeArrayLoadMode() {
    using namespace ShaderRecompiler;
    const DescriptorValue descriptor{{0x02466600u, 0x01400000u, 0u, 0xa0500204u, 1u, 0x00700000u, 0u, 0u}, 8u};
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2DArray;
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const auto selected = ResourceMaterializer::RuntimeImageMode(image, descriptor, modes);
    require(modes[selected].dimension == RdnaImageDimension::Dim3D && !modes[selected].cube, "2D-array image load on a 3D T# did not select a volume runtime mode");
    image.fmaskCompatible = false;
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, ResourceMaterializer::RuntimeImageModes(image))); }, "incompatible with the static runtime image interface", "2D-array image sample on a 3D T# was accepted");
    ImageResource store{};
    store.resourceClass = ImageResourceClass::Storage;
    store.dimension = RdnaImageDimension::Dim2DArray;
    store.written = true;
    store.fmaskCompatible = false;
    const auto storeModes = ResourceMaterializer::RuntimeImageModes(store);
    require(storeModes[ResourceMaterializer::RuntimeImageMode(store, descriptor, storeModes)].dimension == RdnaImageDimension::Dim3D, "2D-array image store on a 3D T# did not select a volume runtime mode");
    store.atomic = true;
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(store, descriptor, ResourceMaterializer::RuntimeImageModes(store))); }, "incompatible with the static runtime image interface", "2D-array image atomic on a 3D T# was accepted");
}

void verifySrgbDepthCompareModes() {
    using namespace ShaderRecompiler;
    for (const auto decodeFormats : {0u, 1u, 2u, 3u}) {
        ImageResource image{};
        image.resourceClass = ImageResourceClass::Sampled;
        image.dimension = RdnaImageDimension::Dim2D;
        image.depthCompare = true;
        image.srgbDecodeCompatible = false;
        image.srgbDecodeFormats = decodeFormats;
        const auto modes = ResourceMaterializer::RuntimeImageModes(image);
        for (const auto format : {128u, 129u, 130u}) {
            const DescriptorValue descriptor{{0x100u, format << 20u, 0u, 0x90000facu, 0u, 0u, 0u, 0u}, 8u};
            const auto selected = ResourceMaterializer::RuntimeImageMode(image, descriptor, modes);
            require(!modes[selected].srgbDecode && (modes[selected].emulatedCompare & EmulatedCompare::Enabled) != 0u, "sRGB color comparison did not select its emulated runtime mode independently of decode support");
        }
    }
}

int main(int argc, char** argv) {
    try {
        using namespace ShaderRecompiler;
        if (argc == 2 && std::string_view(argv[1]) == "--quad-mask-no-demote") {
            verifyQuadMaskNeighbourReads();
            std::cout << "Quad masks preserve neighbour-lane helpers or refuse without demote\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--frontend-pair") {
            verifyFrontendPair();
            std::cout << "Prepared frontend reuse preserves full and partial-thread programs\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--layout-pair") {
            verifyLayoutPair();
            std::cout << "Both push-constant layouts of a pixel shader compile from one program unchanged\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--bindless") {
            verifyImageTableKeys();
            verifyImageTables();
            verifyImageTableBulkReads();
            verifyImageTableSrtKey();
            verifyImageTableMaskedOffset();
            verifyImageTableKeyUnion();
            verifyImageTableLateKey();
            verifyImageTableUnkeyedAccess();
            verifyImageTableRegistration();
            verifyScalarAddressRule();
            verifyPointerImageTable();
            verifyPointerTableOffsetCarry();
            std::cout << "Image table keys, maps, records and registration passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--srt-read-patching") {
            verifySrtReadPatching();
            std::cout << "SRT read patching replaces repeated and multiple uses\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--type-zero-image-materialization") {
            verifyTypeZeroImageMaterialization();
            std::cout << "TYPE=0 image descriptor materialization passed\n";
            return 0;
        }
        require(argc == 1, "unknown shader memory test arguments");
        verifyRegisterSources();
        verifyEvaluatedValues();
        verifyFrontendPair();
        verifyLayoutPair();
        verifySignedSrtComparison();
        verifyTypeZeroImageMaterialization();
        verifyOutOfRangeConstantReads();
        verifySignedMinimum();
        verifyMayEvaluate();
        verifyPureFlatSlots();
        verifyGuardedDescriptorSources();
        verifyImageTableKeys();
        verifyImageTables();
        verifyImageTableRegistration();
        verifyScalarAddressRule();
        verifyPointerImageTable();
        verifyPointerTableOffsetCarry();
        verifyDescriptorPhis();
        verifyUniformTableImage();
        verifyLaneSelectedDescriptors();
        verifyProgramCounterRelativeData();
        verifyLanesOutsideHostSubgroup();
        verifyHalfWaveReduction();
        verifyNarrowVertexWave();
        verifySingleLaneVertexWave();
        verifyMeshConfiguration();
        verifyPixelInputs();
        verifyCanonicalVersion20And21Requests();
        verifyPixelRequestSerialization();
        verifyVertexInfoVersion19();
        verifyPositionExportComponents();
        verifyLegacyPixelRequests();
        verifyPixelExportReplay();
        verifyPixelParameterSlots();
        verifyF16PixelParameterSlots();
        verifyComputedTexelOffsets();
        verifyShaderClockScopes();
        verifyInt64AtomicCapabilities();
        verifySpecializedBufferAtomicBindings();
        verifyCoherentBufferAccesses();
        verifyUnnormalizedSamplers();
        verifyUnusedUnnormalizedSampler();
        verifyWaveUniformValues();
        verifyTwoLaneUniformValues();
        verifyBdaReadFallbackFunctions();
        verifyFunctionLdsBound();
        verifyFunctionLdsUpdates();
        verifySrtReadPatching();
        verifyGuardedNullPointers();
        verifyCubeDepthCompareMode();
        verifyVolumeArrayLoadMode();
        verifySrgbDepthCompareModes();
        verifyQuadMaskNeighbourReads();
        verifyCoherentGds();
#if ANYPS5_ENABLE_SPIRV_TOOLS
        const std::vector<std::uint32_t> minimalSpirv{
            0x07230203u, 0x00010000u, 0u, 5u, 0u,
            0x00020011u, 1u,
            0x0003000eu, 0u, 1u,
            0x0005000fu, 5u, 3u, 0x6e69616du, 0u,
            0x00060010u, 3u, 17u, 1u, 1u, 1u,
            0x00020013u, 1u,
            0x00030021u, 2u, 1u,
            0x00050036u, 1u, 3u, 0u, 2u,
            0x000200f8u, 4u,
            0x00010000u,
            0x000100fdu,
            0x00010038u
        };
        const auto optimizedSpirv = ValidateAndOptimizeSpirv(minimalSpirv, 0x00401001u, 0x00010000u);
        require(optimizedSpirv.size() < minimalSpirv.size(), "SPIR-V optimization did not remove the no-op");
        require(optimizedSpirv == ValidateAndOptimizeSpirv(minimalSpirv, 0x00401001u, 0x00010000u), "SPIR-V optimization is not deterministic");
#endif
        const std::array<std::uint32_t, 8> code{0xf4040004u, 0xfa000000u, 0xf4000080u, 0xfa000000u, 0x7e000202u, 0xf80008cfu, 0u, 0xbf810000u};
        struct alignas(4096) GuestTables {
            std::uint32_t payload = 0;
            std::uint64_t table = 0;
        };
        static GuestTables guest;
        auto& payload = guest.payload;
        payload = 0x3f800000u;
        auto& table = guest.table;
        table = reinterpret_cast<std::uintptr_t>(&payload);
        const auto address = reinterpret_cast<std::uintptr_t>(&table);
        const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
        request.target = BufferTarget();
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 8;
        request.context.userData = userData;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;

        expectFailure([&] { static_cast<void>(Recompile(request)); }, "SrtWalker::EvaluateRuntimeSources", "missing snapshot unexpectedly read live memory");
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        {
            // The payload slot is consumed by the export alone: pure, its leaf traced at the
            // payload's address; the table pointer's words (the payload's address cone) are among
            // the other reads.
            const auto& pure = capture->plan->pureFlatSlots;
            require(std::count(pure.begin(), pure.end(), std::uint8_t{1}) == 1, "the payload slot is not the one pure flat slot");
            const auto& trace = capture->readTrace;
            require(trace.leaves.size() == 1 && trace.leaves[0].second == reinterpret_cast<std::uintptr_t>(&payload), "the pure slot's leaf was not traced at the payload");
            const auto slot = trace.leaves[0].first;
            require(slot < pure.size() && pure[slot] != 0 && capture->snapshot.flattenedSrt.at(slot) == payload, "the traced leaf is not the pure slot");
            require(std::find(trace.otherReads.begin(), trace.otherReads.end(), address) != trace.otherReads.end(), "the table pointer read was not traced among the other reads");
            require(std::find(trace.otherReads.begin(), trace.otherReads.end(), reinterpret_cast<std::uintptr_t>(&payload)) == trace.otherReads.end(), "the payload counts as a walk read");
        }
        auto regions = memory.Regions();
        std::size_t capturedBytes = 0;
        for (const auto& region : regions) capturedBytes += region.bytes.size();
        require(capturedBytes == sizeof(table) + sizeof(payload), "nested pointer reads were not captured");
        request.context.memory = regions;
        const auto first = Recompile(request);
        require(!first.spirv.empty(), "empty compiled shader");
        require(!first.cacheHit, "first shader compilation unexpectedly hit the cache");
        const auto plan = GetResourcePlan(request);
        require(plan == GetResourcePlan(request), "resource plan was rebuilt");
        const auto cached = Recompile(request);
        require(cached.cacheHit, "unchanged shader did not hit the cache");
        verifyResult(first, cached);
        auto relocated = request;
        relocated.shader.codeAddress += 0x1000;
        require(Recompile(relocated).cacheHit, "shader relocation caused recompilation");
        auto changedTarget = request;
        changedTarget.target.subgroupSize = 32;
        require(GetResourcePlan(changedTarget) != plan, "different target reused the source entry");
        std::vector<std::uint32_t> changedCode(code.begin(), code.end());
        changedCode.insert(changedCode.begin(), 0xbf800000u);
        auto changedSource = request;
        changedSource.shader.code = changedCode;
        require(GetResourcePlan(changedSource) != plan, "changed code reused the source entry");
        auto uncached = request;
        uncached.useCache = false;
        require(GetResourcePlan(uncached) != plan, "disabled cache reused the resource plan");
        const auto fresh = Recompile(uncached);
        require(!fresh.cacheHit, "disabled cache reused the compiled variant");
        verifyResult(first, fresh);
        require(!RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(uncached)).request.useCache, "cache policy was lost in serialization");
        auto offsets = uncached;
        offsets.target.nonConstantImageOffsets = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(offsets)).request.target.nonConstantImageOffsets, "non-constant texel offsets were lost in serialization");
        auto srgb = uncached;
        srgb.target.srgbDecodeFormats = 3u;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(srgb)).request.target.srgbDecodeFormats == 3u, "the sRGB formats decoded in the shader were lost in serialization");
        auto narrowClock = uncached;
        narrowClock.target.narrowSubgroupClock = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(narrowClock)).request.target.narrowSubgroupClock, "the narrow subgroup clock was lost in serialization");
        auto float64Denormals = uncached;
        float64Denormals.target.float64DenormPreserve = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(float64Denormals)).request.target.float64DenormPreserve, "f64 denormal preservation was lost in serialization");
        auto registrationModules = uncached;
        registrationModules.target.registrationModules = true;
        registrationModules.target.registrationWordLimit = 1234u;
        const auto roundTrip = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(registrationModules)).request.target;
        require(roundTrip.registrationModules && roundTrip.registrationWordLimit == 1234u, "registration modules were lost in serialization");
        std::vector<std::uint64_t> plainKey;
        std::vector<std::uint64_t> registrationKey;
        RecompileCacheKey::Build(uncached, plainKey);
        RecompileCacheKey::Build(registrationModules, registrationKey);
        require(plainKey != registrationKey, "registration modules did not change the cache key");
        auto float16Denormals = uncached;
        float16Denormals.target.float16DenormPreserve = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(float16Denormals)).request.target.float16DenormPreserve, "f16 denormal preservation was lost in serialization");
        auto changedLayout = request;
        changedLayout.layout.pushConstantSizeBytes = 64;
        require(!Recompile(changedLayout).cacheHit, "binding layout change reused an incompatible variant");
        require(Recompile(changedLayout).cacheHit, "new binding layout variant was not cached");
        require(Recompile(request).cacheHit, "compiling a new variant evicted the original");
        auto missingMemory = request;
        missingMemory.context.memory = {};
        expectFailure([&] { static_cast<void>(Recompile(missingMemory)); }, "SrtWalker::EvaluateRuntimeSources", "cache hit bypassed resource validation");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        auto invalidSpirv = first.spirv;
        invalidSpirv[0] = 0;
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(invalidSpirv, request.target.vulkanVersion, request.target.spirvVersion)); }, "SPIR-V validation before optimization failed", "invalid SPIR-V passed validation");
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(first.spirv, 0x00400000u, 0x00010600u)); }, "unsupported Vulkan/SPIR-V target", "incompatible target accepted");
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(first.spirv, 0x00405000u, 0x00010600u)); }, "unsupported Vulkan target", "unknown Vulkan target accepted");
#endif
        payload = 0x40000000u;
        AgcDriver::ShaderMemory updatedMemory({});
        const auto updatedCapture = updatedMemory.Capture(request);
        const auto updatedRegions = updatedMemory.Regions();
        auto updated = request;
        updated.context.memory = updatedRegions;
        const auto updatedCached = Recompile(updated);
        require(updatedCached.cacheHit, "dynamic shader data caused recompilation");
        updated.useCache = false;
        verifyResult(updatedCached, Recompile(updated));
        bool changedData = updatedCached.pushConstants != first.pushConstants;
        for (std::size_t i = 0; i < first.bindings.size(); ++i) changedData = changedData || updatedCached.bindings.at(i).guestDescriptor != first.bindings[i].guestDescriptor;
        require(changedData, "cache hit retained stale shader data");
        {
            // Two captures differing only in the payload (the pure slot) recompile to the same
            // variant, with bindings equal apart from that slot's FlattenedSrt word.
            payload = 0x40400000u;
            AgcDriver::ShaderMemory changedMemory({});
            const auto changedCapture = changedMemory.Capture(request);
            auto changed = updated;
            changed.useCache = true;
            changed.context.memory = changedMemory.Regions();
            auto before = updated;
            before.useCache = true;
            before.context.memory = updatedRegions;
            const auto first = Recompile(before, *updatedCapture);
            const auto second = Recompile(changed, *changedCapture);
            require(first->variantId == second->variantId, "a pure slot's value changed the variant");
            require(first->bindings.size() == second->bindings.size(), "a pure slot's value changed the bindings");
            const auto slot = changedCapture->readTrace.leaves.at(0).first;
            for (std::size_t i = 0; i < first->bindings.size(); ++i) {
                const auto& left = first->bindings[i];
                const auto& right = second->bindings[i];
                if (left.role != DescriptorRole::FlattenedSrt) {
                    require(left.guestDescriptor == right.guestDescriptor, "a pure slot's value changed a non-data binding");
                    continue;
                }
                require(left.guestDescriptor.size() == right.guestDescriptor.size() && left.guestDescriptor.at(slot) == 0x40000000u && right.guestDescriptor.at(slot) == 0x40400000u, "the FlattenedSrt binding does not carry the pure slot's value");
                for (std::size_t j = 0; j < left.guestDescriptor.size(); ++j) {
                    if (j != slot) require(left.guestDescriptor[j] == right.guestDescriptor[j], "the FlattenedSrt binding differs beyond the pure slot");
                }
            }
            payload = 0x40000000u;
        }
        auto concurrent = request;
        concurrent.layout.pushConstantSizeBytes = 60;
        std::array<std::future<RecompileResult>, 4> concurrentResults;
        for (auto& future : concurrentResults) future = std::async(std::launch::async, [concurrent] { return Recompile(concurrent); });
        std::uint32_t compilations = 0;
        for (auto& future : concurrentResults) {
            const auto result = future.get();
            if (!result.cacheHit) ++compilations;
        }
        require(compilations == 1, "concurrent requests compiled the same variant repeatedly");
        const auto serialized = RequestSerializer{}.Serialize(request);
        table = 0;
        payload = 0xdeadbeefu;
        verifyResult(first, Recompile(request));
        auto replay = RequestSerializer{}.Deserialize(serialized);
        verifyResult(first, Recompile(replay.request));
        RequestMemoryView view(replay.request.context.memory);
        const auto runtime = view.MakeRuntime(userData, request.shader.codeAddress);
        std::uint32_t captured = 0;
        require(runtime.readMemory(runtime.userContext, reinterpret_cast<std::uintptr_t>(&payload), &captured) && captured == 0x3f800000u, "snapshot changed with live memory");

        request.context.userDataBaseRegister = 0x8c;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "shader user data exceeds the scalar register bank", "PM4 register address accepted as SGPR base");
        request.context.userDataBaseRegister = 105;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "shader user data exceeds the scalar register bank", "user data overran scalar register bank");
        request.context.userDataBaseRegister = 8;
        request.context.memory = {};
        AgcDriver::ShaderMemory guarded({});
        const auto poisoned = guarded.Capture(request);
        require(poisoned->snapshot.srtPoison.size() == 1u && poisoned->snapshot.srtPoison[0].pc == 8u && poisoned->snapshot.srtPoison[0].address == 0u && GuardRecords(*poisoned), "null nested pointer was not poisoned at its read");
        const auto guardedRegions = guarded.Regions();
        request.context.memory = guardedRegions;
        const auto poisonedVertex = Recompile(request, *poisoned);
        require(poisonedVertex->poisonedSrtReads == 1u && poisonedVertex->bdaAbiVersion == BdaAbi::Version && poisonedVertex->PipelineVariantId() == first.PipelineVariantId() && std::any_of(poisonedVertex->bindings.begin(), poisonedVertex->bindings.end(), [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::FaultBuffer; }), "a vertex shader reading through a null nested pointer has no fault buffer or another variant");
        request.context.memory = {};
        const std::array<std::uint32_t, 2> nullUserData{};
        request.context.userData = nullUserData;
        AgcDriver::ShaderMemory nullRoot({});
        const auto nullRootCapture = nullRoot.Capture(request);
        require(nullRootCapture->snapshot.nullRootReads != 0u, "reads through a null user-data pointer were not counted");
        request.context.memory = nullRoot.Regions();
        require(Recompile(request, *nullRootCapture)->poisonedSrtReads >= nullRootCapture->snapshot.nullRootReads, "a capture through a null user-data pointer is cacheable");
        std::cout << "Shader memory capture, strict validation and deterministic replay passed\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
