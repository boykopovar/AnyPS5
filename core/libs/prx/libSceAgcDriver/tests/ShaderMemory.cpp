#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include "Optimization/SrtWalker/SrtTape.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "Optimization/SrtWalker.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "SavedExecMaskLoop.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "CacheKey.hpp"
#include "CompiledVariant.hpp"
#include "BdaAbi.hpp"
#include "ImageTableAbi.hpp"
#include "ShaderDiskCache.hpp"
#include <limits>
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <set>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <future>
#include <memory>
#include <new>
#include <stdexcept>
#include <span>
#include <atomic>
#include <functional>
#include <new>
#include <chrono>
#include <thread>
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

thread_local std::uint64_t allocationCount = 0;

}

void* operator new(std::size_t bytes) {
    allocationCount++;
    if (auto* allocation = std::malloc(bytes == 0 ? 1 : bytes)) return allocation;
    throw std::bad_alloc();
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(bytes);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* allocation) noexcept {
    std::free(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept {
    std::free(allocation);
}

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
    changed.context.waveSize = 32u;
    changed.context.compute->numThreads = {32u, 2u, 1u};
    verifyMiss(changed, "frontend pair: a different wave size reused the frontend");
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

void verifyFrontendWaveSizeCalls() {
    using namespace ShaderRecompiler;
    const std::vector<std::uint32_t> code{
        0xbb0e0005u,
        0xd4c2000eu, 0x00020300u,
        0xd5010002u, 0x003a0300u,
        0xbf820001u,
        0xbe80200eu,
        0xbf810000u};
    const std::array<std::uint32_t, 2> userData{};
    for (const std::uint32_t waveSize : {32u, 64u}) {
        auto request = FrontendRequest(code, userData);
        request.context.waveSize = waveSize;
        request.context.compute->numThreads = {waveSize, 1u, 1u};
        ShaderPreparationContext preparation;
        require(preparation.AcquireFrontend(request) != nullptr, "frontend wave size: a mask write that reuses the link pair and its v_cndmask read were refused");
    }
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
    verifyFrontendWaveSizeCalls();
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
        require(preparation.AcquireFrontend(request) != retained, "frontend pair: a variant with another wave size reused the frontend");
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

void verifySpecializedModuleRelease() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 1> color{0x3f800000u};
    const std::vector<std::uint32_t> code{0xbf800003u, 0x7e000200u, 0xf800180fu, 0u, 0xbf810000u};
    auto pixel = FrontendRequest(code, color);
    pixel.shader.stage = ShaderStage::Fragment;
    pixel.context.compute.reset();
    pixel.context.pixel = ShaderPixelStageInfo{};
    pixel.context.pixel->wave32 = true;
    pixel.context.pixel->targetOutputMode[0] = 9u;
    pixel.context.waveSize = 32u;
    pixel.useCache = true;
    SharedSpirv::Weak released;
    std::uint64_t specializationId = 0;
    std::vector<std::uint32_t> words;
    {
        const auto first = Recompile(pixel);
        require(first.specializationId != 0u, "specialized module release: the pixel fixture was not specialized");
        released = first.spirv.Weaken();
        specializationId = first.specializationId;
        words = first.spirv.Copy();
    }
    require(released.Expired(), "specialized module release: the module caches kept the words of a module no result uses");
    const auto again = Recompile(pixel);
    require(again.specializationId == specializationId, "specialized module release: a module built again from its key got another specialization ID");
    require(std::ranges::equal(again.spirv.Words(), words), "specialized module release: a module built again from its key has other words");
    const auto shared = Recompile(pixel);
    require(shared.spirv.data() == again.spirv.data(), "specialized module release: a second result of a live module copied its words");
}

const std::vector<std::uint32_t>& resultMemoPixelCode() {
    static const std::vector<std::uint32_t> code{0xbf800000u, 0x7e000200u, 0xf800180fu, 0u, 0xbf810000u};
    return code;
}

bool compileResultMemoPixel(std::uint32_t value) {
    using namespace ShaderRecompiler;
    std::array<std::uint32_t, 1> color{value};
    auto request = FrontendRequest(resultMemoPixelCode(), color);
    request.shader.stage = ShaderStage::Fragment;
    request.context.compute.reset();
    request.context.pixel = ShaderPixelStageInfo{};
    request.context.pixel->wave32 = true;
    request.context.pixel->targetOutputMode[0] = 9u;
    request.context.waveSize = 32u;
    request.useCache = true;
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto regions = memory.Regions();
    request.context.memory = regions;
    bool hit = false;
    static_cast<void>(Recompile(request, *capture, &hit));
    return hit;
}

void verifyResultMemoBudgetThreads() {
    using namespace ShaderRecompiler;
    require(!compileResultMemoPixel(1u) && compileResultMemoPixel(1u), "result memo budget threads: the memo did not keep a result");
    const auto one = ResultMemoBytes();
    const auto budget = 3u * one;
    SetResultMemoBudget(budget);
    static_cast<void>(compileResultMemoPixel(2u));
    static_cast<void>(compileResultMemoPixel(3u));
    require(ResultMemoBytes() == budget, "result memo budget threads: three results did not fill a budget of three");
    std::vector<std::thread> late;
    SetResultMemoTrimHook([&] {
        const auto before = ResultMemoBytes();
        for (const auto value : {5u, 6u}) late.emplace_back([value] { static_cast<void>(compileResultMemoPixel(value)); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (ResultMemoBytes() < before + 2u * one && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    });
    static_cast<void>(compileResultMemoPixel(4u));
    for (auto& thread : late) thread.join();
    require(late.size() == 2u, "result memo budget threads: the fourth result did not start a trim");
    require(ResultMemoBytes() <= budget, "result memo budget threads: results inserted during a trim left the memo over its budget");
    SetResultMemoBudget(0u);
}

void verifyResultMemoBudgetMasked() {
    using namespace ShaderRecompiler;
    require(ResultMemoBytes() == 0u, "result memo budget masked: the memo held results before the first compile");
    require(!compileResultMemoPixel(1u), "result memo budget masked: the first compile hit the memo");
    const auto first = ResultMemoBytes();
    require(!compileResultMemoPixel(2u), "result memo budget masked: a new snapshot hit the memo");
    const auto second = ResultMemoBytes() - first;
    require(second != 0u && second < first, std::string("result memo budget masked: the masked table's entry was not counted (first result " + std::to_string(first) + " bytes, patched result " + std::to_string(second) + ")").c_str());
    SetResultMemoBudget(1u);
    require(ResultMemoBytes() == 0u, std::string("result memo budget masked: a trim to one byte left " + std::to_string(ResultMemoBytes()) + " bytes").c_str());
    SetResultMemoBudget(0u);
    require(!compileResultMemoPixel(3u), "result memo budget masked: a snapshot after the trim hit the memo");
    require(ResultMemoBytes() == first, std::string("result memo budget masked: the trim left a masked entry to patch from (" + std::to_string(ResultMemoBytes()) + " bytes after one compile, " + std::to_string(first) + " for a full one)").c_str());
}

void verifyResultMemoBudget() {
    using namespace ShaderRecompiler;
    const auto compile = [](std::uint32_t value) { return compileResultMemoPixel(value); };
    require(ResultMemoBytes() == 0u, "result memo budget: the memo held results before the first compile");
    require(!compile(1u) && compile(1u), "result memo budget: the memo did not keep a result");
    const auto one = ResultMemoBytes();
    require(one != 0u, "result memo budget: a memo'd result counted no bytes");
    const auto budget = 3u * one;
    SetResultMemoBudget(budget);
    for (const auto value : {2u, 3u, 4u}) {
        require(!compile(value), "result memo budget: a new snapshot hit the memo");
        require(ResultMemoBytes() <= budget, "result memo budget: the memo holds more than its budget");
    }
    require(ResultMemoBytes() < budget, "result memo budget: a fourth result over a budget of three was not trimmed");
    require(compile(4u) && compile(3u), "result memo budget: the most recently used results were dropped");
    require(!compile(1u), "result memo budget: the least recently used result outlived the trim");
    SetResultMemoBudget(0u);
    for (const auto value : {5u, 6u, 7u}) static_cast<void>(compile(value));
    require(ResultMemoBytes() > budget, "result memo budget: clearing the budget kept bounding the memo");
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

void verifyPlanConditionPruning() {
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
    const auto userData = [&](std::uint32_t index) -> IrValue& {
        auto& reg = make(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister({RegisterBank::Scalar, index});
        auto& read = make(IrOpcode::GetUserData, IrType::U32);
        read.AddArgument(&reg);
        return read;
    };
    const auto equal = [&](IrValue& left, IrValue& right) -> IrValue& {
        auto& compare = make(IrOpcode::IEqual32, IrType::Bool);
        compare.AddArgument(&left);
        compare.AddArgument(&right);
        return compare;
    };
    constexpr std::uint32_t Additions = 64;
    IrValue* address = &userData(0);
    for (std::uint32_t index = 0; index < Additions; ++index) {
        auto& sum = make(IrOpcode::IAdd32, IrType::U32);
        sum.AddArgument(address);
        sum.AddArgument(&constant(4));
        address = &sum;
    }
    auto& load = make(IrOpcode::LoadBufferU32, IrType::U32);
    load.AddArgument(address);
    auto& pair = make(IrOpcode::CompositeConstructU32x2, IrType::U32x2);
    pair.AddArgument(&userData(1));
    pair.AddArgument(&load);
    auto& low = make(IrOpcode::CompositeExtractU32x2, IrType::U32);
    low.AddArgument(&pair);
    low.AddArgument(&constant(0));
    static std::uint32_t word = 5;
    const auto wordAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&word));
    auto& handle = make(IrOpcode::CompositeConstructU64, IrType::U64);
    handle.AddArgument(&userData(2));
    handle.AddArgument(&userData(3));
    auto& read = make(IrOpcode::LoadAddressU32, IrType::U32);
    read.SetFlags(MemoryFlags{0, 0});
    read.AddArgument(&handle);
    read.AddArgument(&constant(0));
    IrProgram program;
    auto& resources = program.Resources();
    resources.srtPlanComplete = true;
    resources.resourceTrackingComplete = true;
    MemoryInfo memory;
    memory.kind = ResourceKind::ScalarAddress;
    resources.memoryInfo.push_back(memory);
    resources.controlFlow.push_back({&equal(load, constant(0)), {1, 2}, {}, {}});
    resources.controlFlow.push_back({&equal(low, constant(7)), {3, 4}, {}, {}});
    resources.controlFlow.push_back({&equal(read, constant(5)), {5, 6}, {}, {}});
    auto& choice = make(IrOpcode::SelectU32, IrType::U32);
    choice.AddArgument(&equal(userData(4), constant(0)));
    choice.AddArgument(&load);
    choice.AddArgument(&constant(1));
    resources.controlFlow.push_back({&equal(choice, constant(1)), {7, 8}, {}, {}});
    const auto plan = ResourceMaterializer{}.ExtractPlan(program);
    const auto copied = std::ranges::count_if(plan.valueStorage, [](const IrValue& value) { return value.Opcode() == IrOpcode::IAdd32; });
    require(copied == 0, "plan pruning: the address computation under a load the evaluator cannot read was copied");
    require(plan.controlFlow[0].condition == nullptr && plan.controlFlow[3].condition == nullptr, "plan pruning: a condition the evaluator can never read was kept");
    require(plan.controlFlow[1].condition != nullptr && plan.controlFlow[2].condition != nullptr, "plan pruning: a condition the evaluator can read was dropped");
    const auto copiedLoad = std::ranges::find_if(plan.valueStorage, [](const IrValue& value) { return value.Opcode() == IrOpcode::LoadBufferU32; });
    require(copiedLoad != plan.valueStorage.end() && copiedLoad->ArgumentCount() == 0u, "plan pruning: the unevaluable load was not kept as a leaf");
    require(std::ranges::all_of(plan.valueStorage, [](const IrValue& value) { return !value.HasUses() && value.OperandUses().empty() && value.Arguments().capacity() == std::max(value.ArgumentCount(), IrOperands::InlineCapacity); }), "plan compaction: a plan value keeps use lists or spare heap operand capacity");
    const std::array<std::uint32_t, 4> data{0x100u, 7u, static_cast<std::uint32_t>(wordAddress), static_cast<std::uint32_t>(wordAddress >> 32u)};
    SrtRuntime runtime{data};
    Detail::Evaluator evaluator(plan, runtime);
    std::uint32_t result = 0;
    require(evaluator.Evaluate(Detail::IrNode(plan.controlFlow[1].condition), result) && result == 1u, "plan pruning: a condition read through an extracted pair was not evaluated");
    require(evaluator.Evaluate(Detail::IrNode(plan.controlFlow[2].condition), result) && result == 1u, "plan pruning: a condition on a raw read lost its handle");
}

struct PlanWalk {
    std::vector<ShaderRecompiler::DescriptorValue> results;
    std::vector<std::uint32_t> flat;
    std::vector<std::uint8_t> active;
};

PlanWalk walkPlan(const ShaderRecompiler::IrResourcePlan& plan, std::span<const std::uint32_t> sources, std::span<const std::uint8_t> cleanFlatSlots, const ShaderRecompiler::SrtRuntime& runtime) {
    PlanWalk walk;
    ShaderRecompiler::SrtWalker{}.EvaluateRuntimeSources(plan, sources, runtime, walk.results, walk.flat, cleanFlatSlots, walk.active);
    return walk;
}

bool readFive(void*, std::uint64_t, std::uint32_t* value) {
    *value = 5u;
    return true;
}

void verifyPlanRetention() {
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
    const auto userData = [&](std::uint32_t index) -> IrValue& {
        auto& reg = make(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister({RegisterBank::Scalar, index});
        auto& read = make(IrOpcode::GetUserData, IrType::U32);
        read.AddArgument(&reg);
        return read;
    };
    const auto binary = [&](IrOpcode opcode, IrType type, IrValue& left, IrValue& right) -> IrValue& {
        auto& value = make(opcode, type);
        value.AddArgument(&left);
        value.AddArgument(&right);
        return value;
    };
    constexpr std::uint32_t Chain = 64;
    IrValue* address = &userData(4);
    for (std::uint32_t step = 0; step < Chain; ++step) address = &binary(IrOpcode::IAdd32, IrType::U32, *address, constant(1));
    auto& load = make(IrOpcode::LoadBufferU32, IrType::U32);
    load.AddArgument(address);
    auto& low = binary(IrOpcode::CompositeExtractU32x2, IrType::U32, binary(IrOpcode::CompositeConstructU32x2, IrType::U32x2, userData(1), load), constant(0));
    auto& mask = binary(IrOpcode::IEqual32, IrType::Bool, userData(5), constant(0));
    auto& choice = make(IrOpcode::SelectU32, IrType::U32);
    choice.AddArgument(&mask);
    choice.AddArgument(&userData(6));
    choice.AddArgument(&load);
    auto& offset = binary(IrOpcode::ReadFirstLane, IrType::U32, choice, mask);
    auto& read = binary(IrOpcode::LoadAddressU32, IrType::U32, binary(IrOpcode::CompositeConstructU64, IrType::U64, userData(2), userData(3)), offset);
    read.SetFlags(MemoryFlags{0, 0});
    IrProgram program;
    auto& resources = program.Resources();
    resources.srtPlanComplete = true;
    resources.resourceTrackingComplete = true;
    MemoryInfo memory;
    memory.kind = ResourceKind::ScalarAddress;
    resources.memoryInfo.push_back(memory);
    for (std::uint32_t source = 0; source < 2u; ++source) {
        DescriptorSource descriptor;
        descriptor.dwordCount = 4;
        for (std::uint32_t dword = 0; dword < 4u; ++dword) descriptor.dwords[dword] = &userData(10u + source * 4u + dword);
        resources.descriptorSources.push_back(descriptor);
        BufferResource buffer;
        buffer.source = source;
        resources.info.buffers.push_back(buffer);
    }
    resources.controlFlow.push_back({&binary(IrOpcode::IEqual32, IrType::Bool, low, constant(7)), {1, 2}, {0}, {}});
    resources.controlFlow.push_back({&binary(IrOpcode::IEqual32, IrType::Bool, read, constant(5)), {3, 4}, {}, {}});
    resources.controlFlow.push_back({nullptr, {}, {}, {}});
    resources.controlFlow.push_back({nullptr, {}, {}, {}});
    resources.controlFlow.push_back({nullptr, {}, {1}, {}});
    const auto plan = ResourceMaterializer{}.ExtractPlan(program);
    constexpr std::size_t Read = 40;
    require(plan.valueStorage.Size() == Read, ("plan retention: the plan keeps " + std::to_string(plan.valueStorage.Size()) + " values; its conditions and descriptor sources read " + std::to_string(Read)).c_str());
    require(std::ranges::none_of(plan.valueStorage, [](const IrValue& value) { return value.Opcode() == IrOpcode::IAdd32; }), "plan retention: the address under a vector load was copied");
    const auto copiedLoad = std::ranges::find_if(plan.valueStorage, [](const IrValue& value) { return value.Opcode() == IrOpcode::LoadBufferU32; });
    require(copiedLoad != plan.valueStorage.end() && copiedLoad->ArgumentCount() == 0u, "plan retention: the vector load was not kept as a leaf");
    std::array<std::uint32_t, 18> userWords{};
    userWords[1] = 7u;
    userWords[2] = 0x1000u;
    userWords[6] = 8u;
    for (std::uint32_t word = 10; word < userWords.size(); ++word) userWords[word] = 0x100u + word;
    SrtRuntime runtime{userWords};
    runtime.readMemory = readFive;
    runtime.readSpecializationMemory = readFive;
    const auto copiedWalk = walkPlan(plan, plan.materializationSources, plan.cleanFlatSlots, runtime);
    const std::array<std::uint32_t, 2> sources{0u, 1u};
    const auto wholeWalk = walkPlan(program.Resources(), sources, {}, runtime);
    require(wholeWalk.active == std::vector<std::uint8_t>{1u, 0u}, "plan retention: the program's walk did not decide both conditions");
    require(copiedWalk.active == wholeWalk.active && copiedWalk.results == wholeWalk.results && copiedWalk.flat == wholeWalk.flat, "plan retention: the plan's walk differs from the program's");
}

void verifyPlanRetentionChains() {
    using namespace ShaderRecompiler;
    std::vector<std::size_t> kept;
    std::vector<std::size_t> conditions;
    for (const std::uint32_t chain : {1u, 16u, 64u}) {
        std::vector<std::uint32_t> code{0xf4080600u, 0xfa000060u, 0xbf8cc07fu, 0x7e140300u};
        code.insert(code.end(), chain, 0x4a141481u);
        code.insert(code.end(), {0x34141482u, 0xe0301000u, 0x80060b0au, 0xbf8c0000u, 0x7e28050bu, 0xf4000540u, 0x28000000u, 0xf4080700u, 0xfa000020u, 0xbf8c0000u, 0xbf078015u, 0xbf840002u, 0xe0700000u, 0x80070000u, 0xbf810000u});
        const std::array<std::uint32_t, 2> userData{0x10000u, 0u};
        const auto plan = GetResourcePlan(FrontendRequest(code, userData));
        kept.push_back(plan->valueStorage.Size());
        conditions.push_back(static_cast<std::size_t>(std::ranges::count_if(plan->controlFlow, [](const ResourceBlock& block) { return block.condition != nullptr; })));
    }
    require(conditions.front() != 0u, "plan retention: the program's plan keeps no branch condition, so the test does not reach the condition's values");
    require(kept[0] == kept[1] && kept[1] == kept[2], ("plan retention: the plan keeps " + std::to_string(kept[0]) + ", " + std::to_string(kept[1]) + " and " + std::to_string(kept[2]) + " values for vector chains of 1, 16 and 64 steps before the scalar load its branch condition reads; it must keep only what the condition's evaluation can reach").c_str());
}

void verifyPlanCopyAnswers() {
    using namespace ShaderRecompiler;
    std::vector<std::unique_ptr<IrValue>> values;
    std::vector<std::unique_ptr<IrBlock>> blocks;
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
    const auto userData = [&](std::uint32_t index) -> IrValue& {
        auto& reg = make(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister({RegisterBank::Scalar, index});
        auto& read = make(IrOpcode::GetUserData, IrType::U32);
        read.AddArgument(&reg);
        return read;
    };
    const auto binary = [&](IrOpcode opcode, IrType type, IrValue& left, IrValue& right) -> IrValue& {
        auto& value = make(opcode, type);
        value.AddArgument(&left);
        value.AddArgument(&right);
        return value;
    };
    const auto vectorLoad = [&](IrValue& address) -> IrValue& {
        auto& load = make(IrOpcode::LoadBufferU32, IrType::U32);
        load.AddArgument(&address);
        return load;
    };
    const auto rawRead = [&](IrValue& low, IrValue& high, IrValue& offset) -> IrValue& {
        auto& read = binary(IrOpcode::LoadAddressU32, IrType::U32, binary(IrOpcode::CompositeConstructU64, IrType::U64, low, high), offset);
        read.SetFlags(MemoryFlags{0, 0});
        return read;
    };
    const auto walks = [&](IrProgram& program, IrValue& read, const std::array<std::uint32_t, 14>& userWords, std::uint8_t expected, const char* what) {
        auto& resources = program.Resources();
        resources.srtPlanComplete = true;
        resources.resourceTrackingComplete = true;
        MemoryInfo memory;
        memory.kind = ResourceKind::ScalarAddress;
        resources.memoryInfo.push_back(memory);
        DescriptorSource descriptor;
        descriptor.dwordCount = 4;
        for (std::uint32_t dword = 0; dword < 4u; ++dword) descriptor.dwords[dword] = &userData(10u + dword);
        resources.descriptorSources.push_back(descriptor);
        BufferResource buffer;
        resources.info.buffers.push_back(buffer);
        resources.controlFlow.push_back({&binary(IrOpcode::IEqual32, IrType::Bool, read, constant(5)), {1, 2}, {}, {}});
        resources.controlFlow.push_back({nullptr, {}, {}, {}});
        resources.controlFlow.push_back({nullptr, {}, {0}, {}});
        const auto plan = ResourceMaterializer{}.ExtractPlan(program);
        SrtRuntime runtime{userWords};
        runtime.readMemory = readFive;
        runtime.readSpecializationMemory = readFive;
        const std::array<std::uint32_t, 1> sources{0u};
        const auto whole = walkPlan(resources, sources, {}, runtime);
        const auto taped = walkPlan(plan, plan.materializationSources, plan.cleanFlatSlots, runtime);
        const auto interpreted = walkPlan(plan, sources, {}, runtime);
        require(whole.active == std::vector<std::uint8_t>{expected}, what);
        require(taped.active == whole.active && taped.results == whole.results, what);
        require(interpreted.active == whole.active && interpreted.results == whole.results, what);
    };
    std::array<std::uint32_t, 14> userWords{};
    for (std::uint32_t word = 10; word < userWords.size(); ++word) userWords[word] = 0x100u + word;

    IrProgram phiProgram;
    auto& shared = userData(1);
    const auto low = [&](IrValue& hidden) -> IrValue& {
        return binary(IrOpcode::CompositeExtractU32x2, IrType::U32, binary(IrOpcode::CompositeConstructU32x2, IrType::U32x2, shared, hidden), constant(0));
    };
    auto& left = *blocks.emplace_back(std::make_unique<IrBlock>(1u));
    auto& right = *blocks.emplace_back(std::make_unique<IrBlock>(2u));
    auto& phi = make(IrOpcode::Phi, IrType::U32);
    phi.AddPhiOperand(&left, &low(vectorLoad(userData(4))));
    phi.AddPhiOperand(&right, &low(vectorLoad(userData(5))));
    userWords[1] = 8u;
    userWords[2] = 0x1000u;
    walks(phiProgram, rawRead(userData(2), userData(3), phi), userWords, 1u, "plan copy: a phi whose incoming values differ below a vector load resolved in the plan, not in the program");

    IrProgram rootProgram;
    auto& mask = binary(IrOpcode::IEqual32, IrType::Bool, userData(5), constant(0));
    auto& choice = make(IrOpcode::SelectU32, IrType::U32);
    choice.AddArgument(&mask);
    choice.AddArgument(&userData(2));
    choice.AddArgument(&vectorLoad(rawRead(userData(8), userData(9), constant(0))));
    userWords[1] = 0u;
    userWords[2] = 0u;
    walks(rootProgram, rawRead(binary(IrOpcode::ReadFirstLane, IrType::U32, choice, mask), userData(3), constant(0)), userWords, 0u, "plan copy: a read from a null base loaded from memory only below a vector load counted as a null root in the plan, not in the program");
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
    auto& firstInput = make(IrOpcode::ReadFirstLane, IrType::U32, {&input, &varying});
    auto& firstLane = make(IrOpcode::ReadFirstLane, IrType::U32, {&lane, &varying});
    auto& firstActive = make(IrOpcode::ReadFirstLane, IrType::U32, {&make(IrOpcode::SelectU32, IrType::U32, {&varying, &input, &lane}), &varying});
    auto& firstInactive = make(IrOpcode::ReadFirstLane, IrType::U32, {&make(IrOpcode::SelectU32, IrType::U32, {&varying, &lane, &input}), &varying});
    auto& conjMask = make(IrOpcode::LogicalAnd, IrType::Bool, {&varying, &uniform});
    auto& firstActiveConjunct = make(IrOpcode::ReadFirstLane, IrType::U32, {&make(IrOpcode::SelectU32, IrType::U32, {&varying, &input, &lane}), &conjMask});
    auto& otherCond = make(IrOpcode::IEqual32, IrType::Bool, {&lane, &constant(5)});
    auto& firstNotImplied = make(IrOpcode::ReadFirstLane, IrType::U32, {&make(IrOpcode::SelectU32, IrType::U32, {&otherCond, &input, &lane}), &conjMask});
    plan.srtReads = {{&input, 0u}, {&lane, 4u}, {nullptr, 8u}};
    auto& constInput = make(IrOpcode::ReadConst, IrType::U32, {&constant(0), &constant(0)});
    auto& constLane = make(IrOpcode::ReadConst, IrType::U32, {&constant(0), &constant(1)});
    auto& constOutside = make(IrOpcode::ReadConst, IrType::U32, {&constant(0), &constant(4)});
    auto& constItself = make(IrOpcode::ReadConst, IrType::U32, {&constant(0), &constant(2)});
    plan.srtReads[2].value = &constItself;
    auto& carried = make(IrOpcode::Phi, IrType::U32, {&input});
    auto& firstCarried = make(IrOpcode::ReadFirstLane, IrType::U32, {&carried, &varying});
    carried.AddArgument(&make(IrOpcode::IAdd32, IrType::U32, {&firstCarried, &constant(1)}));
    plan.srtReads.push_back({nullptr, 12u});
    auto& constFirstItself = make(IrOpcode::ReadConst, IrType::U32, {&constant(0), &constant(3)});
    plan.srtReads[3].value = &make(IrOpcode::ReadFirstLane, IrType::U32, {&constFirstItself, &varying});
    std::array<std::uint32_t, 1> data{4};
    SrtRuntime runtime{data};
    for (const auto& [value, expected, name] : std::array<std::tuple<IrValue*, bool, const char*>, 23>{{
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
             {&firstInput, true, "a first-lane read of user data"},
             {&firstLane, false, "a first-lane read of a vector register"},
             {&firstActive, true, "a first-lane read of a select whose active arm is user data"},
             {&firstInactive, false, "a first-lane read of a select whose active arm is a vector register"},
             {&firstActiveConjunct, true, "a first-lane read of a select whose condition is implied by a conjunct mask"},
             {&firstNotImplied, false, "a first-lane read of a select whose condition is not implied by the mask"},
             {&constInput, true, "a constant read whose slot holds user data"},
             {&constLane, false, "a constant read whose slot holds a vector register"},
             {&constOutside, false, "a constant read past the last slot"},
             {&constItself, false, "a constant read whose slot reads itself"},
             {&firstCarried, false, "a first-lane read of a loop-carried value that reaches itself through the read"},
             {&constFirstItself, false, "a constant read whose slot reads itself through a first-lane read"},
         }}) {
        require(Detail::Evaluator::MayEvaluate(plan, value) == expected, (std::string("MayEvaluate misjudged ") + name).c_str());
        Detail::Evaluator evaluator(plan, runtime);
        std::uint32_t result = 0;
        require(evaluator.Evaluate(Detail::IrNode(value), result) == expected, (std::string("MayEvaluate disagreed with the evaluator on ") + name).c_str());
    }
    require(SrtWalker().ValidateRuntimeValue(plan, &firstActiveConjunct), "ValidateRuntimeValue rejects select on a conjunct of active mask");
    require(!SrtWalker().ValidateRuntimeValue(plan, &firstNotImplied), "ValidateRuntimeValue accepts select not implied by active mask");
    auto& countBits = make(IrOpcode::BitCount32, IrType::U32, {&input});
    auto& findILsb = make(IrOpcode::FindILsb32, IrType::U32, {&input});
    auto& findUMsb = make(IrOpcode::FindUMsb32, IrType::U32, {&input});
    Detail::Evaluator bitEvaluator(plan, runtime);
    std::uint32_t bitResult = 0;
    require(bitEvaluator.Evaluate(Detail::IrNode(&countBits), bitResult) && bitResult == 1u, "Evaluator evaluated BitCount32 incorrectly");
    require(bitEvaluator.Evaluate(Detail::IrNode(&findILsb), bitResult) && bitResult == 2u, "Evaluator evaluated FindILsb32 incorrectly");
    require(bitEvaluator.Evaluate(Detail::IrNode(&findUMsb), bitResult) && bitResult == 2u, "Evaluator evaluated FindUMsb32 incorrectly");
    const std::array<IrOpcode, 7> takenAsEvaluable{IrOpcode::Identity, IrOpcode::Phi, IrOpcode::Void, IrOpcode::GetUserData, IrOpcode::GetShaderBase, IrOpcode::LoadAddressU32, IrOpcode::ReadConstBuffer};
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

    std::vector<std::uint32_t> code{0xf40c0200u, 0xfa000000u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf4000400u, 0xfa000050u, 0xf4080a00u, 0xfa000080u, 0xbf8cc07fu, 0xf42c0814u, 0xfa000000u, 0xbf8cc07fu, 0xbf068010u, 0xbf850008u};
    for (std::uint32_t dword = 0; dword < 8u; dword++) {
        code.push_back(0xbe800300u | ((8u + dword) << 16u) | (32u + dword));
    }
    code.insert(code.end(), {0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u});
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    const auto makeImage = [&](const Texture& texture, std::uint32_t extent) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (extent << 30u), extent << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    const auto oldT = makeImage(textures[0], 3u);
    const auto newT = makeImage(textures[1], 7u);
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    alignas(4096) static std::array<std::uint32_t, 1024> srt{};
    static const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x23001u, code, 0, {}};
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

    static std::uint64_t syncBegin = 0;
    static std::uint64_t syncEnd = 0;
    static std::uint64_t scalarSyncWord = 0;
    static std::uint64_t waits = 0;
    static std::uint64_t rawFirstWords = 0;
    static std::uint64_t scalarSyncHits = 0;
    static bool spanAwareSync = true;
    static bool flushed = false;
    static std::array<std::uint32_t, 8> replacementT{};

    struct FlushReset {
        ~FlushReset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); }
    } flushReset;

    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
        if (!flushed && address < syncEnd && syncBegin < address + bytes) {
            flushed = true;
            ++waits;
            std::memcpy(srt.data(), replacementT.data(), sizeof(replacementT));
        }
    });

    const auto pending = +[](std::uint64_t address, std::size_t bytes, std::span<std::byte>) -> AgcDriver::ShaderMemory::PendingWrite {
        using Pending = AgcDriver::ShaderMemory::PendingWrite;
        if (bytes >= 4096u) {
            return address <= srtAddress && srtAddress < address + bytes ? Pending::Sync : Pending::None;
        }
        if (scalarSyncWord != 0u && address < scalarSyncWord + sizeof(std::uint32_t) && scalarSyncWord < address + bytes) {
            ++scalarSyncHits;
            return Pending::Sync;
        }
        if (flushed) return Pending::None;
        if (spanAwareSync) {
            if (address < syncEnd && syncBegin < address + bytes) return Pending::Sync;
        } else {
            if (address >= syncBegin && address < syncEnd) return Pending::Sync;
        }
        if (address >= srtAddress && address + bytes <= srtAddress + 32u) {
            if (bytes == sizeof(std::uint32_t) && address < syncBegin) ++rawFirstWords;
            return Pending::Raw;
        }
        return Pending::None;
    };
    const auto hookWaits = +[]() -> std::uint64_t { return waits; };

    for (const int mode : {0, 1, 2}) {
        const bool allRaw = mode == 0;
        const bool primaryBarrier = mode == 1;
        srt.fill(0u);
        std::copy(oldT.begin(), oldT.end(), srt.begin());
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
        srt[20] = 1u;
        syncBegin = allRaw ? srtAddress + 32u : srtAddress + 12u;
        syncEnd = srtAddress + 32u;
        scalarSyncWord = srtAddress + 84u;
        waits = 0u;
        rawFirstWords = 0u;
        scalarSyncHits = 0u;
        spanAwareSync = primaryBarrier;
        flushed = false;
        replacementT = newT;

        AgcDriver::ShaderMemory memory({}, pending, nullptr, hookWaits);
        const auto capture = memory.Capture(request);
        require(scalarSyncHits == 0u, "descriptor pendingWrite barrier: a 4-byte scalar read widened its pendingWrite query into a neighbouring dword");
        const bool holdsNew = std::ranges::any_of(capture->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(newT.begin(), newT.end(), value.dwords.begin());
        });
        const bool holdsOld = std::ranges::any_of(capture->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(oldT.begin(), oldT.end(), value.dwords.begin());
        });
        if (allRaw) {
            require(!flushed && waits == 0u && rawFirstWords == 8u && holdsOld && !holdsNew,
                    "descriptor pendingWrite barrier: an all-Raw T# did not return the complete pre-sync T#");
        } else {
            require(flushed && waits == 1u, "descriptor pendingWrite barrier: the pending GPU write on the second half of T# was not flushed");
            if (primaryBarrier) {
                require(rawFirstWords == 0u, "descriptor pendingWrite primary barrier: leading T# dwords took PendingWrite::Raw despite a pending Sync in the 32-byte descriptor span");
            } else {
                require(rawFirstWords > 0u, "descriptor pendingWrite secondary barrier test setup did not exercise initial Raw dword reads before the GPU wait");
            }
            require(holdsNew && !holdsOld,
                    primaryBarrier ? "descriptor pendingWrite primary barrier: capture returned a torn or stale T# instead of the complete post-sync T#"
                                   : "descriptor pendingWrite secondary barrier: cached Raw dwords were not invalidated and re-read after the GPU wait");
        }
    }

    static Texture extraTexture;
    const auto thirdT = makeImage(extraTexture, 11u);
    const auto makeTableV = [](const void* data, std::uint32_t stride, std::uint32_t numRecords) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
        return std::array<std::uint32_t, 4>{
            static_cast<std::uint32_t>(base),
            static_cast<std::uint32_t>((base >> 32u) & 0xffffu) | (stride << 16u),
            numRecords,
            stride == 0u ? 0x31016facu : 0x01016facu,
        };
    };
    const std::vector<std::uint32_t> wholeTableCode{
        0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u,
        0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u,
        0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u,
    };
    auto wideTableCode = wholeTableCode;
    std::replace(wideTableCode.begin(), wideTableCode.end(), 0x8f108510u, 0x9310b010u);
    const std::vector<std::uint32_t> indirectTableCode{
        0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u,
        0xf4080100u, 0xfa000030u, 0x7e200500u, 0x8f108210u, 0xf4200444u, 0x20000000u,
        0x8711ff11u, 0x000000ffu, 0x8f128411u, 0xf4240b02u, 0x24000008u, 0xf4040b96u,
        0xfa000010u, 0xf40c0917u, 0xfa000020u, 0xf09c0f08u, 0x01090000u, 0xe0700000u,
        0x80070000u, 0xbf810000u,
    };
    alignas(4096) static std::array<std::uint32_t, 20> tableSrt{};
    static std::array<std::array<std::uint32_t, 4>, 3> tableMaterials{};
    static std::array<std::uint32_t, 4> tableOutput{};
    const auto tableSrtAddr = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(tableSrt.data()));
    const std::array<std::uint32_t, 2> tableUserData{static_cast<std::uint32_t>(tableSrtAddr), static_cast<std::uint32_t>(tableSrtAddr >> 32u)};
    const auto inPageSrtAddr = srtAddress + 512u * sizeof(std::uint32_t);
    const std::array<std::uint32_t, 2> inPageUserData{static_cast<std::uint32_t>(inPageSrtAddr), static_cast<std::uint32_t>(inPageSrtAddr >> 32u)};
    const auto fillWholeSrt = [&](std::uint32_t* dst, const void* heap, std::uint32_t stride, std::uint32_t numRecords) {
        const auto heapV = makeTableV(heap, stride, numRecords);
        const auto matV = makeTableV(tableMaterials.data(), 16u, 3u);
        const auto outV = makeTableV(tableOutput.data(), 0u, 16u);
        std::fill_n(dst, 20u, 0u);
        std::copy(heapV.begin(), heapV.end(), dst);
        std::copy(matV.begin(), matV.end(), dst + 8u);
        std::copy(outV.begin(), outV.end(), dst + 12u);
    };
    RecompileRequest tableReq = request;
    tableReq.shader = {ShaderStage::Compute, 0x23010u, wholeTableCode, 0, {}};
    tableReq.context.waveSize = 64u;
    tableReq.context.userData = tableUserData;
    tableReq.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    RecompileRequest wideTableReq = tableReq;
    wideTableReq.shader = {ShaderStage::Compute, 0x23018u, wideTableCode, 0, {}};
    wideTableReq.context.userData = inPageUserData;

    static std::uint32_t phase = 0;
    static std::array<std::uint32_t, 8> pendingT0{};
    static std::array<std::uint32_t, 8> pendingT1{};
    static std::array<std::uint32_t, 8> pendingT2{};
    static std::uint64_t record0Addr = 0;
    static std::uint64_t record1Addr = 0;
    static std::uint64_t record2Addr = 0;

    static std::uint32_t forcedHookCalls = 0;
    static std::uint32_t srtRootReads = 0;
    static std::uint64_t lastPendingAddr = 0;
    static bool gateSecondWaitOnSrtRoot = false;

    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t address, std::size_t bytes) {
        if (phase == 1u && !flushed && address < syncEnd && syncBegin < address + bytes) {
            flushed = true;
            ++waits;
            std::memcpy(reinterpret_cast<void*>(record0Addr), pendingT0.data(), sizeof(pendingT0));
            if (record1Addr != 0u) std::memcpy(reinterpret_cast<void*>(record1Addr), pendingT1.data(), sizeof(pendingT1));
        } else if (phase == 2u || phase == 3u) {
            if (waits == 0u && address < record1Addr + 32u && record1Addr + 12u < address + bytes) {
                ++waits;
                std::memcpy(reinterpret_cast<void*>(record1Addr), pendingT1.data(), sizeof(pendingT1));
            } else if ((waits == 1u || (phase == 3u && waits >= 1u)) && address < record2Addr + 32u && record2Addr + 12u < address + bytes) {
                ++waits;
                std::memcpy(reinterpret_cast<void*>(record0Addr), pendingT0.data(), sizeof(pendingT0));
                std::memcpy(reinterpret_cast<void*>(record2Addr), pendingT2.data(), sizeof(pendingT2));
            } else if (phase == 2u && waits == 2u && address < record0Addr + 32u && record0Addr < address + bytes) {
                ++forcedHookCalls;
            }
        } else if (phase == 5u) {
            ++waits;
        }
    });

    const auto multiPending = +[](std::uint64_t address, std::size_t bytes, std::span<std::byte> known) -> AgcDriver::ShaderMemory::PendingWrite {
        using Pending = AgcDriver::ShaderMemory::PendingWrite;
        if (bytes >= 4096u) {
            return address <= srtAddress && srtAddress < address + bytes ? Pending::Sync : Pending::None;
        }
        if (address != lastPendingAddr) {
            lastPendingAddr = address;
            if (address == srtAddress + 512u * sizeof(std::uint32_t) + 48u) {
                ++srtRootReads;
            }
        }
        if (phase == 0u || phase == 5u) return Pending::Raw;
        if (phase == 4u) {
            if (address >= record0Addr && address + bytes <= record0Addr + 32u) {
                if (known.size() == bytes) {
                    std::memcpy(known.data(), reinterpret_cast<const std::byte*>(pendingT0.data()) + (address - record0Addr), bytes);
                }
                return Pending::KnownValue;
            }
            return Pending::Raw;
        }
        if (phase == 1u) {
            if (flushed) return Pending::None;
            if (spanAwareSync) {
                if (address < syncEnd && syncBegin < address + bytes) return Pending::Sync;
            } else {
                if (address >= syncBegin && address < syncEnd) return Pending::Sync;
            }
            return Pending::Raw;
        }
        if (waits == 0u) {
            if (address >= record1Addr + 12u && address < record1Addr + 32u && bytes == sizeof(std::uint32_t)) return Pending::Sync;
            return Pending::Raw;
        }
        if (waits == 1u || (phase == 3u && waits >= 1u)) {
            if (address >= record1Addr && address + bytes <= record1Addr + 32u) return Pending::None;
            if ((!gateSecondWaitOnSrtRoot || srtRootReads >= 2u) && address >= record2Addr + 12u && address < record2Addr + 32u && bytes == sizeof(std::uint32_t)) return Pending::Sync;
            return Pending::Raw;
        }
        if (phase == 2u && waits == 2u) {
            if (gateSecondWaitOnSrtRoot &&
                ((srtRootReads == 3u && address == srtAddress + 512u * sizeof(std::uint32_t) + 56u) ||
                 (srtRootReads == 4u && address == srtAddress + 512u * sizeof(std::uint32_t) + 60u))) {
                return Pending::Sync;
            }
            if ((gateSecondWaitOnSrtRoot && address >= srtAddress + 512u * sizeof(std::uint32_t) + 48u && address < srtAddress + 512u * sizeof(std::uint32_t) + 64u) ||
                (address < record0Addr + 32u && record0Addr < address + bytes)) {
                return Pending::Raw;
            }
        }
        return Pending::None;
    };

    srt.fill(0u);
    std::copy(oldT.begin(), oldT.end(), srt.begin());
    std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
    srt[20] = 1u;
    phase = 0u;
    waits = 0u;
    flushed = false;
    record0Addr = srtAddress;
    record1Addr = 0u;
    pendingT0 = thirdT;
    {
        AgcDriver::ShaderMemory knownTransitionMemory({}, multiPending, nullptr, hookWaits);
        const auto rawCapture = knownTransitionMemory.Capture(request);
        require(!flushed && waits == 0u && std::ranges::any_of(rawCapture->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(oldT.begin(), oldT.end(), value.dwords.begin());
        }), "raw-to-known descriptor transition: initial Raw capture did not cache oldT");
        phase = 4u;
        const auto knownCapture = knownTransitionMemory.Capture(request);
        require(!flushed && waits == 0u && std::ranges::any_of(knownCapture->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(thirdT.begin(), thirdT.end(), value.dwords.begin());
        }), "raw-to-known descriptor transition: cached Raw descriptor was not invalidated and replaced by KnownValue");
    }

    for (std::uint32_t record = 0; record < 128u; ++record) {
        std::copy(oldT.begin(), oldT.end(), srt.begin() + record * 8u);
    }
    fillWholeSrt(tableSrt.data(), srt.data(), 32u, 128u);
    phase = 0u;
    waits = 0u;
    flushed = false;
    record0Addr = srtAddress;
    record1Addr = 0u;
    pendingT0 = thirdT;
    {
        AgcDriver::ShaderMemory fullPageMemory({}, multiPending, nullptr, hookWaits);
        const auto initialCapture = fullPageMemory.Capture(tableReq);
        const auto& initialCol = initialCapture->snapshot.tables.tables.at(0);
        require(initialCol.keys == 128u && initialCol.words.at(initialCol.codes.at(0)).dwords == oldT,
                "full raw page readWords test: initial raw capture did not populate the table with oldT");
        phase = 4u;
        const auto knownCapture = fullPageMemory.Capture(tableReq);
        const auto& knownCol = knownCapture->snapshot.tables.tables.at(0);
        require(!flushed && waits == 0u && knownCol.words.at(knownCol.codes.at(0)).dwords == thirdT,
                "full raw page readWords: cached Raw descriptor words did not consume KnownValue replacement");
        phase = 1u;
        pendingT0 = newT;
        spanAwareSync = true;
        syncBegin = srtAddress + 12u;
        syncEnd = srtAddress + 32u;
        const auto updatedCapture = fullPageMemory.Capture(tableReq);
        const auto& updatedCol = updatedCapture->snapshot.tables.tables.at(0);
        require(flushed && waits == 1u && updatedCol.words.at(updatedCol.codes.at(0)).dwords == newT,
                "full raw page readWords: readWords() bypassed pendingWrite policy on a fully cached wordwise page");
    }

    for (const bool spanSync : {false, true}) {
        std::fill(srt.begin(), srt.end(), 0u);
        std::copy(oldT.begin(), oldT.end(), srt.begin());
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 12u);
        fillWholeSrt(srt.data() + 512u, srt.data(), 48u, 2u);
        phase = 1u;
        waits = 0u;
        flushed = false;
        spanAwareSync = spanSync;
        record0Addr = srtAddress;
        record1Addr = srtAddress + 48u;
        syncBegin = record1Addr + 12u;
        syncEnd = record1Addr + 32u;
        pendingT0 = newT;
        pendingT1 = thirdT;
        AgcDriver::ShaderMemory multiRunMemory({}, multiPending, nullptr, hookWaits);
        const auto multiRunCapture = multiRunMemory.Capture(wideTableReq);
        const auto& col = multiRunCapture->snapshot.tables.tables.at(0);
        require(flushed && waits == 1u && col.keys == 2u, "multi-run table wait: second run did not flush");
        require(col.words.at(col.codes.at(0)).dwords == newT && col.words.at(col.codes.at(1)).dwords == thirdT,
                "multi-run table wait: a GPU wait on a later run did not rebuild descriptors from an earlier run");
    }

    {
        alignas(4096) static std::array<std::uint32_t, 16> indRoot{};
        alignas(4096) static std::array<std::uint32_t, 8> indRecords{};
        alignas(4096) static std::array<std::array<std::uint32_t, 8>, 2> indLinks{};
        static std::array<std::uint32_t, 8> indKeys{};
        const auto ptrAddr = [](const void* p) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)); };
        const auto storePtr = [](std::uint32_t* at, std::uint64_t pointer) {
            at[0] = static_cast<std::uint32_t>(pointer);
            at[1] = static_cast<std::uint32_t>(pointer >> 32u);
        };
        std::fill(srt.begin(), srt.end(), 0u);
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 8u);
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 24u);
        storePtr(indRecords.data() + 2u, ptrAddr(indLinks[0].data()));
        storePtr(indRecords.data() + 6u, ptrAddr(indLinks[1].data()));
        storePtr(indLinks[0].data() + 4u, srtAddress);
        storePtr(indLinks[1].data() + 4u, srtAddress + 64u);
        const auto keysV = makeTableV(indKeys.data(), 0u, static_cast<std::uint32_t>(sizeof(indKeys)));
        const auto outV = makeTableV(tableOutput.data(), 0u, static_cast<std::uint32_t>(sizeof(tableOutput)));
        const auto recsV = makeTableV(indRecords.data(), 0u, static_cast<std::uint32_t>(sizeof(indRecords)));
        indRoot.fill(0u);
        std::copy(keysV.begin(), keysV.end(), indRoot.begin());
        std::copy(outV.begin(), outV.end(), indRoot.begin() + 4u);
        std::copy(recsV.begin(), recsV.end(), indRoot.begin() + 12u);
        const std::array<std::uint32_t, 2> indUserData{static_cast<std::uint32_t>(ptrAddr(indRoot.data())), static_cast<std::uint32_t>(ptrAddr(indRoot.data()) >> 32u)};
        RecompileRequest indReq = tableReq;
        indReq.shader = {ShaderStage::Compute, 0x23020u, indirectTableCode, 0, {}};
        indReq.context.userData = indUserData;
        phase = 1u;
        waits = 0u;
        flushed = false;
        spanAwareSync = false;
        record0Addr = srtAddress + 32u;
        record1Addr = srtAddress + 96u;
        syncBegin = record1Addr + 12u;
        syncEnd = record1Addr + 32u;
        pendingT0 = newT;
        pendingT1 = thirdT;
        AgcDriver::ShaderMemory indirectMemory({}, multiPending, nullptr, hookWaits);
        const auto indCapture = indirectMemory.Capture(indReq);
        const auto& col = indCapture->snapshot.tables.tables.at(0);
        require(flushed && waits == 1u && col.keys == 2u, "indirect table wait: second record did not flush");
        require(col.words.at(col.codes.at(0)).dwords == newT && col.words.at(col.codes.at(1)).dwords == thirdT,
                "indirect table wait: a GPU wait on a later indirect record did not rebuild an earlier record");
    }

    const auto postForcedSrtAddr = srtAddress + 256u * sizeof(std::uint32_t);
    const std::array<std::uint32_t, 2> postForcedUserData{static_cast<std::uint32_t>(postForcedSrtAddr), static_cast<std::uint32_t>(postForcedSrtAddr >> 32u)};
    RecompileRequest postForcedReq = request;
    postForcedReq.context.userData = postForcedUserData;

    for (const int mode : {0, 1, 2}) {
        const bool stridedRuns = mode != 0;
        gateSecondWaitOnSrtRoot = mode == 2;
        const std::uint32_t stride = stridedRuns ? 48u : 32u;
        const std::size_t stepWords = stride / sizeof(std::uint32_t);
        std::fill(srt.begin(), srt.end(), 0u);
        std::copy(oldT.begin(), oldT.end(), srt.begin());
        std::copy(oldT.begin(), oldT.end(), srt.begin() + stepWords);
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 2u * stepWords);
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 256u);
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 256u + 16u);
        srt[256u + 20u] = 1u;
        fillWholeSrt(srt.data() + 512u, srt.data(), stride, 3u);
        RecompileRequest twoWaitReq = stridedRuns ? wideTableReq : tableReq;
        twoWaitReq.context.userData = inPageUserData;
        phase = 2u;
        waits = 0u;
        forcedHookCalls = 0u;
        srtRootReads = 0u;
        lastPendingAddr = 0u;
        flushed = false;
        record0Addr = srtAddress;
        record1Addr = srtAddress + stride;
        record2Addr = srtAddress + 2u * stride;
        pendingT0 = newT;
        pendingT1 = thirdT;
        pendingT2 = newT;
        AgcDriver::ShaderMemory twoWaitMemory({}, multiPending, nullptr, hookWaits);
        const auto twoWaitCapture = twoWaitMemory.Capture(twoWaitReq);
        const auto& col = twoWaitCapture->snapshot.tables.tables.at(0);
        require(waits == 2u && forcedHookCalls == 15u && (!gateSecondWaitOnSrtRoot || srtRootReads == 5u) && col.keys == 3u,
                "multi-wait readWords: forced third attempt did not execute or nested forceSync scope was not preserved");
        require(col.words.at(col.codes.at(0)).dwords == newT && col.words.at(col.codes.at(1)).dwords == thirdT && col.words.at(col.codes.at(2)).dwords == newT,
                "multi-wait readWords: retry pass accepted an unstable epoch after a second invalidation");
        phase = 5u;
        waits = 0u;
        forcedHookCalls = 0u;
        const auto normalAfterForced = twoWaitMemory.Capture(postForcedReq);
        const auto cachedAfterForced = twoWaitMemory.Capture(postForcedReq);
        require(waits == 0u && forcedHookCalls == 0u && std::ranges::any_of(normalAfterForced->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(oldT.begin(), oldT.end(), value.dwords.begin());
        }) && std::ranges::any_of(cachedAfterForced->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(oldT.begin(), oldT.end(), value.dwords.begin());
        }), "forced pass scope: a normal pass over Raw words after a forced third attempt waited on the hook or bumped rawInvalidations");

        std::fill(srt.begin(), srt.end(), 0u);
        std::copy(oldT.begin(), oldT.end(), srt.begin());
        std::copy(oldT.begin(), oldT.end(), srt.begin() + stepWords);
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 2u * stepWords);
        std::copy(oldT.begin(), oldT.end(), srt.begin() + 256u);
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 256u + 16u);
        srt[256u + 20u] = 1u;
        fillWholeSrt(srt.data() + 512u, srt.data(), stride, 3u);
        phase = 3u;
        waits = 0u;
        forcedHookCalls = 0u;
        srtRootReads = 0u;
        lastPendingAddr = 0u;
        flushed = false;
        AgcDriver::ShaderMemory unstableMemory({}, multiPending, nullptr, hookWaits);
        bool refused = false;
        try {
            static_cast<void>(unstableMemory.Capture(twoWaitReq));
        } catch (const std::runtime_error&) {
            refused = true;
        }
        require(refused && waits >= 3u, "unstable epoch refusal: capture succeeded despite invalidation on every retry attempt");
        phase = 5u;
        waits = 0u;
        forcedHookCalls = 0u;
        const auto normalAfterUnstable = unstableMemory.Capture(postForcedReq);
        require(waits == 0u && forcedHookCalls == 0u && std::ranges::any_of(normalAfterUnstable->snapshot.images, [&](const DescriptorValue& value) {
            return std::equal(oldT.begin(), oldT.end(), value.dwords.begin());
        }), "forced pass exception scope: a normal pass over Raw words after a throwing forced attempt waited on the hook or bumped rawInvalidations");
    }
    gateSecondWaitOnSrtRoot = false;
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
    const auto memoryRegions = memory.Regions();
    request.context.memory = memoryRegions;
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

void verifyLaneMaskGuardedDescriptorSources() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    constexpr std::uint32_t Zero = 0x00000000u;
    constexpr std::uint32_t NegativeZero = 0x80000000u;
    constexpr std::uint32_t One = 0x3f800000u;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const std::array<std::uint32_t, 8> valid{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 8> invalid{0x43700000u, 0x43070000u, 0x3b888889u, 0x3bf2b9d6u, 0x3b088889u, 0x3b72b9d6u, 0x3f7f7777u, 0x3f7f0d46u};
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
    const std::vector<std::uint32_t> code{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xd40d006au, 0x00002080u, 0xbf860006u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    request.shader = {ShaderStage::Compute, 0x24000u, code, 0, {}};
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

    fill(invalid, Zero);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.samplers.size() == 1u, "lane-mask guarded sources: the image and its sampler were not planned");
    require(!plan->controlFlow.empty() && std::ranges::any_of(plan->controlFlow, [](const ResourceBlock& block) { return block.condition != nullptr; }), "lane-mask guarded sources: the VCC branch around the image kept no condition");
    for (const auto mode : {Zero, NegativeZero}) {
        fill(invalid, mode);
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::ranges::all_of(capture->snapshot.images[0].dwords, [](std::uint32_t word) { return word == 0u; }), "lane-mask guarded sources: the T# behind a branch no lane takes was not left null");
    }
    fill(invalid, One);
    {
        AgcDriver::ShaderMemory memory({});
        expectFailure([&] { static_cast<void>(memory.Capture(request)); }, "runtime image descriptor is invalid", "lane-mask guarded sources: an invalid T# behind a branch the lanes may take was accepted");
    }
    fill(valid, One);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::equal(valid.begin(), valid.end(), capture->snapshot.images[0].dwords.begin()), "lane-mask guarded sources: the T# behind a branch the lanes may take was not read");
    }
}

void verifyExecGuardedDescriptorSources() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const std::array<std::uint32_t, 8> valid{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 8> invalid{0x43700000u, 0x43070000u, 0x3b888889u, 0x3bf2b9d6u, 0x3b088889u, 0x3b72b9d6u, 0x3f7f7777u, 0x3f7f0d46u};
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
    const std::vector<std::uint32_t> code{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0x7da80010u, 0xbf880006u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    request.shader = {ShaderStage::Compute, 0x24000u, code, 0, {}};
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
    require(plan->info.images.size() == 1u && plan->info.samplers.size() == 1u, "exec guarded sources: the image and its sampler were not planned");
    require(!plan->controlFlow.empty() && std::ranges::any_of(plan->controlFlow, [](const ResourceBlock& block) { return block.condition != nullptr; }), "exec guarded sources: the EXEC branch around the image kept no condition");
    {
        fill(invalid, 0u);
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::ranges::all_of(capture->snapshot.images[0].dwords, [](std::uint32_t word) { return word == 0u; }), "exec guarded sources: the T# behind a branch no lane takes was not left null");
    }
    fill(invalid, 1u);
    {
        AgcDriver::ShaderMemory memory({});
        expectFailure([&] { static_cast<void>(memory.Capture(request)); }, "runtime image descriptor is invalid", "exec guarded sources: an invalid T# behind a branch the lanes may take was accepted");
    }
    fill(valid, 1u);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        require(capture->snapshot.images.size() == 1u && std::equal(valid.begin(), valid.end(), capture->snapshot.images[0].dwords.begin()), "exec guarded sources: the T# behind a branch the lanes may take was not read");
    }
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
        const auto memoryRegions = memory.Regions();
        auto compileRequest = storeRequest;
        compileRequest.context.memory = memoryRegions;
        return Recompile(compileRequest, *capture);
    };
    const auto storePlan = GetResourcePlan(storeRequest);
    require(storePlan->info.images.size() == 2u && storePlan->descriptorSources.size() == 1u, "guarded images: the two stores were not planned as two images of one T#");
    static_cast<void>(compileStore(1u));
    expectFailure([&] { static_cast<void>(compileStore(0u)); }, "incompatible with the static runtime image interface", "guarded images: a store of the wrong dimension behind a taken branch was accepted");
}

bool readDirect(void*, std::uint64_t address, std::uint32_t* value) {
    std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
    return true;
}

void verifySrtTape() {
    using namespace ShaderRecompiler;
    struct Walk {
        bool ok = false;
        std::vector<DescriptorValue> results;
        std::vector<std::uint32_t> flat;
        std::vector<std::uint8_t> active;
        std::size_t poisoned = 0;
        std::uint32_t nullRoots = 0;
        std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves;
        std::vector<std::uint64_t> reads;
        std::uint64_t asBase = 0;
        std::uint64_t asValue = 0;
        std::vector<std::pair<std::uint64_t, std::int32_t>> userDataReads;
        bool operator==(const Walk&) const = default;
    };
    const auto walk = [](const IrResourcePlan& plan, SrtRuntime runtime, bool tape) {
        Walk out;
        SrtReadTrace trace;
        SrtUserDataUse use;
        runtime.readTrace = &trace;
        runtime.userDataUse = &use;
        const std::vector<std::uint32_t> sources(plan.materializationSources.begin(), plan.materializationSources.end());
        const std::vector<std::uint8_t> clean(plan.cleanFlatSlots.begin(), plan.cleanFlatSlots.end());
        const auto sourceSpan = tape ? std::span<const std::uint32_t>(plan.materializationSources) : std::span<const std::uint32_t>(sources);
        const auto cleanSpan = tape ? std::span<const std::uint8_t>(plan.cleanFlatSlots) : std::span<const std::uint8_t>(clean);
        std::vector<SrtReadPoison> poison;
        out.ok = Detail::EvaluateRuntimeSourcesImpl(plan, sourceSpan, runtime, out.results, out.flat, true, cleanSpan, out.active, &poison, &out.nullRoots);
        out.poisoned = poison.size();
        out.leaves = trace.leaves;
        out.reads = trace.otherReads;
        std::sort(out.leaves.begin(), out.leaves.end());
        std::sort(out.reads.begin(), out.reads.end());
        out.reads.erase(std::unique(out.reads.begin(), out.reads.end()), out.reads.end());
        out.asBase = use.asBase;
        out.asValue = use.asValue;
        out.userDataReads = use.reads;
        std::sort(out.userDataReads.begin(), out.userDataReads.end());
        out.userDataReads.erase(std::unique(out.userDataReads.begin(), out.userDataReads.end()), out.userDataReads.end());
        return out;
    };
    const auto answers = [](const IrResourcePlan& plan, const SrtRuntime& runtime) {
        const auto* tape = Detail::SrtTape::For(plan);
        require(tape != nullptr, "SRT tape: a complete plan has no tape");
        std::vector<DescriptorValue> results;
        std::vector<std::uint32_t> flat;
        std::vector<std::uint8_t> active;
        return tape->Run(plan, runtime, results, flat, active);
    };
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const std::array<std::uint32_t, 8> valid{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 8> invalid{0x12345678u, 0xffffffffu, 0xffffffffu, 0u, 0u, 0u, 0u, 0u};
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::array<std::uint32_t, 36> srt{};
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 2> nullUserData{0u, 0u};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const auto requestFor = [&](std::uint64_t address, const std::vector<std::uint32_t>& code) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, address, code, 0, {}};
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
        return request;
    };
    const auto runtimeFor = [&](std::span<const std::uint32_t> words) {
        SrtRuntime runtime{words};
        runtime.readSpecializationMemory = readDirect;
        return runtime;
    };

    const std::vector<std::uint32_t> guarded{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xbf068010u, 0xbf850006u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const auto guardedPlan = GetResourcePlan(requestFor(0x24000u, guarded));
    require(!guardedPlan->controlFlow.empty() && guardedPlan->info.images.size() == 1u, "SRT tape: the guarded program has no branch around its image");
    const auto imageActivity = guardedPlan->descriptorSources.size();
    for (const auto& [image, mode] : std::array<std::pair<const std::array<std::uint32_t, 8>*, std::uint32_t>, 4>{{{&invalid, 0u}, {&invalid, 1u}, {&valid, 0u}, {&valid, 1u}}}) {
        srt.fill(0u);
        std::copy(image->begin(), image->end(), srt.begin());
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
        srt[20] = mode;
        const auto runtime = runtimeFor(userData);
        require(answers(*guardedPlan, runtime), "SRT tape: the tape refused the guarded program's walk");
        const auto tape = walk(*guardedPlan, runtime, true);
        require(tape == walk(*guardedPlan, runtime, false), "SRT tape: the tape's walk differs from the Evaluator's on the guarded program");
        require(tape.ok && tape.active.size() == imageActivity + 1u && tape.active[imageActivity] == mode, "SRT tape: the image behind the branch has the wrong activity");
        require(tape.asBase != 0u && tape.asValue != 0u && !tape.userDataReads.empty(), "SRT tape: the guarded program's walk reported no user-data use");
    }

    const auto nullRuntime = runtimeFor(nullUserData);
    require(!answers(*guardedPlan, nullRuntime), "SRT tape: the tape answered a walk through a null user-data root");
    const auto nullWalk = walk(*guardedPlan, nullRuntime, true);
    require(nullWalk == walk(*guardedPlan, nullRuntime, false) && nullWalk.ok && nullWalk.nullRoots != 0u, "SRT tape: a walk through a null user-data root differs from the Evaluator's");

    std::vector<std::uint32_t> outOfRange{0xf40c0200u, 0xfa000000u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf4000400u, 0xfa000050u, 0xf4080a00u, 0xfa000080u, 0xbf8cc07fu, 0xf42c0814u, 0xfa000000u, 0xbf8cc07fu, 0xbf068010u, 0xbf850008u};
    for (std::uint32_t dword = 0; dword < 8u; dword++) outOfRange.push_back(0xbe800300u | ((8u + dword) << 16u) | (32u + dword));
    outOfRange.insert(outOfRange.end(), {0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u});
    srt.fill(0u);
    std::copy(valid.begin(), valid.end(), srt.begin());
    std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
    srt[20] = 1u;
    std::copy(valid.begin(), valid.end(), srt.begin() + 24);
    const auto outOfRangePlan = GetResourcePlan(requestFor(0x25000u, outOfRange));
    const auto runtime = runtimeFor(userData);
    require(answers(*outOfRangePlan, runtime), "SRT tape: the tape refused a constant read past its V#");
    require(walk(*outOfRangePlan, runtime, true) == walk(*outOfRangePlan, runtime, false), "SRT tape: a constant read past its V# differs from the Evaluator's");
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

constexpr std::uint32_t TableFormat16UNorm = 7;
constexpr std::uint32_t TableFormat8888UNorm = 56;
constexpr std::uint32_t TableFormat32UInt = 20;
constexpr std::uint32_t TableFormat16SInt = 12;
constexpr std::uint32_t TableFormat11_11_10UInt = 34;
constexpr std::uint32_t TableFormat32_32UInt = 62;
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

std::array<std::uint32_t, 8> TableSelected(const ShaderRecompiler::ImageTableSnapshot& tables, std::uint32_t table, const char* message) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    const auto& column = tables.tables.at(table);
    require(column.records == 1u && column.keys == 1u && column.codes.size() == 1u && (column.codes[0] & Abi::PoisonFlag) == 0u, message);
    return column.words.at(column.codes[0]).dwords;
}

void verifyTargetFeatureKey() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 1> code{0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    const std::vector<std::uint32_t> capabilities{1u, 4448u, 5347u};
    const std::vector<std::string_view> extensions{"SPV_KHR_8bit_storage", "SPV_KHR_physical_storage_buffer"};
    const auto build = [&](std::span<const std::uint32_t> capabilityList, std::span<const std::string_view> extensionList) {
        auto keyed = request;
        keyed.target.supportedCapabilities = capabilityList;
        keyed.target.supportedExtensions = extensionList;
        std::vector<std::uint64_t> key;
        RecompileCacheKey::Build(keyed, key);
        return key;
    };
    const auto base = build(capabilities, extensions);
    const std::vector<std::uint32_t> capabilityCopy = capabilities;
    const std::vector<std::string> extensionStorage{"SPV_KHR_8bit_storage", "SPV_KHR_physical_storage_buffer"};
    const std::vector<std::string_view> extensionCopy{extensionStorage[0], extensionStorage[1]};
    require(build(capabilityCopy, extensionCopy) == base, "target feature key: equal capability and extension lists in other storage gave another key");
    auto renamed = extensionStorage;
    renamed[1].back() = 'X';
    const std::vector<std::string_view> renamedViews{renamed[0], renamed[1]};
    require(build(capabilities, renamedViews) != base, "target feature key: an extension name that differs in its last byte gave the same key");
    const std::vector<std::string_view> moved{"SPV_KHR_8bit_storageS", "PV_KHR_physical_storage_buffer"};
    require(build(capabilities, moved) != base, "target feature key: a byte moved between two extension names gave the same key");
    const std::vector<std::uint32_t> more{1u, 4448u, 5347u, 11u};
    require(build(more, extensions) != base, "target feature key: an added capability gave the same key");
    std::vector<std::string> many;
    for (std::uint32_t index = 0; index < 40u; ++index) many.push_back("SPV_EXT_descriptor_indexing_" + std::to_string(index));
    const std::vector<std::string_view> manyViews(many.begin(), many.end());
    require(build(capabilities, manyViews).size() == base.size(), "target feature key: the key grows with the device's extension names");
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

    place(1028u, 4u);
    CountingMemory insideWordReads{reinterpret_cast<std::uintptr_t>(words.data() + 1028u), reinterpret_cast<std::uintptr_t>(words.data() + 1060u)};
    const auto insideByWord = capture(request, insideWordReads, false);
    AgcDriver::ShaderMemory inside({});
    const auto insideCaptured = inside.Capture(request);
    const auto regions = inside.Regions();
    const auto first = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(words.data()));
    const auto past = first + words.size() * sizeof(std::uint32_t);
    const auto tableFirst = first + 1028u * sizeof(std::uint32_t);
    const auto tablePast = first + 1060u * sizeof(std::uint32_t);
    const bool exact = std::all_of(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& region) { return region.guestAddress + region.bytes.size() <= first || region.guestAddress >= past || (region.guestAddress >= tableFirst && region.guestAddress + region.bytes.size() <= tablePast); });
    require(insideCaptured->snapshot.tables == insideByWord->snapshot.tables && TableCovered(regions, words.data() + 1028u, 32u * sizeof(std::uint32_t)) && exact, "image table bulk reads: run reads inside 16-word blocks changed the snapshot or captured words outside the records");
}

std::uint64_t pendingTableWord = 0;
std::uint32_t pendingTableValue = 0;
bool pendingTableKnown = false;

AgcDriver::ShaderMemory::PendingWrite PendingTableWrite(std::uint64_t address, std::size_t bytes, std::span<std::byte> known) {
    using Pending = AgcDriver::ShaderMemory::PendingWrite;
    if (pendingTableWord < address || pendingTableWord - address >= bytes) return Pending::None;
    if (!pendingTableKnown) return Pending::Sync;
    if (known.size() == sizeof(pendingTableValue)) std::memcpy(known.data(), &pendingTableValue, sizeof(pendingTableValue));
    return Pending::KnownValue;
}

std::uint64_t deferredTableRecord = 0;
std::array<std::uint32_t, 8> deferredTableWords{};
bool deferredTableStored = false;

void StoreDeferredTableRecord(std::uint64_t address, std::size_t bytes) {
    if (deferredTableStored || deferredTableRecord < address || deferredTableRecord - address >= bytes) return;
    deferredTableStored = true;
    AgcDriver::GuestMemory::StoreOwnBytes(deferredTableRecord, std::as_bytes(std::span(deferredTableWords)));
}

std::uint32_t* AllocateWatchedTable(std::size_t bytes) {
    auto* heap = static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    require(heap != nullptr, "table snapshot reuse: cannot allocate the table in the guest arena");
    GuestArena::GuestArenaCommit_nid_postfix(heap, bytes, 0x04u, bytes);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(heap, bytes);
#endif
    return heap;
}

void ProtectTablePage(std::uintptr_t page, std::uint32_t protection) {
    GuestArena::GuestArenaSetProtection_nid_postfix(page, 4096u, protection);
#ifdef _WIN32
    DWORD previous = 0;
    require(VirtualProtect(reinterpret_cast<void*>(page), 4096u, protection, &previous) != 0, "table snapshot reuse: cannot protect a table page");
#endif
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096u);
}

void verifyRegionMerge(bool strided = false, bool benchmark = false) {
    using namespace ShaderRecompiler;
    constexpr std::size_t SlotBytes = 8192;
    constexpr std::size_t TableBytes = 4096;
    const std::size_t stride = strided ? 64u : 32u;
    constexpr std::size_t MaximumBlocks = 64;
    constexpr std::size_t Bytes = (MaximumBlocks + 8u) * SlotBytes;
    auto* heap = AllocateWatchedTable(Bytes);
    struct Release {
        void* pointer;
        ~Release() { GuestArena::GuestArenaRelease_nid_postfix(pointer, Bytes); }
    } release{heap};
    std::vector<TableGuest> guests(MaximumBlocks);
    std::vector<std::array<std::uint32_t, 2>> userData(MaximumBlocks);
    for (std::size_t block = 0; block < MaximumBlocks; ++block) {
        auto* table = heap + block * SlotBytes / sizeof(std::uint32_t);
        for (std::size_t record = 0; record < TableBytes / stride; ++record) {
            const auto words = TableT(reinterpret_cast<const void*>(0x100000000ull + record * 256u), TableFormat8888UNorm, TableType2D, 1u);
            std::copy(words.begin(), words.end(), table + record * stride / sizeof(std::uint32_t));
        }
        FillTableSrt(guests[block], table, stride, TableBytes / stride);
        userData[block] = TableUserData(guests[block]);
    }
    const bool watched = AgcDriver::GuestMemory::CollectWrites(reinterpret_cast<std::uintptr_t>(heap), Bytes) != 0u;
    for (const std::size_t count : {0u, 1u, 2u, 3u, 7u, 8u, 16u, 64u}) {
        std::vector<MemoryRegion> initial;
        for (std::size_t block = 0; block <= count; ++block) {
            for (std::size_t offset = TableBytes; offset < SlotBytes; offset += 64u) {
                auto* word = heap + (block * SlotBytes + offset) / sizeof(std::uint32_t);
                *word = static_cast<std::uint32_t>(block * SlotBytes + offset);
                initial.push_back({reinterpret_cast<std::uintptr_t>(word), std::as_bytes(std::span(word, 1))});
            }
        }
        AgcDriver::ShaderMemory memory(initial);
        for (std::size_t position = 0; position < count; ++position) {
            const auto block = (position * (count > 1u ? count - 1u : 1u)) % count;
            const auto request = TableRequest(TableWholeCode, userData[block]);
            {
                AgcDriver::ShaderMemory warm({});
                static_cast<void>(warm.Capture(request));
            }
            static_cast<void>(memory.Capture(request));
        }
        if (watched) require(memory.SuppliedBlocks().size() == count, "region merge: the fixture did not retain every shared table");
        const auto verify = [&](const std::vector<MemoryRegion>& regions, bool recent) {
            for (std::size_t index = 0; index < regions.size(); ++index) {
                const auto& region = regions[index];
                require(index == 0u || regions[index - 1u].guestAddress + regions[index - 1u].bytes.size() <= region.guestAddress, "region merge: regions were reordered or duplicated");
                require(std::memcmp(region.bytes.data(), reinterpret_cast<const void*>(region.guestAddress), region.bytes.size()) == 0, "region merge: collected words changed");
            }
            for (std::size_t block = 0; block < count; ++block) {
                for (std::size_t record = 0; record < TableBytes / stride; ++record) require(TableCovered(regions, heap + (block * SlotBytes + record * stride) / sizeof(std::uint32_t), 32u), "region merge: a shared table record was omitted");
            }
            for (const auto& entry : initial) require(TableCovered(regions, reinterpret_cast<const std::uint32_t*>(entry.guestAddress), entry.bytes.size()) != recent, "region merge: initial-region visibility changed");
        };
        const auto regions = memory.Regions();
        verify(regions, false);
        verify(memory.TakeRecentRegions(), true);
        require(memory.TakeRecentRegions().empty(), "region merge: recent regions were not reset");
        verify(memory.Regions(), false);
        if (!strided && benchmark) {
            constexpr std::size_t Iterations = 1000;
            std::size_t total = 0;
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t iteration = 0; iteration < Iterations; ++iteration) total += memory.Regions().size();
            const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count();
            require(total == Iterations * regions.size(), "region merge: repeated collection changed coverage");
            std::printf("region-merge blocks=%zu regions=%zu iterations=%zu us_per_call=%.3f\n", count, regions.size(), Iterations, elapsed / Iterations);
        }
    }
    std::puts("region merge: mixed initial, captured and shared regions retain order, coverage and recent-read semantics");
}


void verifyTableSnapshotReuse() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::uint32_t Records = 4096;
    constexpr std::size_t Bytes = Records * 32u;
    const auto texture = [](std::uint32_t index) { return TableT(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x100000000ull + index * 256ull)), TableFormat8888UNorm, TableType2D, 1u + index % 5u); };
    const auto fill = [&](std::uint32_t* table, std::uint32_t records) {
        for (std::uint32_t record = 0; record < records; record++) {
            const auto words = texture(record % 1024u);
            std::copy(words.begin(), words.end(), table + static_cast<std::size_t>(record) * 8u);
        }
    };
    struct Outcome {
        ImageTableSnapshot tables;
        bool covered = false;
        ImageTableReuseCounts reuse;
        AgcDriver::ShaderMemory::SupplyCounts supplied{};
        const std::byte* tableBytes = nullptr;
        std::size_t tableRegionBytes = 0;
        std::vector<std::pair<std::uint64_t, std::size_t>> tableRegions;
    };
    const auto sortedDisjoint = [](const std::vector<MemoryRegion>& regions) {
        for (std::size_t index = 1; index < regions.size(); index++) {
            if (regions[index - 1].guestAddress + regions[index - 1].bytes.size() > regions[index].guestAddress) return false;
        }
        return true;
    };
    const auto holds = [](const std::vector<MemoryRegion>& regions, const std::uint32_t* table, std::size_t bytes, const std::uint32_t* expected) {
        const auto begin = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(table));
        std::size_t found = 0;
        for (const auto& region : regions) {
            const auto from = std::max(region.guestAddress, begin);
            const auto to = std::min(region.guestAddress + region.bytes.size(), begin + bytes);
            if (from >= to) continue;
            if (std::memcmp(region.bytes.data() + (from - region.guestAddress), reinterpret_cast<const std::byte*>(expected) + (from - begin), static_cast<std::size_t>(to - from)) != 0) return false;
            found += static_cast<std::size_t>(to - from);
        }
        return found == bytes;
    };
    const auto capture = [&](const RecompileRequest& request, const std::uint32_t* table, std::size_t bytes, AgcDriver::ShaderMemory::PendingWriteQuery pending) {
        const auto before = ResourceMaterializer::TableReuse();
        const auto suppliedBefore = AgcDriver::ShaderMemory::Supplies();
        Outcome outcome;
        AgcDriver::ShaderMemory memory({}, pending);
        outcome.tables = memory.Capture(request)->snapshot.tables;
        const auto regions = memory.Regions();
        const auto suppliedAfter = AgcDriver::ShaderMemory::Supplies();
        outcome.supplied = {suppliedAfter.shared - suppliedBefore.shared, suppliedAfter.copied - suppliedBefore.copied, suppliedAfter.spilled - suppliedBefore.spilled};
        for (const auto& region : regions) {
            if (region.guestAddress < reinterpret_cast<std::uintptr_t>(table) || region.guestAddress >= reinterpret_cast<std::uintptr_t>(table) + bytes) continue;
            if (outcome.tableBytes == nullptr) outcome.tableBytes = region.bytes.data();
            if (region.bytes.data() == outcome.tableBytes + outcome.tableRegionBytes) outcome.tableRegionBytes += region.bytes.size();
            outcome.tableRegions.emplace_back(region.guestAddress, region.bytes.size());
        }
        outcome.covered = TableCovered(regions, table, bytes) && std::all_of(regions.begin(), regions.end(), [&](const MemoryRegion& region) {
            return region.guestAddress < reinterpret_cast<std::uintptr_t>(table) || region.guestAddress >= reinterpret_cast<std::uintptr_t>(table) + bytes || std::memcmp(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(region.guestAddress)), region.bytes.data(), region.bytes.size()) == 0;
        });
        const auto after = ResourceMaterializer::TableReuse();
        outcome.reuse = {after.unwritten - before.unwritten, after.compared - before.compared, after.wordsRead - before.wordsRead, after.shared - before.shared, after.columns - before.columns};
        return outcome;
    };
    const auto recordWords = [](const Outcome& outcome, std::uint32_t record) { return outcome.tables.tables.at(0).words.at(outcome.tables.tables.at(0).codes.at(record)).dwords; };
    const auto heldRecords = [&](const Outcome& outcome, const std::uint32_t* table, std::uint32_t records) {
        for (std::uint32_t record = 0; record < records; record++) {
            std::array<std::uint32_t, 8> words{};
            std::copy_n(table + static_cast<std::size_t>(record) * 8u, 8u, words.begin());
            if (recordWords(outcome, record) != words) return false;
        }
        return true;
    };
    const auto sharesColumn = [](const Outcome& left, const Outcome& right) {
        const auto& a = left.tables.tables.at(0);
        const auto& b = right.tables.tables.at(0);
        return a.codes.Shares(b.codes) && a.words.Shares(b.words) && a.readRanges.Shares(b.readRanges);
    };
    const auto sharedColumnOf = [](const std::shared_ptr<const ResourceCapture>& capture, const Outcome& outcome) {
        const auto& a = capture->snapshot.tables.tables.at(0);
        const auto& b = outcome.tables.tables.at(0);
        return a.codes.Shares(b.codes) && a.words.Shares(b.words) && a.readRanges.Shares(b.readRanges);
    };

    auto* heap = AllocateWatchedTable(Bytes);
    fill(heap, Records);
    const auto address = reinterpret_cast<std::uintptr_t>(heap);
    const bool watched = AgcDriver::GuestMemory::CollectWrites(address, Bytes) != 0;
    alignas(4096) static TableGuest guest;
    FillTableSrt(guest, heap, 32u, Records);
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableWholeCode, userData);
    const auto expectUnwritten = [&](const Outcome& outcome, const std::string& what) {
        require(heldRecords(outcome, heap, Records) && outcome.covered, ("table snapshot reuse: " + what + ": the snapshot or the captured words do not hold the table").c_str());
        if (watched) require(outcome.reuse.unwritten == 1u && outcome.reuse.compared == 0u && outcome.reuse.wordsRead == 0u, ("table snapshot reuse: " + what + ": an unwritten table was read again").c_str());
        else require(outcome.reuse.compared == 1u && outcome.reuse.wordsRead == Records * 8u, ("table snapshot reuse: " + what + ": an unchanged unwatched table was not compared and reused").c_str());
    };
    const auto expectReread = [&](const Outcome& outcome, const std::string& what) {
        require(outcome.reuse.unwritten == 0u && outcome.reuse.wordsRead == Records * 8u, ("table snapshot reuse: " + what + ": the table was not read again exactly once").c_str());
    };

    const auto fresh = capture(request, heap, Bytes, nullptr);
    require(fresh.reuse.unwritten == 0u && fresh.reuse.compared == 0u && fresh.reuse.wordsRead == Records * 8u && heldRecords(fresh, heap, Records) && fresh.covered, "table snapshot reuse: the first snapshot is wrong");
    require(fresh.reuse.columns == 1u && fresh.reuse.shared == 0u, "table snapshot reuse: the first snapshot did not build one column");
    const std::vector<std::uint32_t> original(heap, heap + Bytes / sizeof(std::uint32_t));
    const auto unchanged = capture(request, heap, Bytes, nullptr);
    expectUnwritten(unchanged, "an unchanged table");
    require(unchanged.tables == fresh.tables, "table snapshot reuse: a reused snapshot differs from the snapshot it reuses");
    const auto reusedAgain = capture(request, heap, Bytes, nullptr);
    expectUnwritten(reusedAgain, "an unchanged table reused again");
    require(sharesColumn(unchanged, fresh) && sharesColumn(reusedAgain, fresh) && unchanged.reuse.shared == 1u && reusedAgain.reuse.shared == 1u && unchanged.reuse.columns == 0u && reusedAgain.reuse.columns == 0u, "table snapshot reuse: an unchanged table reused twice did not share one column without a copy");
    if (watched) {
        const auto sharedWords = [&](const Outcome& outcome) { return outcome.supplied.shared == Records * 8u && outcome.supplied.copied == 0u && outcome.supplied.spilled == 0u && outcome.tableRegionBytes == Bytes; };
        require(sharedWords(unchanged) && sharedWords(reusedAgain) && unchanged.tableBytes != nullptr && unchanged.tableBytes == reusedAgain.tableBytes && fresh.tableBytes != unchanged.tableBytes && fresh.supplied.shared == 0u, "table snapshot reuse: the captured words of an unchanged table were copied instead of shared");
    }
    require(fresh.tableRegions.size() == Bytes / 4096u && unchanged.tableRegions == fresh.tableRegions && reusedAgain.tableRegions == fresh.tableRegions, "table snapshot reuse: a reused table's words are not reported in the regions a read of them reports");
    {
        AgcDriver::ShaderMemory stages({});
        const auto suppliedBefore = AgcDriver::ShaderMemory::Supplies();
        const auto vertex = stages.Capture(request);
        const auto vertexRecent = stages.TakeRecentRegions();
        const auto pixel = stages.Capture(request);
        const auto pixelRecent = stages.TakeRecentRegions();
        const auto both = stages.Regions();
        require(sharedColumnOf(vertex, fresh) && sharedColumnOf(pixel, fresh) && sortedDisjoint(vertexRecent) && sortedDisjoint(pixelRecent) && sortedDisjoint(both) && holds(vertexRecent, heap, Bytes, original.data()) && holds(pixelRecent, heap, Bytes, original.data()) && holds(both, heap, Bytes, original.data()), "table snapshot reuse: two stages of one shader memory did not each capture the shared table once");
        alignas(4096) static TableGuest halfGuest;
        FillTableSrt(halfGuest, heap, 32u, Records / 2u);
        const auto halfUserData = TableUserData(halfGuest);
        const auto half = stages.Capture(TableRequest(TableWholeCode, halfUserData));
        const auto all = stages.Regions();
        const auto suppliedAfter = AgcDriver::ShaderMemory::Supplies();
        require(half->snapshot.tables.tables.at(0).keys == Records / 2u && sortedDisjoint(all) && holds(all, heap, Bytes, original.data()), "table snapshot reuse: a read over supplied words reported them twice or changed them");
        if (watched) require(suppliedAfter.shared - suppliedBefore.shared == 2u * Records * 8u && suppliedAfter.copied == suppliedBefore.copied && suppliedAfter.spilled - suppliedBefore.spilled == Records * 8u, "table snapshot reuse: a second stage copied the shared words, or a read over them did not move them into pages once");
    }
    AgcDriver::ShaderMemory heldMemory({});
    const auto heldCapture = heldMemory.Capture(request);
    const auto freshColumn = [&](const Outcome& outcome, const std::string& what) {
        require(outcome.reuse.columns == 1u && outcome.reuse.shared == 0u && !sharesColumn(outcome, fresh) && !(outcome.tables == fresh.tables) && outcome.supplied.shared == 0u, ("table snapshot reuse: " + what + ": the snapshot shares the old column or holds its words").c_str());
        require(heldRecords(unchanged, original.data(), Records) && heldRecords(reusedAgain, original.data(), Records) && sharesColumn(reusedAgain, fresh), ("table snapshot reuse: " + what + ": the old column changed under the snapshots sharing it").c_str());
        require(sharedColumnOf(heldCapture, fresh) && holds(heldMemory.Regions(), heap, Bytes, original.data()), ("table snapshot reuse: " + what + ": the words a live shader memory captured before it changed").c_str());
    };

    const auto changeRecord = [&](std::uint32_t record, std::uint32_t replacement, const std::function<void(std::uint64_t, const std::array<std::uint32_t, 8>&)>& store) {
        const auto words = texture(replacement);
        store(address + static_cast<std::uint64_t>(record) * 32u, words);
    };
    changeRecord(77u, 2000u, [](std::uint64_t at, const std::array<std::uint32_t, 8>& words) { std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(at)), words.data(), sizeof(words)); });
    const auto cpuStore = capture(request, heap, Bytes, nullptr);
    expectReread(cpuStore, "a CPU store");
    require(heldRecords(cpuStore, heap, Records) && cpuStore.covered && recordWords(cpuStore, 77u) == texture(2000u), "table snapshot reuse: the snapshot after a CPU store misses the stored T#");
    freshColumn(cpuStore, "a CPU store");
    expectUnwritten(capture(request, heap, Bytes, nullptr), "the table after a CPU store");

    changeRecord(1234u, 2001u, [](std::uint64_t at, const std::array<std::uint32_t, 8>& words) { AgcDriver::GuestMemory::Write(at, std::as_bytes(std::span(words)), 4); });
    const auto driverStore = capture(request, heap, Bytes, nullptr);
    expectReread(driverStore, "a driver store (CP write, DMA copy or fill)");
    require(heldRecords(driverStore, heap, Records) && recordWords(driverStore, 1234u) == texture(2001u), "table snapshot reuse: the snapshot after a driver store misses the stored T#");
    freshColumn(driverStore, "a driver store");
    expectUnwritten(capture(request, heap, Bytes, nullptr), "the table after a driver store");

    changeRecord(2048u, 2002u, [](std::uint64_t at, const std::array<std::uint32_t, 8>& words) { AgcDriver::GuestMemory::StoreOwnBytes(at, std::as_bytes(std::span(words))); });
    const auto gpuStore = capture(request, heap, Bytes, nullptr);
    expectReread(gpuStore, "a GPU result stored by the driver");
    require(heldRecords(gpuStore, heap, Records) && recordWords(gpuStore, 2048u) == texture(2002u), "table snapshot reuse: the snapshot after a stored GPU result misses the stored T#");
    freshColumn(gpuStore, "a GPU result stored by the driver");
    expectUnwritten(capture(request, heap, Bytes, nullptr), "the table after a stored GPU result");

    AgcDriver::GuestMemory::MarkWritten(address + 3000u * 32u, 32u);
    const auto marked = capture(request, heap, Bytes, nullptr);
    require(marked.reuse.unwritten == 0u && marked.reuse.compared == 1u && heldRecords(marked, heap, Records) && marked.tables == gpuStore.tables, "table snapshot reuse: a GPU write into an import (MarkWritten) did not make the table compared again");
    require(sharesColumn(marked, gpuStore) && marked.reuse.columns == 0u, "table snapshot reuse: a compared table did not share the column it matched");

    pendingTableWord = address + 500u * 32u;
    pendingTableValue = 0x0badf00du;
    pendingTableKnown = true;
    const auto known = capture(request, heap, Bytes, &PendingTableWrite);
    expectReread(known, "a pending GPU write with a known value");
    require(recordWords(known, 500u)[0] == pendingTableValue && recordWords(known, 499u) == recordWords(gpuStore, 499u), "table snapshot reuse: the snapshot under a pending GPU write misses its known value");
    freshColumn(known, "a pending GPU write with a known value");
    require(!sharesColumn(known, gpuStore), "table snapshot reuse: a pending GPU write's snapshot shares the column read before it");
    pendingTableKnown = false;
    const auto synced = capture(request, heap, Bytes, &PendingTableWrite);
    require(synced.reuse.unwritten == 0u && heldRecords(synced, heap, Records), "table snapshot reuse: a table under a pending GPU write was reused without a read");
    require(!sharesColumn(synced, known) && recordWords(known, 500u)[0] == pendingTableValue, "table snapshot reuse: a synced read shares or changed the column of the known value it replaced");
    pendingTableWord = 0;
    const auto settled = capture(request, heap, Bytes, nullptr);
    require(settled.reuse.unwritten == 0u && heldRecords(settled, heap, Records), "table snapshot reuse: a snapshot taken under a pending GPU write was reused without a read");
    expectUnwritten(capture(request, heap, Bytes, nullptr), "the table after the pending GPU write");

    deferredTableRecord = address + 3500u * 32u;
    deferredTableWords = texture(2003u);
    struct FlushHookReset {
        ~FlushHookReset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); }
    } flushHookReset;
    AgcDriver::GuestMemory::SetFlushHook(&StoreDeferredTableRecord);
    const auto deferred = capture(request, heap, Bytes, nullptr);
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    require(deferredTableStored && deferred.reuse.unwritten == 0u && heldRecords(deferred, heap, Records) && recordWords(deferred, 3500u) == texture(2003u), "table snapshot reuse: a GPU result a read would store first was not stored and read before the table was reused");
    freshColumn(deferred, "a GPU result stored by the flush hook");
    expectUnwritten(capture(request, heap, Bytes, nullptr), "the table after a GPU result stored by the flush hook");

    constexpr std::size_t UnrelatedBytes = 65536;
    void* unrelated = ::operator new(UnrelatedBytes, std::align_val_t{UnrelatedBytes});
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(unrelated, UnrelatedBytes, true, true, false);
    }
    const auto remapped = capture(request, heap, Bytes, nullptr);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(unrelated);
    }
    ::operator delete(unrelated, std::align_val_t{UnrelatedBytes});
    require(remapped.reuse.unwritten == 0u && remapped.reuse.compared == 1u && heldRecords(remapped, heap, Records), "table snapshot reuse: a table was reused without a read after a guest mapping changed");

    auto& flipped = heap[100u * 8u + 1u];
    const auto keptWord = flipped;
    flipped ^= 0x100u;
    const auto changed = capture(request, heap, Bytes, nullptr);
    flipped = keptWord;
    const auto restored = capture(request, heap, Bytes, nullptr);
    const auto& remappedColumn = remapped.tables.tables.at(0);
    const auto& changedColumn = changed.tables.tables.at(0);
    const auto& restoredColumn = restored.tables.tables.at(0);
    require(changed.reuse.columns == 1u && !changedColumn.words.Shares(remappedColumn.words) && !(changedColumn.words == remappedColumn.words) && changedColumn.words.Hash() != remappedColumn.words.Hash(), "table snapshot reuse: a changed table matched the column read before the change");
    require(restored.reuse.columns == 1u && !sharesColumn(restored, remapped) && !sharesColumn(restored, changed) && restored.tables == remapped.tables && restoredColumn.codes.Hash() == remappedColumn.codes.Hash() && restoredColumn.words.Hash() == remappedColumn.words.Hash() && !(restored.tables == changed.tables), "table snapshot reuse: a table written back to its old words is not a new column equal to the old one");
    const SharedValues<std::uint32_t> sealed(std::vector<std::uint32_t>{1u, 2u, 3u});
    const auto alias = sealed;
    const SharedValues<std::uint32_t> equal(std::vector<std::uint32_t>{1u, 2u, 3u});
    const SharedValues<std::uint32_t> unequal(std::vector<std::uint32_t>{1u, 2u, 4u});
    require(alias.Shares(sealed) && alias == sealed && !equal.Shares(sealed) && equal == sealed && equal.Hash() == sealed.Hash() && !unequal.Shares(sealed) && !(unequal == sealed) && unequal.Hash() != sealed.Hash() && SharedValues<std::uint32_t>{}.Shares(SharedValues<std::uint32_t>(std::vector<std::uint32_t>{})), "table snapshot reuse: shared values compare by identity before their contents");

    struct CapturedVariant {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        std::vector<std::uint32_t> words;
        std::vector<AgcDriver::SharedWords> shared;
    };
    const auto captureVariant = [](const RecompileRequest& request) {
        CapturedVariant variant;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        const auto supplied = memory.SuppliedBlocks();
        for (const auto& region : memory.TakeRecentRegions()) {
            variant.runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
            const auto offset = variant.words.size();
            variant.words.resize(offset + region.bytes.size() / sizeof(std::uint32_t));
            std::memcpy(variant.words.data() + offset, region.bytes.data(), region.bytes.size());
            AgcDriver::AppendSharedWords(variant.shared, variant.runs.size() - 1u, offset, region.bytes, supplied);
        }
        return variant;
    };
    const auto sameVariant = [](const CapturedVariant& left, const CapturedVariant& right, std::size_t& compared) { return left.runs == right.runs && AgcDriver::SameCapturedWords(left.words, left.shared, right.words, right.shared, &compared); };
    const auto sameBlock = [](const CapturedVariant& left, const CapturedVariant& right) { return left.shared.size() == 1u && right.shared.size() == 1u && !left.shared[0].block.owner_before(right.shared[0].block) && !right.shared[0].block.owner_before(left.shared[0].block); };
    const auto tableWords = static_cast<std::size_t>(Records) * 8u;
    const auto keptVariant = captureVariant(request);
    const auto freshVariant = captureVariant(request);
    std::size_t comparedWords = 0;
    require(sameVariant(keptVariant, freshVariant, comparedWords) && keptVariant.words == freshVariant.words, "draw cache identity: two captures of an unchanged table did not match");
    if (watched) require(sameBlock(keptVariant, freshVariant) && keptVariant.shared[0].count == tableWords && comparedWords == keptVariant.words.size() - tableWords, "draw cache identity: two captures sharing a table column compared its words instead of its identity");
    else require(keptVariant.shared.empty() && comparedWords == keptVariant.words.size(), "draw cache identity: captures of an unwatched table were not compared word by word");

    alignas(16) static std::array<std::uint32_t, 4> variantBuffer{};
    const auto variantBufferV = TableV(variantBuffer.data(), 0u, static_cast<std::uint32_t>(sizeof(variantBuffer)));
    const std::array<std::uint32_t, 4> variantKeptV{guest.srt[12], guest.srt[13], guest.srt[14], guest.srt[15]};
    std::copy(variantBufferV.begin(), variantBufferV.end(), guest.srt.begin() + 12);
    const auto otherSrtVariant = captureVariant(request);
    std::copy(variantKeptV.begin(), variantKeptV.end(), guest.srt.begin() + 12);
    require(keptVariant.runs == otherSrtVariant.runs && !sameVariant(keptVariant, otherSrtVariant, comparedWords), "draw cache identity: a capture with other SRT words matched through the table column it shares");
    if (watched) require(sameBlock(keptVariant, otherSrtVariant), "draw cache identity: a capture with other SRT words did not share the unchanged table column");

    auto& variantWord = heap[9u * 8u + 1u];
    const auto variantKeptWord = variantWord;
    variantWord ^= 0x100u;
    const auto writtenVariant = captureVariant(request);
    variantWord = variantKeptWord;
    require(keptVariant.runs == writtenVariant.runs && !sameVariant(keptVariant, writtenVariant, comparedWords) && comparedWords == keptVariant.words.size(), "draw cache identity: a capture after a CPU store matched the capture before it");
    const auto rereadVariant = captureVariant(request);
    const auto rewrittenVariant = captureVariant(request);
    require(sameVariant(keptVariant, rereadVariant, comparedWords) && comparedWords == keptVariant.words.size() && sameVariant(keptVariant, rewrittenVariant, comparedWords) && comparedWords == keptVariant.words.size(), "draw cache identity: a table written back to its old words did not match by its words alone");
    if (watched) require(writtenVariant.shared.empty() && rereadVariant.shared.empty() && rewrittenVariant.shared.size() == 1u && !sameBlock(keptVariant, rewrittenVariant) && keptVariant.shared[0].block.expired(), "draw cache identity: a new column with the old words was taken for the replaced column");

    const auto identityBlock = std::make_shared<const SrtGuestWords>(SrtGuestWords{{{0x1000u, 4u}}, {1u, 2u, 3u, 4u}});
    const auto twinBlock = std::make_shared<const SrtGuestWords>(SrtGuestWords{{{0x1000u, 4u}}, {1u, 2u, 3u, 4u}});
    const std::vector<std::shared_ptr<const SrtGuestWords>> identityBlocks{identityBlock, twinBlock};
    const auto identityBytes = std::as_bytes(std::span(identityBlock->words));
    const std::vector<std::uint32_t> copiedWords(identityBlock->words);
    std::vector<AgcDriver::SharedWords> whole, tail, twin, copied, misaligned, nothing;
    AgcDriver::AppendSharedWords(whole, 0u, 1u, identityBytes, identityBlocks);
    AgcDriver::AppendSharedWords(tail, 0u, 2u, identityBytes.subspan(4u), identityBlocks);
    AgcDriver::AppendSharedWords(twin, 0u, 1u, std::as_bytes(std::span(twinBlock->words)), identityBlocks);
    AgcDriver::AppendSharedWords(copied, 0u, 1u, std::as_bytes(std::span(copiedWords)), identityBlocks);
    AgcDriver::AppendSharedWords(misaligned, 0u, 1u, identityBytes.subspan(2u, 8u), identityBlocks);
    AgcDriver::AppendSharedWords(nothing, 0u, 1u, identityBytes.subspan(0u, 0u), identityBlocks);
    require(whole.size() == 1u && whole[0].first == 1u && whole[0].count == 4u && whole[0].offset == 0u && tail.size() == 1u && tail[0].first == 2u && tail[0].count == 3u && tail[0].offset == 1u && twin.size() == 1u && twin[0].block.lock() == twinBlock && copied.empty() && misaligned.empty() && nothing.empty(), "draw cache identity: words were credited to a block they were not copied from");
    std::vector<AgcDriver::SharedWords> joined, gapped, crossed, skipped;
    AgcDriver::AppendSharedWords(joined, 0u, 1u, identityBytes.subspan(0u, 8u), identityBlocks);
    AgcDriver::AppendSharedWords(joined, 1u, 3u, identityBytes.subspan(8u), identityBlocks);
    AgcDriver::AppendSharedWords(gapped, 0u, 1u, identityBytes.subspan(0u, 4u), identityBlocks);
    AgcDriver::AppendSharedWords(gapped, 1u, 3u, identityBytes.subspan(4u, 4u), identityBlocks);
    AgcDriver::AppendSharedWords(gapped, 2u, 4u, identityBytes.subspan(12u, 4u), identityBlocks);
    AgcDriver::AppendSharedWords(crossed, 0u, 1u, identityBytes.subspan(0u, 4u), identityBlocks);
    AgcDriver::AppendSharedWords(crossed, 1u, 2u, std::as_bytes(std::span(twinBlock->words)).subspan(4u, 4u), identityBlocks);
    AgcDriver::AppendSharedWords(skipped, 0u, 1u, identityBytes.subspan(0u, 8u), identityBlocks);
    AgcDriver::AppendSharedWords(skipped, 2u, 3u, identityBytes.subspan(8u), identityBlocks);
    require(skipped.size() == 2u && skipped[0].region == 0u && skipped[0].regions == 1u && skipped[1].region == 2u && skipped[1].regions == 1u, "draw cache identity: words of regions that are not consecutive were joined");
    require(joined.size() == 1u && joined[0].first == 1u && joined[0].count == 4u && joined[0].offset == 0u && joined[0].region == 0u && joined[0].regions == 2u && gapped.size() == 3u && gapped[1].first == 3u && gapped[1].offset == 1u && gapped[2].first == 4u && gapped[2].offset == 3u && crossed.size() == 2u && crossed[1].block.lock() == twinBlock, "draw cache identity: consecutive words of one block were not joined, or words of two places were");
    const std::vector<std::uint32_t> leftWords{9u, 1u, 2u, 3u, 4u};
    auto rightWords = leftWords;
    std::size_t identityCompared = 0;
    require(AgcDriver::SameCapturedWords(leftWords, whole, rightWords, whole, &identityCompared) && identityCompared == 1u, "draw cache identity: words copied from one block were compared");
    require(AgcDriver::SameCapturedWords(leftWords, whole, rightWords, twin, &identityCompared) && identityCompared == leftWords.size(), "draw cache identity: equal words from two blocks did not match by their words");
    rightWords[0] = 8u;
    require(!AgcDriver::SameCapturedWords(leftWords, whole, rightWords, whole, &identityCompared) && identityCompared == 1u, "draw cache identity: a shared block hid a difference outside it");
    rightWords = {9u, 1u, 2u, 3u, 5u};
    const std::vector<AgcDriver::SharedWords> unowned{{1u, 4u, 0u, {}}};
    const std::vector<AgcDriver::SharedWords> shifted{{1u, 4u, 1u, identityBlock}};
    const std::vector<AgcDriver::SharedWords> beyond{{3u, 4u, 0u, identityBlock}};
    auto doomed = std::make_shared<const SrtGuestWords>(SrtGuestWords{{{0x2000u, 4u}}, {1u, 2u, 3u, 4u}});
    std::vector<AgcDriver::SharedWords> dead;
    AgcDriver::AppendSharedWords(dead, 0u, 1u, std::as_bytes(std::span(doomed->words)), std::span<const std::shared_ptr<const SrtGuestWords>>(&doomed, 1u));
    doomed.reset();
    require(!AgcDriver::SameCapturedWords(leftWords, whole, rightWords, twin, &identityCompared) && identityCompared == leftWords.size(), "draw cache identity: different words from two blocks matched");
    require(!AgcDriver::SameCapturedWords(leftWords, unowned, rightWords, unowned, &identityCompared) && identityCompared == leftWords.size(), "draw cache identity: runs without a block vouched for words");
    require(!AgcDriver::SameCapturedWords(leftWords, whole, rightWords, shifted, &identityCompared) && identityCompared == leftWords.size(), "draw cache identity: runs at other offsets of one block vouched for words");
    require(!AgcDriver::SameCapturedWords(leftWords, beyond, rightWords, beyond, &identityCompared) && identityCompared == leftWords.size(), "draw cache identity: a run past the words vouched for them");
    require(dead.size() == 1u && dead[0].block.expired() && !AgcDriver::SameCapturedWords(leftWords, dead, rightWords, dead, &identityCompared) && identityCompared == leftWords.size(), "draw cache identity: a released block vouched for words");
    require(!AgcDriver::SameCapturedWords(leftWords, whole, std::vector<std::uint32_t>(leftWords.begin(), leftWords.end() - 1), {}, &identityCompared) && identityCompared == 0u, "draw cache identity: captures of different sizes matched");

    const auto compiled = CompileTable(request);
    const auto& map = TableMap(*compiled.result);
    const auto codes = TableCodes(map, 0);
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && codes.size() == Records, "table snapshot reuse: the reused table has no image table binding");
    for (const auto key : {0u, 77u, 1234u, 2048u, 3071u, 4095u}) {
        const auto element = map.at(codes.at(key) + Abi::ImageRecordElement);
        require((codes[key] & Abi::PoisonFlag) == 0u && std::equal(heap + static_cast<std::size_t>(key) * 8u, heap + static_cast<std::size_t>(key) * 8u + 8u, images->guestDescriptor.begin() + static_cast<std::ptrdiff_t>(element) * 8), ("table snapshot reuse: key " + std::to_string(key) + " selects the wrong record of a reused table").c_str());
    }

    const auto resolvedBefore = ResourceMaterializer::TableReuse();
    alignas(16) static std::array<std::uint32_t, 4> otherOutput{};
    const auto otherOutputV = TableV(otherOutput.data(), 0u, static_cast<std::uint32_t>(sizeof(otherOutput)));
    const std::array<std::uint32_t, 4> outputV{guest.srt[12], guest.srt[13], guest.srt[14], guest.srt[15]};
    std::copy(otherOutputV.begin(), otherOutputV.end(), guest.srt.begin() + 12);
    const auto otherBuffer = CompileTable(request);
    std::copy(outputV.begin(), outputV.end(), guest.srt.begin() + 12);
    const auto resolvedShared = ResourceMaterializer::TableReuse();
    const bool resolves = std::getenv("APS5_NO_MASKED_RESULT_MEMO") != nullptr || std::getenv("APS5_VERIFY_MASKED_RESULT_MEMO") != nullptr;
    require(otherBuffer.result != compiled.result && otherBuffer.result->variantId == compiled.result->variantId && resolvedShared.resolutionsShared - resolvedBefore.resolutionsShared == (resolves ? 1u : 0u) && resolvedShared.resolutions == resolvedBefore.resolutions && TableMap(*otherBuffer.result) == map && TableBinding(*otherBuffer.result, DescriptorRole::ImageTable)->guestDescriptor == images->guestDescriptor, "table snapshot reuse: a result for another buffer did not reuse the unchanged table's resolution");
    const auto* otherBuffers = TableBinding(*otherBuffer.result, DescriptorRole::GuestBuffers);
    require(otherBuffers != nullptr && std::search(otherBuffers->guestDescriptor.begin(), otherBuffers->guestDescriptor.end(), otherOutputV.begin(), otherOutputV.end()) != otherBuffers->guestDescriptor.end(), "table snapshot reuse: a result for another buffer does not hold that buffer's descriptor");
    const auto storedWord = heap[5u * 8u];
    heap[5u * 8u] = heap[6u * 8u];
    const auto restoredMap = map;
    const auto storedTable = CompileTable(request);
    heap[5u * 8u] = storedWord;
    const auto resolvedStored = ResourceMaterializer::TableReuse();
    require(resolvedStored.resolutionsShared == resolvedShared.resolutionsShared && resolvedStored.resolutions - resolvedShared.resolutions == 1u && TableMap(*storedTable.result) != restoredMap, "table snapshot reuse: a table changed by a CPU store took the resolution of its old words");
    const auto& storedMap = TableMap(*storedTable.result);
    const auto storedCodes = TableCodes(storedMap, 0);
    const auto* storedImages = TableBinding(*storedTable.result, DescriptorRole::ImageTable);
    const auto storedElement = storedMap.at(storedCodes.at(5u) + Abi::ImageRecordElement);
    require(storedImages != nullptr && std::equal(heap + 6u * 8u, heap + 6u * 8u + 1u, storedImages->guestDescriptor.begin() + static_cast<std::ptrdiff_t>(storedElement) * 8) && std::equal(heap + 5u * 8u + 1u, heap + 5u * 8u + 8u, storedImages->guestDescriptor.begin() + static_cast<std::ptrdiff_t>(storedElement) * 8 + 1), "table snapshot reuse: the resolution after a CPU store does not hold the stored record");

    constexpr std::uint32_t SmallRecords = 64;
    std::vector<std::vector<std::uint32_t>> smallStorage(17, std::vector<std::uint32_t>(SmallRecords * 8u + 1024u));
    std::vector<TableGuest> smallGuests(smallStorage.size());
    std::vector<std::array<std::uint32_t, 2>> smallUserData(smallStorage.size());
    std::vector<RecompileRequest> smallRequests;
    std::vector<std::uint32_t*> smallTables;
    for (std::size_t index = 0; index < smallStorage.size(); index++) {
        auto* table = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(smallStorage[index].data()) + 4095u) & ~std::uintptr_t{4095});
        fill(table, SmallRecords);
        FillTableSrt(smallGuests[index], table, 32u, SmallRecords);
        smallTables.push_back(table);
        smallUserData[index] = TableUserData(smallGuests[index]);
        smallRequests.push_back(TableRequest(TableWholeCode, smallUserData[index]));
    }
    const auto smallBytes = static_cast<std::size_t>(SmallRecords) * 32u;
    const auto first = capture(smallRequests[0], smallTables[0], smallBytes, nullptr);
    const auto again = capture(smallRequests[0], smallTables[0], smallBytes, nullptr);
    require(first.reuse.compared == 0u && again.reuse.unwritten == 0u && again.reuse.compared == 1u && heldRecords(again, smallTables[0], SmallRecords), "table snapshot reuse: an unchanged table outside the write watch was not compared and reused");
    smallTables[0][5u * 8u] ^= 0x10u;
    const auto unwatchedStore = capture(smallRequests[0], smallTables[0], smallBytes, nullptr);
    require(unwatchedStore.reuse.compared == 0u && heldRecords(unwatchedStore, smallTables[0], SmallRecords), "table snapshot reuse: a store into a table outside the write watch was not seen");
    for (std::size_t index = 1; index < smallStorage.size(); index++) static_cast<void>(capture(smallRequests[index], smallTables[index], smallBytes, nullptr));
    const auto evicted = capture(smallRequests[0], smallTables[0], smallBytes, nullptr);
    const auto kept = capture(smallRequests.back(), smallTables.back(), smallBytes, nullptr);
    require(evicted.reuse.unwritten == 0u && evicted.reuse.compared == 0u && kept.reuse.compared == 1u, "table snapshot reuse: the memo kept more than 16 columns");

    constexpr std::uint32_t PagedRecords = 512;
    constexpr std::size_t PagedBytes = PagedRecords * 32u;
    auto* paged = AllocateWatchedTable(PagedBytes);
    fill(paged, PagedRecords);
    alignas(4096) static TableGuest pagedGuest;
    FillTableSrt(pagedGuest, paged, 32u, PagedRecords);
    const auto pagedUserData = TableUserData(pagedGuest);
    const auto pagedRequest = TableRequest(TableWholeCode, pagedUserData);
    const auto pagedFresh = capture(pagedRequest, paged, PagedBytes, nullptr);
    const auto pagedReused = capture(pagedRequest, paged, PagedBytes, nullptr);
    require(pagedFresh.reuse.columns == 1u && sharesColumn(pagedReused, pagedFresh) && heldRecords(pagedReused, paged, PagedRecords), "table snapshot reuse: an unchanged paged table did not share its column");
    const auto hidden = reinterpret_cast<std::uintptr_t>(paged) + 4096u;
    ProtectTablePage(hidden, 0x01u);
    const auto pagedUnmapped = capture(pagedRequest, paged, PagedBytes, nullptr);
    ProtectTablePage(hidden, 0x04u);
    const auto unmappedCode = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped));
    const auto& hiddenCodes = pagedUnmapped.tables.tables.at(0).codes;
    require(pagedUnmapped.reuse.unwritten == 0u && pagedUnmapped.reuse.shared == 0u && pagedUnmapped.reuse.columns == 1u && pagedUnmapped.supplied.shared == 0u && hiddenCodes.at(127u) != unmappedCode && hiddenCodes.at(128u) == unmappedCode && hiddenCodes.at(255u) == unmappedCode && hiddenCodes.at(256u) != unmappedCode, "table snapshot reuse: the records of an unmapped page were not read again as Unmapped poison");
    require(heldRecords(pagedReused, paged, PagedRecords) && sharesColumn(pagedReused, pagedFresh), "table snapshot reuse: unmapping a page changed the column read before it");
    const auto pagedBack = capture(pagedRequest, paged, PagedBytes, nullptr);
    require(pagedBack.reuse.unwritten == 0u && pagedBack.reuse.compared == 1u && sharesColumn(pagedBack, pagedFresh) && heldRecords(pagedBack, paged, PagedRecords), "table snapshot reuse: the column read before a page was unmapped was not compared and shared once the page came back");
    static_cast<void>(capture(pagedRequest, paged, PagedBytes, nullptr));
    GuestArena::GuestArenaReset_nid_postfix(paged, PagedBytes);
    GuestArena::GuestArenaCommit_nid_postfix(paged, PagedBytes, 0x04u, PagedBytes);
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(paged), PagedBytes);
    const auto pagedRemapped = capture(pagedRequest, paged, PagedBytes, nullptr);
    require(pagedRemapped.reuse.unwritten == 0u && pagedRemapped.reuse.shared == 0u && pagedRemapped.reuse.columns == 1u && !sharesColumn(pagedRemapped, pagedFresh) && heldRecords(pagedRemapped, paged, PagedRecords), "table snapshot reuse: a table mapped again with new contents was not read again");
    std::cout << "table snapshot reuse " << (watched ? "with" : "without") << " write watching passed\n";
}

std::atomic<int> tableRacePhase{0};
std::uint64_t tableRaceBegin = 0;
std::uint64_t tableRaceBytes = 0;

AgcDriver::ShaderMemory::PendingWrite RacingTableWrite(std::uint64_t address, std::size_t bytes, std::span<std::byte>) {
    if (tableRaceBytes != 0u && address == tableRaceBegin && bytes >= tableRaceBytes) {
        tableRaceBytes = 0;
        tableRacePhase.store(1, std::memory_order_release);
        while (tableRacePhase.load(std::memory_order_acquire) != 2) std::this_thread::yield();
    }
    return AgcDriver::ShaderMemory::PendingWrite::None;
}

void verifyTableStampRace() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Records = 4096;
    constexpr std::uint32_t Stride = 48;
    constexpr std::size_t Bytes = static_cast<std::size_t>(Records) * Stride;
    constexpr std::size_t RecordWords = Stride / 4u;
    const auto texture = [](std::uint32_t index) { return TableT(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x100000000ull + index * 256ull)), TableFormat8888UNorm, TableType2D, 1u + index % 5u); };
    auto* heap = AllocateWatchedTable(Bytes);
    for (std::uint32_t record = 0; record < Records; record++) {
        const auto words = texture(record % 1024u);
        std::copy(words.begin(), words.end(), heap + record * RecordWords);
        std::fill_n(heap + record * RecordWords + 8u, 4u, 0x5a5a5a5au);
    }
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(heap));
    if (AgcDriver::GuestMemory::CollectWrites(address, Bytes) == 0) {
        std::cout << "table stamp race without write watching skipped\n";
        return;
    }
    alignas(4096) static TableGuest guest;
    FillTableSrt(guest, heap, Stride, Records);
    const auto userData = TableUserData(guest);
    auto wideCode = TableWholeCode;
    std::replace(wideCode.begin(), wideCode.end(), 0x8f108510u, 0x9310b010u);
    const auto request = TableRequest(wideCode, userData);
    const auto capture = [&](AgcDriver::ShaderMemory::PendingWriteQuery pending) {
        const auto before = ResourceMaterializer::TableReuse();
        AgcDriver::ShaderMemory memory({}, pending);
        static_cast<void>(memory.Capture(request));
        const auto after = ResourceMaterializer::TableReuse();
        return ImageTableReuseCounts{after.unwritten - before.unwritten, after.compared - before.compared, after.wordsRead - before.wordsRead, after.shared - before.shared, after.columns - before.columns};
    };
    tableRacePhase.store(0, std::memory_order_relaxed);
    tableRaceBegin = address;
    tableRaceBytes = Bytes - 16u;
    std::thread writer([&] {
        while (tableRacePhase.load(std::memory_order_acquire) != 1) std::this_thread::yield();
        AgcDriver::GuestMemory::MarkWritten(address + 5u * Stride, 32u);
        tableRacePhase.store(2, std::memory_order_release);
    });
    const auto first = capture(&RacingTableWrite);
    writer.join();
    require(tableRacePhase.load(std::memory_order_relaxed) == 2 && first.columns == 1u, "table stamp race: the racing store did not run while the table was stamped");
    const auto next = capture(nullptr);
    require(next.unwritten == 0u && next.compared == 1u, "table stamp race: a GPU write stamped while the table was stamped, before it was noted pending, was hidden by the table's stamp");
    std::cout << "table stamp race with write watching passed\n";
}

void verifyTableVouch() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Records = 4096;
    constexpr std::uint32_t Stride = 48;
    constexpr std::size_t Bytes = static_cast<std::size_t>(Records) * Stride;
    constexpr std::size_t RecordWords = Stride / 4u;
    const auto texture = [](std::uint32_t index) { return TableT(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x100000000ull + index * 256ull)), TableFormat8888UNorm, TableType2D, 1u + index % 5u); };
    const auto fill = [&](std::uint32_t* heap) {
        for (std::uint32_t record = 0; record < Records; record++) {
            const auto words = texture(record % 1024u);
            std::copy(words.begin(), words.end(), heap + record * RecordWords);
            std::fill_n(heap + record * RecordWords + 8u, 4u, 0x5a5a5a5au);
        }
    };
    struct Captured {
        ImageTableReuseCounts reuse;
        std::uint64_t calls = 0;
        std::vector<std::uint64_t> addresses;
        std::vector<std::size_t> counts;
        std::vector<std::uint32_t> words;
        std::vector<AgcDriver::SharedWords> shared;
        std::vector<std::shared_ptr<const SrtGuestWords>> blocks;
    };
    const auto capture = [](const RecompileRequest& request, AgcDriver::ShaderMemory::PendingWriteQuery pending) {
        Captured captured;
        const auto before = ResourceMaterializer::TableReuse();
        const auto calls = AgcDriver::ShaderMemory::Vouches().calls;
        AgcDriver::ShaderMemory memory({}, pending);
        static_cast<void>(memory.Capture(request));
        captured.calls = AgcDriver::ShaderMemory::Vouches().calls - calls;
        captured.blocks = memory.SuppliedBlocks();
        for (const auto& region : memory.TakeRecentRegions()) {
            const auto offset = captured.words.size();
            captured.addresses.push_back(region.guestAddress);
            captured.counts.push_back(region.bytes.size() / sizeof(std::uint32_t));
            captured.words.resize(offset + region.bytes.size() / sizeof(std::uint32_t));
            std::memcpy(captured.words.data() + offset, region.bytes.data(), region.bytes.size());
            AgcDriver::AppendSharedWords(captured.shared, captured.addresses.size() - 1u, offset, region.bytes, captured.blocks);
        }
        const auto after = ResourceMaterializer::TableReuse();
        captured.reuse = {after.unwritten - before.unwritten, after.compared - before.compared, after.wordsRead - before.wordsRead, after.shared - before.shared, after.columns - before.columns};
        return captured;
    };
    const auto vouch = [](const Captured& captured, AgcDriver::ShaderMemory::PendingWriteQuery pending, std::vector<std::uint8_t>& vouched) {
        std::vector<AgcDriver::VouchedRegions> ranges;
        const auto words = AgcDriver::VouchSharedRegions(captured.shared, captured.addresses.size(), pending, ranges);
        vouched.assign(captured.addresses.size(), 0u);
        std::size_t covered = 0;
        for (const auto& range : ranges) {
            std::fill(vouched.begin() + static_cast<std::ptrdiff_t>(range.begin), vouched.begin() + static_cast<std::ptrdiff_t>(range.end), std::uint8_t{1});
            for (auto index = range.begin; index < range.end; index++) covered += captured.counts[index];
        }
        return covered == words ? words : std::size_t{0};
    };

    auto* heap = AllocateWatchedTable(Bytes);
    fill(heap);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(heap));
    const bool watched = AgcDriver::GuestMemory::CollectWrites(address, Bytes) != 0;
    alignas(4096) static TableGuest guest;
    FillTableSrt(guest, heap, Stride, Records);
    const auto userData = TableUserData(guest);
    auto wideCode = TableWholeCode;
    std::replace(wideCode.begin(), wideCode.end(), 0x8f108510u, 0x9310b010u);
    const auto request = TableRequest(wideCode, userData);
    const auto tableWords = static_cast<std::size_t>(Records) * 8u;
    const auto onlyTable = [&](const Captured& captured, const std::vector<std::uint8_t>& vouched) {
        if (vouched.size() != captured.addresses.size()) return false;
        for (std::size_t index = 0; index < vouched.size(); index++) {
            if ((captured.addresses[index] >= address && captured.addresses[index] < address + Bytes) != (vouched[index] != 0u)) return false;
        }
        return true;
    };

    const auto fresh = capture(request, nullptr);
    require(fresh.reuse.columns == 1u && fresh.reuse.wordsRead == tableWords, "table vouch: the first snapshot did not read the strided table once");
    const auto unchanged = capture(request, nullptr);
    if (watched) {
        require(unchanged.reuse.unwritten == 1u && unchanged.reuse.wordsRead == 0u, "table vouch: an unwritten strided table was read again");
        require(unchanged.calls == 1u, ("table vouch: an unwritten table of " + std::to_string(Records) + " strided records was vouched for in " + std::to_string(unchanged.calls) + " calls instead of one").c_str());
        require(unchanged.blocks.size() == 1u && unchanged.blocks[0]->ordered && unchanged.blocks[0]->runs.size() == Records && unchanged.blocks[0]->low == address && unchanged.blocks[0]->high == address + static_cast<std::uint64_t>(Records - 1u) * Stride + 32u, "table vouch: the strided table's words were not kept as one ordered block over the span of its records");
    } else {
        require(unchanged.reuse.compared == 1u && unchanged.calls == 0u, "table vouch: an unchanged unwatched strided table was not compared");
    }

    std::vector<std::uint8_t> vouched;
    const auto vouchesBefore = AgcDriver::ShaderMemory::Vouches();
    const auto unchangedWords = vouch(unchanged, nullptr, vouched);
    const auto vouchesAfter = AgcDriver::ShaderMemory::Vouches();
    if (watched) {
        require(unchangedWords == tableWords && onlyTable(unchanged, vouched), "table vouch: the regions of an unchanged shared table were not vouched for, or other regions were");
        require(vouchesAfter.calls - vouchesBefore.calls == 1u && vouchesAfter.blocks - vouchesBefore.blocks == 1u && vouchesAfter.refused == vouchesBefore.refused && vouchesAfter.words - vouchesBefore.words == tableWords, "table vouch: one shared block was not vouched for once");
    } else {
        require(unchangedWords == 0u && unchanged.shared.empty() && std::none_of(vouched.begin(), vouched.end(), [](std::uint8_t flag) { return flag != 0u; }), "table vouch: an unwatched table's regions were vouched for");
    }
    if (!watched) {
        std::cout << "table vouch without write watching passed\n";
        return;
    }
    const auto refused = [&](const std::string& what, AgcDriver::ShaderMemory::PendingWriteQuery pending) {
        const auto before = AgcDriver::ShaderMemory::Vouches();
        const auto words = vouch(unchanged, pending, vouched);
        const auto after = AgcDriver::ShaderMemory::Vouches();
        require(words == 0u && std::none_of(vouched.begin(), vouched.end(), [](std::uint8_t flag) { return flag != 0u; }) && after.refused - before.refused == 1u, ("table vouch: " + what + ": the shared table was still vouched for").c_str());
    };

    pendingTableKnown = false;
    pendingTableWord = address + 100u * Stride;
    refused("a pending GPU write into a record", &PendingTableWrite);
    pendingTableWord = address + 100u * Stride + 40u;
    refused("a pending GPU write between two records", &PendingTableWrite);
    pendingTableWord = address + Bytes + 4096u;
    require(vouch(unchanged, &PendingTableWrite, vouched) == tableWords, "table vouch: a pending GPU write outside the table refused it");
    pendingTableWord = 0;

    constexpr std::size_t UnrelatedBytes = 65536;
    void* unrelated = ::operator new(UnrelatedBytes, std::align_val_t{UnrelatedBytes});
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(unrelated, UnrelatedBytes, true, true, false);
    }
    refused("a changed guest mapping", nullptr);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(unrelated);
    }
    ::operator delete(unrelated, std::align_val_t{UnrelatedBytes});

    const auto rewrite = [&](const std::string& what, const std::function<void()>& store, bool gap) {
        auto held = capture(request, nullptr);
        if (held.reuse.unwritten == 0u) held = capture(request, nullptr);
        std::vector<std::uint8_t> heldVouched;
        require(held.reuse.unwritten == 1u && vouch(held, nullptr, heldVouched) == tableWords, ("table vouch: " + what + ": the table was not vouched for before the store").c_str());
        store();
        const auto before = AgcDriver::ShaderMemory::Vouches();
        require(vouch(held, nullptr, heldVouched) == 0u && AgcDriver::ShaderMemory::Vouches().refused - before.refused == 1u, ("table vouch: " + what + ": the table captured before the store was still vouched for").c_str());
        const auto after = capture(request, nullptr);
        if (gap) {
            require(after.reuse.unwritten == 0u && after.reuse.compared == 1u && after.reuse.columns == 0u, ("table vouch: " + what + ": the table was not compared and kept").c_str());
            require(vouch(held, nullptr, heldVouched) == tableWords, ("table vouch: " + what + ": a capture sharing the compared table was not vouched for again").c_str());
            require(capture(request, nullptr).reuse.unwritten == 1u, ("table vouch: " + what + ": the compared table was not reused unwritten after").c_str());
        } else {
            require(after.reuse.unwritten == 0u && after.reuse.compared == 0u && after.reuse.columns == 1u, ("table vouch: " + what + ": the table was not read again").c_str());
            require(vouch(held, nullptr, heldVouched) == 0u, ("table vouch: " + what + ": the replaced table was vouched for after it was read again").c_str());
        }
    };
    std::uint32_t replacement = 3000u;
    const auto record = [&](std::uint32_t index) { return address + static_cast<std::uint64_t>(index) * Stride; };
    rewrite("a CPU store into a record", [&] { const auto words = texture(replacement++); std::memcpy(heap + 7u * RecordWords, words.data(), sizeof(words)); }, false);
    rewrite("a driver store (CP write, DMA copy or fill)", [&] { const auto words = texture(replacement++); AgcDriver::GuestMemory::Write(record(1234u), std::as_bytes(std::span(words)), 4); }, false);
    rewrite("a GPU result stored by the driver", [&] { const auto words = texture(replacement++); AgcDriver::GuestMemory::StoreOwnBytes(record(2048u), std::as_bytes(std::span(words))); }, false);
    rewrite("a CPU store between two records", [&] { heap[300u * RecordWords + 9u] ^= 0x10u; }, true);
    rewrite("a GPU write into an import (MarkWritten)", [&] { AgcDriver::GuestMemory::MarkWritten(record(3000u) + 32u, 16u); }, true);

    {
        auto held = capture(request, nullptr);
        std::vector<std::uint8_t> heldVouched;
        require(held.reuse.unwritten == 1u && !held.blocks.empty() && vouch(held, nullptr, heldVouched) == tableWords, "table vouch: the held table was not vouched for");
        held.blocks.clear();
        heap[11u * RecordWords + 2u] ^= 0x10u;
        const auto replaced = capture(request, nullptr);
        heap[11u * RecordWords + 2u] ^= 0x10u;
        const auto before = AgcDriver::ShaderMemory::Vouches();
        require(replaced.reuse.columns == 1u && held.shared.size() == 1u && held.shared[0].block.expired() && vouch(held, nullptr, heldVouched) == 0u && AgcDriver::ShaderMemory::Vouches().calls == before.calls, "table vouch: a released table block was vouched for");
        static_cast<void>(capture(request, nullptr));
    }

    std::cout << "table vouch with write watching passed\n";
}

std::uint64_t spanQueryBegin = 0;
std::uint64_t spanQueryEnd = 0;
std::uint64_t spanQueriesToPages = 0;
bool spanQueryQueued = false;

AgcDriver::ShaderMemory::PendingWrite PageQuery(std::uint64_t address, std::size_t bytes, std::span<std::byte>) {
    if (address <= spanQueryBegin && address + bytes >= spanQueryEnd) ++spanQueriesToPages;
    return AgcDriver::ShaderMemory::PendingWrite::None;
}

AgcDriver::ShaderMemory::PendingWrite QueuedSpanQuery(std::uint64_t address, std::size_t bytes, std::span<std::byte>) {
    return spanQueryQueued && address < spanQueryEnd && spanQueryBegin < address + bytes ? AgcDriver::ShaderMemory::PendingWrite::Sync : AgcDriver::ShaderMemory::PendingWrite::None;
}

void verifyTableSpanQuery() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Records = 4096;
    constexpr std::uint32_t Stride = 48;
    constexpr std::size_t Bytes = static_cast<std::size_t>(Records) * Stride;
    constexpr std::size_t RecordWords = Stride / 4u;
    auto* heap = AllocateWatchedTable(Bytes);
    for (std::uint32_t record = 0; record < Records; record++) {
        const auto words = TableT(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x100000000ull + (record % 1024u) * 256ull)), TableFormat8888UNorm, TableType2D, 1u + record % 5u);
        std::copy(words.begin(), words.end(), heap + record * RecordWords);
        std::fill_n(heap + record * RecordWords + 8u, 4u, 0x5a5a5a5au);
    }
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(heap));
    if (AgcDriver::GuestMemory::CollectWrites(address, Bytes) == 0) {
        std::cout << "table span query without write watching skipped\n";
        return;
    }
    alignas(4096) static TableGuest guest;
    FillTableSrt(guest, heap, Stride, Records);
    auto wideCode = TableWholeCode;
    std::replace(wideCode.begin(), wideCode.end(), 0x8f108510u, 0x9310b010u);
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(wideCode, userData);
    spanQueryBegin = address;
    spanQueryEnd = address + static_cast<std::uint64_t>(Records - 1u) * Stride + 32u;
    spanQueriesToPages = 0;
    spanQueryQueued = false;
    const auto capture = [&] {
        const auto before = ResourceMaterializer::TableReuse();
        AgcDriver::ShaderMemory memory({}, &PageQuery, nullptr, nullptr, &QueuedSpanQuery);
        static_cast<void>(memory.Capture(request));
        const auto after = ResourceMaterializer::TableReuse();
        return ImageTableReuseCounts{after.unwritten - before.unwritten, after.compared - before.compared, after.wordsRead - before.wordsRead, after.shared - before.shared, after.columns - before.columns};
    };
    require(capture().columns == 1u, "table span query: the first capture did not read the table");
    require(capture().unwritten == 1u, "table span query: the unwritten table was not reused through the span query");
    spanQueryQueued = true;
    const auto queued = capture();
    require(queued.unwritten == 0u && queued.compared == 1u && queued.columns == 0u, "table span query: a write queued over the table's span did not make the capture compare the table");
    spanQueryQueued = false;
    require(capture().unwritten == 1u, "table span query: the table was not reused unwritten once the queued write was gone");
    require(spanQueriesToPages == 0u, ("table span query: " + std::to_string(spanQueriesToPages) + " whole-span checks went to the page query, which waits for queued draws").c_str());
    std::cout << "table span query passed\n";
}

void verifyTableEdgeStore() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Records = 4096;
    constexpr std::uint32_t Stride = 48;
    constexpr std::size_t Bytes = static_cast<std::size_t>(Records) * Stride;
    constexpr std::size_t Tail = 65536;
    constexpr std::size_t RecordWords = Stride / 4u;
    const auto texture = [](std::uint32_t index) { return TableT(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x100000000ull + index * 256ull)), TableFormat8888UNorm, TableType2D, 1u + index % 5u); };
    auto* heap = AllocateWatchedTable(Bytes + Tail);
    for (std::uint32_t record = 0; record < Records; record++) {
        const auto words = texture(record % 1024u);
        std::copy(words.begin(), words.end(), heap + record * RecordWords);
        std::fill_n(heap + record * RecordWords + 8u, 4u, 0x5a5a5a5au);
    }
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(heap));
    if (AgcDriver::GuestMemory::CollectWrites(address, Bytes) == 0) {
        std::cout << "table edge store without write watching skipped\n";
        return;
    }
    AgcDriver::GuestMemory::BumpCollectEpoch();
    AgcDriver::GuestMemory::Unwatch(address + Bytes, Tail);
    alignas(4096) static TableGuest guest;
    FillTableSrt(guest, heap, Stride, Records);
    auto wideCode = TableWholeCode;
    std::replace(wideCode.begin(), wideCode.end(), 0x8f108510u, 0x9310b010u);
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(wideCode, userData);
    struct Captured {
        ImageTableReuseCounts reuse;
        std::vector<std::shared_ptr<const SrtGuestWords>> blocks;
    };
    const auto capture = [&] {
        Captured captured;
        const auto before = ResourceMaterializer::TableReuse();
        AgcDriver::ShaderMemory memory({}, nullptr);
        static_cast<void>(memory.Capture(request));
        captured.blocks = memory.SuppliedBlocks();
        const auto after = ResourceMaterializer::TableReuse();
        captured.reuse = {after.unwritten - before.unwritten, after.compared - before.compared, after.wordsRead - before.wordsRead, after.shared - before.shared, after.columns - before.columns};
        return captured;
    };
    const auto vouched = [](const Captured& captured) {
        const auto& block = captured.blocks.front();
        return AgcDriver::ShaderMemory::Vouched(nullptr, block->low, block->high - block->low, LoadStamp(*block));
    };
    const auto last = address + static_cast<std::uint64_t>(Records - 1u) * Stride;
    std::uint32_t replacement = 3000u;
    const auto across = [&](const std::string& what, const std::function<void(std::span<const std::byte>)>& store) {
        auto held = capture();
        if (held.reuse.unwritten == 0u) held = capture();
        require(held.reuse.unwritten == 1u && held.blocks.size() == 1u && vouched(held), ("table edge store: " + what + ": the table was not vouched for before the store").c_str());
        std::array<std::uint32_t, RecordWords + 16u> words{};
        const auto record = texture(replacement++);
        std::copy(record.begin(), record.end(), words.begin());
        std::fill(words.begin() + 8, words.end(), 0x77777777u);
        store(std::as_bytes(std::span(words)));
        require(!vouched(held), ("table edge store: " + what + " across the heap's edge into unwatched memory did not end the heap's epoch").c_str());
        require(capture().reuse.columns == 1u, ("table edge store: " + what + ": the table was not read again").c_str());
    };
    static_cast<void>(capture());
    across("a driver store", [&](std::span<const std::byte> bytes) { AgcDriver::GuestMemory::Write(last, bytes, 4); });
    across("a GPU result stored by the driver", [&](std::span<const std::byte> bytes) { AgcDriver::GuestMemory::StoreOwnBytes(last, bytes); });
    std::cout << "table edge store passed\n";
}

void verifyImageTableScratchReuse() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::uint32_t Entries = 3000;
    static TableTexture textures[4];
    alignas(4096) static TableGuest guest;
    alignas(4096) static std::array<std::array<std::uint32_t, 8>, Entries> heap{};
    const auto place = [&](std::uint32_t shift) {
        for (std::uint32_t entry = 0; entry < Entries; entry++) heap[entry] = TableT(textures[entry % 4u].bytes.data(), TableFormat8888UNorm, TableType2D, 1u + (entry + shift) % Entries);
    };
    FillTableSrt(guest, heap.data(), 32u, Entries);
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableWholeCode, userData);
    auto staticRequest = request;
    const std::array<std::uint32_t, 2> absentResources{};
    staticRequest.context.userData = absentResources;
    staticRequest.context.memory = {};
    auto staticProgram = PrepareResourceProgram(staticRequest);
    ResourceMaterializer{}.ApplyStaticInterface(staticProgram);
    const auto& info = staticProgram.Resources().info;
    struct Resolved {
        ImageTableSnapshot snapshot;
        ResolvedImageTables tables;
        std::uint64_t allocations = 0;
    };
    const auto resolve = [&] {
        CountingMemory memory;
        SrtRuntime runtime;
        runtime.userData = request.context.userData;
        runtime.shaderBase = request.shader.codeAddress;
        runtime.userContext = &memory;
        runtime.readMemory = &CountingRead;
        runtime.readSpecializationMemory = &CountingRead;
        runtime.accessible = &CountingAccessible;
        runtime.readWords = &CountingReadWords;
        Resolved out;
        const auto before = allocationCount;
        const auto capture = CaptureResources(request, runtime);
        auto tables = ResourceMaterializer::ResolveImageTables(info, capture->snapshot);
        out.allocations = allocationCount - before;
        out.snapshot = capture->snapshot.tables;
        out.tables = std::move(tables);
        return out;
    };
    place(0u);
    static_cast<void>(resolve());
    place(7u);
    const auto warm = resolve();
    const auto fresh = std::async(std::launch::async, resolve).get();
    require(warm.snapshot == fresh.snapshot && warm.tables.map == fresh.tables.map && warm.tables.faults == fresh.tables.faults && warm.tables.poison.size() == fresh.tables.poison.size(), "image table scratch: a snapshot resolved after another one on the same thread differs from one resolved on a new thread");
    require(warm.snapshot.tables.at(0).words.size() == Entries && warm.tables.elements.size() == Entries && warm.tables.map[Abi::HeaderElementCount] == Entries, "image table scratch: the distinct records were not all indexed");
    const auto codes = TableCodes(warm.tables.map, 0);
    for (std::uint32_t entry = 0; entry < Entries; entry++) {
        const auto element = warm.tables.map.at(codes.at(entry) + Abi::ImageRecordElement);
        require(warm.tables.elements.at(element).words == fresh.tables.elements.at(element).words && warm.tables.elements[element].words[2] == heap[entry][2], "image table scratch: a record resolved to another entry's element");
    }
    require(warm.allocations < Entries / 4u, "image table scratch: snapshotting and resolving a table allocated per record");
}

void verifyImageTableBulkKeyScan() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(4096) static TableGuest guest;
    std::fill(guest.heap.begin(), guest.heap.end(), std::array<std::uint32_t, 8>{});
    guest.heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    guest.heap[1] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    guest.heap[3] = guest.heap[0];
    guest.materials = {{{0u, 1u, 0u, 0u}, {0u, 0x08000003u, 0u, 0u}, {0u, 1u, 0u, 0u}}};
    FillTableSrt(guest, guest.heap.data(), 32u, 4u);
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableMaterialCode, userData);
    const auto capture = [&](CountingMemory& memory, bool bulk) {
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
    const auto begin = reinterpret_cast<std::uintptr_t>(guest.materials.data());
    CountingMemory byWordReads{begin, begin + sizeof(guest.materials)};
    CountingMemory byRunReads = byWordReads;
    const auto byWord = capture(byWordReads, false);
    const auto byRun = capture(byRunReads, true);
    require(byWordReads.wordReads == guest.materials.size(), "image table bulk key scan: without a bulk reader the keys were not read one word each");
    require(byRunReads.wordReads == 0u, "image table bulk key scan: the keys were read word by word although a bulk reader exists");
    require(byRun->snapshot.tables == byWord->snapshot.tables, "image table bulk key scan: scanning the keys in bulk changed the snapshot");
    const auto& snapshot = byRun->snapshot.tables.tables.at(0);
    const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    require(snapshot.keys == 4u && snapshot.codes[0] == snapshot.codes[3] && snapshot.codes[0] != snapshot.codes[1] && snapshot.codes[2] == outside, "image table bulk key scan: the narrowed codes are wrong");
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

    const std::vector<std::uint32_t> invariantPhiCode{
        0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u,
        0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xbe910380u,
        0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf068011u, 0xbf850003u,
        0xf42c0502u, 0x20000000u, 0xbf820002u, 0xf42c0502u, 0x20000000u,
        0x80118111u, 0xbf0a8211u, 0xbf85fff2u, 0xbf810000u,
    };
    const auto invariantPhiRequest = TableRequest(invariantPhiCode, userData);
    const auto invariantPhiPlan = GetResourcePlan(invariantPhiRequest);
    require(invariantPhiPlan->info.images.size() == 1u && invariantPhiPlan->info.images[0].table == 0u, "image tables: an invariant descriptor Phi was not planned as one table");
    const auto invariantPhi = CompileTable(invariantPhiRequest);
    require(invariantPhi.capture->snapshot.tables.tables.size() == 1u && invariantPhi.capture->snapshot.tables.tables[0].records == 4u && TableBinding(*invariantPhi.result, DescriptorRole::ImageTable) != nullptr && TableBinding(*invariantPhi.result, DescriptorRole::ImageTable)->count == 2u && !invariantPhi.result->spirv.empty(), "image tables: an invariant descriptor Phi lost its table records");

    auto varyingPhiCode = invariantPhiCode;
    varyingPhiCode[23] = 0x20000020u;
    bool varyingRefusedOrSplit = false;
    try {
        const auto varying = GetResourcePlan(TableRequest(varyingPhiCode, userData));
        varyingRefusedOrSplit = varying->info.images.size() > 1u;
    } catch (const std::runtime_error& error) {
        varyingRefusedOrSplit = std::string_view(error.what()).find("GetImageResource dword 0 is not a valid runtime value") != std::string_view::npos;
    }
    require(varyingRefusedOrSplit, "image tables: a descriptor Phi with a differing offset became one invariant table");

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
    require(snapshot.words.at(snapshot.codes[0]).dwords == heap[0] && snapshot.words.at(snapshot.codes[1]).dwords == heap[1], "image tables: the snapshot words are wrong");
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
    const auto& tail = partialTables.tables[0].words.at(partialTables.tables[0].codes.at(3));
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
    require(plan->info.images.size() == 2u && plan->info.images[0].table == 0u && plan->info.images[1].table == 1u, "image table key union: two keyed reads of one heap were not planned as two tables");
    const auto& first = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    const auto& second = plan->descriptorSources.at(plan->info.images[1].source).tableColumn;
    require(first.has_value() && second.has_value() && first->selected && second->selected && first->heapSource == second->heapSource && first->selection != second->selection, "image table key union: the two keys do not each select a record of the one heap");
    const auto held = [](const ImageTableSnapshot& tables, std::uint32_t table) { return TableSelected(tables, table, "image table key union: a table does not hold one readable selected record"); };
    const auto compiled = CompileTable(request);
    const auto& tables = compiled.capture->snapshot.tables;
    require(tables.tables.size() == 2u && held(tables, 0u) == heap[1] && held(tables, 1u) == heap[3], "image table key union: the tables do not hold the two keys' records");
    require(TableCovered(compiled.regions, &heap[1], sizeof(heap[1])) && TableCovered(compiled.regions, &heap[3], sizeof(heap[3])) && !TableCovered(compiled.regions, &heap[0], sizeof(std::uint32_t)) && !TableCovered(compiled.regions, &heap[2], sizeof(std::uint32_t)), "image table key union: records no key selects were captured");
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && images->count == 2u, "image table key union: the binding does not hold the two distinct images of the selected records");
    guest.srt[17] = 2u;
    const auto moved = CompileTable(request);
    const auto& movedTables = moved.capture->snapshot.tables;
    require(held(movedTables, 0u) == heap[1] && held(movedTables, 1u) == heap[2] && TableCovered(moved.regions, &heap[2], sizeof(heap[2])) && !TableCovered(moved.regions, &heap[3], sizeof(std::uint32_t)) && moved.result->spirv == compiled.result->spirv, "image table key union: a changed second key did not move its record alone or changed the module");
}

void verifyImageTableLateKey() {
    using namespace ShaderRecompiler;
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
    require(plan->info.images.size() == 2u && plan->info.images[0].table == 0u && plan->info.images[1].table == 1u, "image table late key: two keyed reads of one heap were not planned as two tables");
    const auto& first = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    const auto& second = plan->descriptorSources.at(plan->info.images[1].source).tableColumn;
    require(first.has_value() && second.has_value() && first->selected && second->selected && first->selection != second->selection, "image table late key: a key read after the first image operation does not select its own record");
    const auto held = [](const ImageTableSnapshot& tables, std::uint32_t table) { return TableSelected(tables, table, "image table late key: a table does not hold one readable selected record"); };
    const auto compiled = CompileTable(request);
    const auto& tables = compiled.capture->snapshot.tables;
    require(tables.tables.size() == 2u && held(tables, 0u) == heap[1] && held(tables, 1u) == heap[3], "image table late key: the tables do not hold the two keys' records");
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && images->count == 2u, "image table late key: the binding does not hold the two distinct images of the selected records");
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
        const auto& planned = plan->info.images;
        require(planned.size() == 2u && std::all_of(planned.begin(), planned.end(), [](const ImageResource& image) { return image.table != NoTable; }), "image table unkeyed access: the two reads were not planned as two tables");
        const auto columnOf = [&](const ImageResource& image) { return *plan->descriptorSources.at(image.source).tableColumn; };
        const auto selected = std::find_if(planned.begin(), planned.end(), [&](const ImageResource& image) { return columnOf(image).selected; });
        const auto whole = std::find_if(planned.begin(), planned.end(), [&](const ImageResource& image) { return !columnOf(image).selected; });
        require(selected != planned.end() && whole != planned.end(), "image table unkeyed access: the read keyed through the SRT does not select its record beside the read keyed per lane");
        require(columnOf(*whole).unkeyed && columnOf(*whole).keyDomains.empty(), "image table unkeyed access: the read keyed per lane kept a key domain");
        const auto compiled = CompileTable(request);
        const auto& tables = compiled.capture->snapshot.tables;
        require(TableSelected(tables, selected->table, "image table unkeyed access: a table does not hold one readable selected record") == heap[2], "image table unkeyed access: the read keyed through the SRT does not hold its record");
        const auto& snapshot = tables.tables.at(whole->table);
        const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
        require(snapshot.keys == 4u && std::none_of(snapshot.codes.begin(), snapshot.codes.end(), [&](std::uint32_t entry) { return entry == outside; }), "image table unkeyed access: the table keyed per lane was narrowed");
        const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
        require(images != nullptr && images->count == 4u, "image table unkeyed access: the binding does not hold the whole domain");
    }
}

struct TableManyGuest {
    std::array<std::array<std::uint32_t, 8>, 128> heap{};
    std::array<std::uint32_t, 4> output{};
    std::array<std::uint32_t, 128> srt{};
};

constexpr std::uint32_t TableManyDirectWord = 96u;
constexpr std::uint32_t TableManyExtraSamplerWord = 112u;

std::vector<std::uint32_t> TableManyKeyCode(std::uint32_t first, std::uint32_t second, bool twoSamplers, std::uint32_t direct = 0u, bool extraSampler = false) {
    std::vector<std::uint32_t> code{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0xf4080a00u, 0xfa000040u, 0x7e100280u};
    for (std::uint32_t image = 0; image < direct; image++) code.insert(code.end(), {0xf40c0000u | ((44u + image * 8u) << 6u), 0xfa000000u | ((TableManyDirectWord + image * 8u) * 4u)});
    if (extraSampler) code.insert(code.end(), {0xf4080f00u, 0xfa000000u | (TableManyExtraSamplerWord * 4u)});
    for (std::uint32_t read = 0; read < first + second; read++) {
        code.insert(code.end(), {0xf4000400u, 0xfa000050u + read * 4u, 0x8f108510u, read < first ? 0xf42c0502u : 0xf42c0514u, 0x20000000u, 0xf09c0f08u, 0x00450400u, 0x06100908u});
        if (twoSamplers) code.insert(code.end(), {0xf09c0f08u, 0x00650400u, 0x06100908u});
    }
    for (std::uint32_t image = 0; image < direct; image++) {
        const auto resource = ((44u + image * 8u) / 4u) << 16u;
        code.insert(code.end(), {0xf09c0f08u, 0x00400400u | resource, 0x06100908u});
        if (twoSamplers) code.insert(code.end(), {0xf09c0f08u, 0x00600400u | resource, 0x06100908u});
        if (extraSampler && image + 1u == direct) code.insert(code.end(), {0xf09c0f08u, 0x01e00400u | resource, 0x06100908u});
    }
    code.insert(code.end(), {0xe0700000u, 0x80070800u, 0xbf810000u});
    return code;
}

void verifySelectedRecordsWithinLimits(bool pairs, bool direct) {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[4];
    alignas(4096) static TableManyGuest guest;
    auto& heap = guest.heap;
    for (std::uint32_t record = 0; record < heap.size(); record++) heap[record] = TableT(textures[record % 4u].bytes.data(), TableFormat8888UNorm, TableType2D, record + 1u);
    const auto heapV = TableV(heap.data(), 32u, static_cast<std::uint32_t>(heap.size()));
    const auto outputV = TableV(guest.output.data(), 0u, 16u);
    guest.srt = {};
    std::copy(heapV.begin(), heapV.end(), guest.srt.begin());
    guest.srt[8] = 0x92u;
    std::copy(outputV.begin(), outputV.end(), guest.srt.begin() + 12);
    std::copy(heapV.begin(), heapV.end(), guest.srt.begin() + 16);
    for (std::uint32_t read = 0; read + 20u < guest.srt.size(); read++) guest.srt[20u + read] = read + 2u;
    for (std::uint32_t image = 0; image < 2u; image++) {
        const auto direct = TableT(textures[image].bytes.data(), TableFormat8888UNorm, TableType2D, 200u + image);
        std::copy(direct.begin(), direct.end(), guest.srt.begin() + TableManyDirectWord + image * 8u);
    }
    guest.srt[TableManyExtraSamplerWord] = 0x91u;
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const auto columnOf = [](const IrResourcePlan& plan, const ImageResource& image) { return *plan.descriptorSources.at(image.source).tableColumn; };
    const auto selectedTables = [&](const IrResourcePlan& plan) {
        return static_cast<std::uint32_t>(std::count_if(plan.info.images.begin(), plan.info.images.end(), [&](const ImageResource& image) { return image.table != NoTable && columnOf(plan, image).selected; }));
    };
    const std::string name = pairs ? "selected records within the pair limit" : "selected records within the image limit";
    const std::uint32_t limit = pairs ? ShaderInfo::MaxSampledPairs / 2u : ShaderInfo::MaxImages;
    const auto outside = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OutsideDomain));
    if (direct) {
        const auto directImages = [&](const IrResourcePlan& plan) {
            return static_cast<std::uint32_t>(std::count_if(plan.info.images.begin(), plan.info.images.end(), [](const ImageResource& image) { return image.table == NoTable; }));
        };
        const auto reads = limit - 1u;
        const auto fittingDirectCode = TableManyKeyCode(reads, 0u, pairs, 1u);
        const auto fittingDirect = GetResourcePlan(TableRequest(fittingDirectCode, userData));
        const auto describe = [&](const IrResourcePlan& plan) {
            return std::to_string(plan.info.images.size()) + " images (" + std::to_string(selectedTables(plan)) + " selected tables, " + std::to_string(directImages(plan)) + " direct), " + std::to_string(plan.info.sampledPairs.size()) + " pairs";
        };
        require(fittingDirect->info.images.size() == limit && selectedTables(*fittingDirect) == reads && directImages(*fittingDirect) == 1u && fittingDirect->info.sampledPairs.size() == (pairs ? 2u : 1u) * limit, (name + ": selected reads and a direct image that fit lost the per-key split").c_str());
        std::cout << name << ": " << reads << " selected reads and 1 direct image" << (pairs ? ", each with 2 samplers" : "") << ": " << describe(*fittingDirect) << '\n';
        const auto pastDirectCode = TableManyKeyCode(reads, 0u, pairs, pairs ? 1u : 2u, pairs);
        const auto pastDirectRequest = TableRequest(pastDirectCode, userData);
        const auto pastDirect = GetResourcePlan(pastDirectRequest);
        const auto mergedDirect = std::find_if(pastDirect->info.images.begin(), pastDirect->info.images.end(), [](const ImageResource& image) { return image.table != NoTable; });
        require(pastDirect->info.images.size() == (pairs ? 2u : 3u) && selectedTables(*pastDirect) == 0u && directImages(*pastDirect) == (pairs ? 1u : 2u) && pastDirect->info.sampledPairs.size() == (pairs ? 5u : 3u) && mergedDirect != pastDirect->info.images.end(), (name + ": selected reads that the direct " + (pairs ? "sampler" : "images") + " push past the limit were not merged into one table").c_str());
        const auto mergedColumn = columnOf(*pastDirect, *mergedDirect);
        require(!mergedColumn.selected && mergedColumn.selection == 0u && !mergedColumn.unkeyed && mergedColumn.keyDomains.size() == reads, (name + ": the table merged beside the direct " + (pairs ? "sampler" : "images") + " is not the union of the keys").c_str());
        const auto compiledDirect = CompileTable(pastDirectRequest);
        const auto& directTables = compiledDirect.capture->snapshot.tables;
        require(directTables.tables.size() == 1u, (name + ": the snapshot beside the direct " + (pairs ? "sampler" : "images") + " does not hold one table").c_str());
        const auto& directSnapshot = directTables.tables[mergedDirect->table];
        for (const auto key : {2u, 2u + reads / 2u, 1u + reads}) {
            require(key < directSnapshot.codes.size() && (directSnapshot.codes[key] & Abi::PoisonFlag) == 0u && directSnapshot.words.at(directSnapshot.codes[key]).dwords == heap[key], (name + ": a key beside the direct " + (pairs ? "sampler" : "images") + " does not bind its record").c_str());
        }
        require(directSnapshot.codes.at(1) == outside && directSnapshot.codes.at(reads + 2u) == outside, (name + ": a key no read selects was bound beside the direct " + (pairs ? "sampler" : "images")).c_str());
        std::cout << name << ": " << reads << " selected reads and " << (pairs ? "1 direct image with a third sampler" : "2 direct images") << ": " << describe(*pastDirect) << ", the reads merged into one table with " << mergedColumn.keyDomains.size() << " key domains\n";
        return;
    }
    const auto fitting = TableManyKeyCode(limit, 0u, pairs);
    const auto atLimit = GetResourcePlan(TableRequest(fitting, userData));
    require(atLimit->info.images.size() == limit && selectedTables(*atLimit) == limit && atLimit->info.sampledPairs.size() == (pairs ? 2u : 1u) * limit, (name + ": reads that fit lost the per-key split").c_str());
    const auto past = TableManyKeyCode(limit + 1u, 0u, pairs);
    const auto request = TableRequest(past, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u && plan->info.sampledPairs.size() == (pairs ? 2u : 1u), (name + ": reads past the limit were not merged into one table").c_str());
    const auto merged = columnOf(*plan, plan->info.images[0]);
    require(!merged.selected && merged.selection == 0u && !merged.unkeyed && merged.keyDomains.size() == limit + 1u, (name + ": the merged table is not the union of the keys").c_str());
    const auto compiled = CompileTable(request);
    const auto& tables = compiled.capture->snapshot.tables;
    require(tables.tables.size() == 1u, (name + ": the snapshot does not hold one table").c_str());
    const auto& snapshot = tables.tables[0];
    for (const auto key : {2u, 2u + limit / 2u, 2u + limit}) {
        require(key < snapshot.codes.size() && (snapshot.codes[key] & Abi::PoisonFlag) == 0u && snapshot.words.at(snapshot.codes[key]).dwords == heap[key], (name + ": a key does not bind its record").c_str());
    }
    require(snapshot.codes.at(1) == outside && snapshot.codes.at(limit + 3u) == outside, (name + ": a key no read selects was bound").c_str());
    if (pairs) return;
    const auto split = TableManyKeyCode(40u, 30u, false);
    const auto twoHeaps = GetResourcePlan(TableRequest(split, userData));
    const auto unselected = std::find_if(twoHeaps->info.images.begin(), twoHeaps->info.images.end(), [&](const ImageResource& image) { return !columnOf(*twoHeaps, image).selected; });
    require(twoHeaps->info.images.size() == 31u && selectedTables(*twoHeaps) == 30u && unselected != twoHeaps->info.images.end() && columnOf(*twoHeaps, *unselected).keyDomains.size() == 40u, (name + ": the larger heap was not merged first, or the smaller one lost its split").c_str());
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
    require(column.has_value() && column->selected && column->selection < plan->descriptorSources.size(), "image table SRT key: a T# whose key is read through the SRT does not select its record");
    const auto compiled = CompileTable(request);
    const auto& tables = compiled.capture->snapshot.tables;
    const auto& snapshot = tables.tables.at(0);
    require(snapshot.records == 1u && snapshot.keys == 1u && snapshot.codes.size() == 1u && (snapshot.codes[0] & Abi::PoisonFlag) == 0u && snapshot.words.at(snapshot.codes[0]).dwords == heap[2], "image table SRT key: the snapshot does not hold the selected record alone");
    const auto* images = TableBinding(*compiled.result, DescriptorRole::ImageTable);
    require(images != nullptr && images->count == 1u, "image table SRT key: the binding holds other entries than the selected one");
    require(TableCovered(compiled.regions, &guest.srt[16], sizeof(std::uint32_t)) && TableCovered(compiled.regions, &heap[2], sizeof(heap[2])), "image table SRT key: the key or the selected record was not captured");
    require(!TableCovered(compiled.regions, &heap[0], sizeof(std::uint32_t)) && !TableCovered(compiled.regions, &heap[1], sizeof(std::uint32_t)) && !TableCovered(compiled.regions, &heap[3], sizeof(std::uint32_t)), "image table SRT key: records the key does not select were captured");
    guest.srt[16] = 3u;
    const auto moved = CompileTable(request);
    const auto& movedTables = moved.capture->snapshot.tables;
    require(movedTables.tables.at(0).words.at(movedTables.tables.at(0).codes.at(0)).dwords == heap[3] && TableCovered(moved.regions, &heap[3], sizeof(heap[3])) && !TableCovered(moved.regions, &heap[2], sizeof(std::uint32_t)) && moved.result->spirv == compiled.result->spirv && moved.key == compiled.key, "image table SRT key: a changed key did not move the selected record alone");
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

void verifyImageTableFirstLaneScale() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(4096) static TableGuest guest;
    auto& heap = guest.heap;
    std::fill(heap.begin(), heap.end(), std::array<std::uint32_t, 8>{});
    heap[0] = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    heap[3] = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    heap[9] = heap[0];
    FillTableSrt(guest, heap.data(), 32u, 12u);
    const auto userData = TableUserData(guest);
    const auto scaled = [](std::initializer_list<std::uint32_t> offset) {
        std::vector<std::uint32_t> code(TableWholeCode);
        const auto first = std::find(code.begin(), code.end(), 0x7e200500u);
        require(first != code.end() && first[1] == 0x8f108510u, "image table first-lane scale: the program has no v_readfirstlane_b32 and s_lshl_b32");
        code.insert(code.erase(first, first + 2), offset);
        return code;
    };
    const auto code = scaled({0x34020085u, 0xd7460001u, 0x04050301u, 0x7e200501u});
    const auto request = TableRequest(code, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u && plan->info.images[0].tableOperation == TableOperation::Supported, "image table first-lane scale: the T# was not planned as a table");
    const auto& column = plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.has_value() && column->stride == 96u && column->addend == 0u && column->offset == 0u && column->dwordCount == 8u && column->unkeyed, "image table first-lane scale: the column is wrong");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    require(snapshot.records == 4u && snapshot.keys == 4u && snapshot.fault == 0u, "image table first-lane scale: the snapshot does not hold the four 96-byte records");
    const auto codes = TableCodes(TableMap(*compiled.result), 0);
    require(codes.size() == 4u && codes[0] == codes[3] && codes[2] == Abi::NullCode && codes[1] != codes[0] && (codes[0] & Abi::PoisonFlag) == 0u && (codes[1] & Abi::PoisonFlag) == 0u, "image table first-lane scale: the table map is wrong");
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
    require(storePlan.info.images.size() == 1u && storePlan.info.images[0].table == 0u && storePlan.info.images[0].tableOperation == TableOperation::Supported, "image table registration: a table T# feeding image_store was not a supported table operation");
    const auto& storeMap = TableMap(*store.result);
    const auto storeFault = storeMap[Abi::TableHeader(0) + Abi::TableFault];
    const auto* storeTable = TableBinding(*store.result, DescriptorRole::ImageStorageTable);
    require((storeFault & Abi::PoisonFlag) == 0u && storeTable != nullptr && storeTable->count != 0u && std::all_of(storeTable->imageWritten.begin(), storeTable->imageWritten.end(), [](bool written) { return written; }), "image table registration: the store's table was not bound as written storage elements");
    require(TableBinding(*store.result, DescriptorRole::ImageTable) == nullptr || TableBinding(*store.result, DescriptorRole::ImageTable)->count == 0u, "image table registration: a store-only table bound sampled elements");
    auto atomicCode = storeCode;
    atomicCode[8] = 0xf0440108u;
    const auto atomic = CompileTable(TableRequest(atomicCode, userData));
    const auto& atomicPlan = *atomic.capture->plan;
    require(atomicPlan.info.images.size() == 1u && atomicPlan.info.images[0].table == 0u && atomicPlan.info.images[0].tableOperation == TableOperation::Supported, "image table registration: a table T# feeding image_atomic_add was not a supported table operation");
    const auto& atomicMap = TableMap(*atomic.result);
    const auto atomicFault = atomicMap[Abi::TableHeader(0) + Abi::TableFault];
    const auto* atomicTable = TableBinding(*atomic.result, DescriptorRole::ImageStorageTable);
    require((atomicFault & Abi::PoisonFlag) == 0u && atomicTable != nullptr && atomicTable->count != 0u && atomicTable->imageAtomic.size() == atomicTable->count && std::all_of(atomicTable->imageWritten.begin(), atomicTable->imageWritten.end(), [](bool written) { return written; }) && std::all_of(atomicTable->imageAtomic.begin(), atomicTable->imageAtomic.end(), [](bool atomic) { return atomic; }), "image table registration: the image_atomic_add table was not bound as atomic storage elements");
    require(TableBinding(*atomic.result, DescriptorRole::ImageTable) == nullptr || TableBinding(*atomic.result, DescriptorRole::ImageTable)->count == 0u, "image table registration: an atomic-only table bound sampled elements");
    auto atomic64Code = storeCode;
    atomic64Code[8] = 0xf0440308u;
    guest.heap[0] = TableT(texture.bytes.data(), TableFormat32_32UInt, TableType2D);
    static constexpr std::array<std::uint32_t, 5> int64ImageCapabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityInt64ImageEXT};
    auto atomic64Request = TableRequest(atomic64Code, userData);
    atomic64Request.target.supportedCapabilities = int64ImageCapabilities;
    const auto atomic64 = CompileTable(atomic64Request);
    const auto& atomic64Plan = *atomic64.capture->plan;
    require(atomic64Plan.info.images.size() == 1u && atomic64Plan.info.images[0].table == 0u && atomic64Plan.info.images[0].tableOperation == TableOperation::Supported, "image table registration: a table T# feeding image_atomic64_add was not a supported table operation");
    const auto& atomic64Map = TableMap(*atomic64.result);
    const auto atomic64Fault = atomic64Map[Abi::TableHeader(0) + Abi::TableFault];
    const auto* atomic64Table = TableBinding(*atomic64.result, DescriptorRole::ImageStorageTable);
    require((atomic64Fault & Abi::PoisonFlag) == 0u && atomic64Table != nullptr && atomic64Table->count != 0u && atomic64Table->imageAtomic64.size() == atomic64Table->count && std::all_of(atomic64Table->imageAtomic64.begin(), atomic64Table->imageAtomic64.end(), [](bool atomic) { return atomic; }), "image table registration: the image_atomic64_add table was not bound as 64-bit atomic storage elements");
    const std::array<std::pair<std::uint32_t, const char*>, 2> unsupportedStores{{{0xf0240f08u, "image_store_mip"}, {0xf0280f08u, "image_store_pck"}}};
    for (const auto& [instruction, name] : unsupportedStores) {
        auto code = storeCode;
        code[8] = instruction;
        const auto unsupported = CompileTable(TableRequest(code, userData));
        const auto& plan = *unsupported.capture->plan;
        require(plan.info.images.size() == 1u && plan.info.images[0].table == 0u && (plan.info.images[0].tableOperation == TableOperation::Unsupported || plan.info.images[0].mipMode == ImageMipMode::DynamicStorage), (std::string("image table registration: a table T# feeding ") + name + " was not planned as an unsupported or dynamic-mip table store").c_str());
        const auto fault = TableMap(*unsupported.result)[Abi::TableHeader(0) + Abi::TableFault];
        require((fault & Abi::PoisonFlag) != 0u && unsupported.result->imageTablePoison.at(fault & ~Abi::PoisonFlag).reason == static_cast<std::uint32_t>(Abi::PoisonReason::UnsupportedOperation) && unsupported.result->imageTableFaults != 0u, (std::string("image table registration: the ") + name + " table does not fault on use").c_str());
        const auto* storage = TableBinding(*unsupported.result, DescriptorRole::ImageStorageTable);
        require(storage == nullptr || storage->count == 0u, (std::string("image table registration: the unsupported ") + name + " bound storage table elements").c_str());
    }

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
    require(snapshot.codes[0] == snapshot.codes[1] && snapshot.codes[2] == snapshot.codes[255] && snapshot.codes[0] != snapshot.codes[2] && snapshot.words.at(snapshot.codes[2]).dwords[0] == root[(Palette + 2u * 32u) / 4u], "pointer image table: the snapshot words are wrong");
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

    auto phiCode = maskedCode;
    phiCode.erase(phiCode.begin() + 8, phiCode.begin() + 10);
    phiCode.insert(phiCode.begin() + 8, {
        0xbf068010u, 0xbf850003u, 0xf4200444u, 0x20000000u, 0xbf820002u, 0xf4200444u, 0x20000000u
    });
    const auto phiRequest = request(phiCode, 0x36000u);
    const auto phiPlan = GetResourcePlan(phiRequest);
    require(phiPlan->info.images.size() == 1u && phiPlan->info.images[0].table == 0u, "pointer image table: a pointer read through a phi was not planned as a table");
    const auto phiTable = CompileTable(phiRequest);
    require(phiTable.capture->snapshot.tables.tables.at(0).codes == snapshot.codes, "pointer image table: a pointer read through a phi captured different records");

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
    require((snapshot.codes[6] & Abi::PoisonFlag) == 0u && (snapshot.codes[8] & Abi::PoisonFlag) == 0u && snapshot.words.at(snapshot.codes[6]).dwords[0] == root[0x100000010ull / 4u] && snapshot.words.at(snapshot.codes[8]).dwords[0] == root[0x50u / 4u] && root[0x100000010ull / 4u] != root[0x50u / 4u], "pointer table offset carry: the records on either side of the carry were not resolved");
    require(!TableTouched(compiled.regions, root + 0x100000030ull / 4u, 0x20u), "pointer table offset carry: the record whose second load wraps was read");
#ifdef _WIN32
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, Reserved);
#endif
}

void verifySelectedRecordFallbacks() {
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
    guest.materials = {};
    guest.srt[16] = 2u;
    const auto userData = TableUserData(guest);
    const auto request = TableRequest(TableSrtKeyCode, userData);
    const auto valid = CompileTable(request);

    heap[2][3] &= 0x0fffffffu;
    const auto invalid = CompileTable(request);
    const auto& invalidMap = TableMap(*invalid.result);
    const auto invalidCodes = TableCodes(invalidMap, 0);
    require(invalidCodes.size() == 1u && invalidCodes[0] != Abi::NullCode && (invalidCodes[0] & Abi::PoisonFlag) == 0u, "selected record fallbacks: an invalid selected T# has no entry");
    const auto invalidFlags = invalidMap.at(invalidCodes[0] + Abi::ImageRecordFlags);
    require((invalidFlags & Abi::PoisonFlag) != 0u, "selected record fallbacks: an invalid selected T# was bound");
    const auto& poison = invalid.result->imageTablePoison.at(invalidFlags & ~Abi::PoisonFlag);
    require(poison.reason == static_cast<std::uint32_t>(Abi::PoisonReason::NotImage) && poison.dwordCount == 8u && poison.words[3] == heap[2][3] && invalid.result->imageTableFaults != 0u, "selected record fallbacks: an invalid selected T# was not poisoned with its words");
    require(invalid.result->variantId == valid.result->variantId, "selected record fallbacks: an invalid selected T# changed the artifact");
    heap[2] = heap[1];

    guest.srt[16] = 200u;
    const auto beyond = CompileTable(request);
    const auto beyondCodes = TableCodes(TableMap(*beyond.result), 0);
    require(beyondCodes.size() == 1u && beyondCodes[0] == Abi::NullCode && beyond.result->imageTableFaults == 0u && !TableTouched(beyond.regions, heap.data(), sizeof(heap)), "selected record fallbacks: a key past the heap did not select a null image without reading the heap");
    guest.srt[16] = 2u;

    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, 4096, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    require(reserved != nullptr, "selected record fallbacks: cannot reserve an inaccessible page");
    FillTableSrt(guest, reserved, 32u, 4u);
    const auto unmapped = CompileTable(request);
    const auto& unmappedTable = unmapped.capture->snapshot.tables.tables.at(0);
    require(unmappedTable.keys == 1u && unmappedTable.codes.at(0) == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped)) && !TableTouched(unmapped.regions, reserved, 4096u), "selected record fallbacks: an unmapped selected record is not Unmapped poison or was read");
    FillTableSrt(guest, heap.data(), 32u, 4u);

    std::vector<std::uint32_t> loadedKey(TableSrtKeyCode);
    const auto shift = std::find(loadedKey.begin(), loadedKey.end(), 0x8f108510u);
    require(shift != loadedKey.end(), "selected record fallbacks: the program has no s_lshl_b32");
    loadedKey.insert(shift, {0xf4200406u, 0x20000000u});
    guest.materials[0][1] = 3u;
    guest.srt[16] = 4u;
    const auto loadedRequest = TableRequest(loadedKey, userData);
    const auto loadedPlan = GetResourcePlan(loadedRequest);
    require(loadedPlan->info.images.size() == 1u && loadedPlan->descriptorSources.at(loadedPlan->info.images[0].source).tableColumn->selected, "selected record fallbacks: a key loaded from a constant buffer at an SRT offset does not select its record");
    const auto loaded = CompileTable(loadedRequest);
    require(TableSelected(loaded.capture->snapshot.tables, 0u, "selected record fallbacks: a loaded key does not hold one readable selected record") == heap[3] && TableCovered(loaded.regions, &guest.materials[0][1], sizeof(std::uint32_t)), "selected record fallbacks: the record of a loaded key was not selected or its key was not captured");
    const auto materials = TableV(guest.materials.data(), 16u, 3u);
    const auto unreadable = TableV(reserved, 16u, 3u);
    std::copy(unreadable.begin(), unreadable.end(), guest.srt.begin() + 8);
    const auto lost = CompileTable(loadedRequest);
    const auto& lostTable = lost.capture->snapshot.tables.tables.at(0);
    require(lostTable.keys == 0u && lostTable.fault == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped)) && lost.result->imageTableFaults != 0u && lost.result->variantId == loaded.result->variantId, "selected record fallbacks: a key that cannot be read did not fault its table");
    std::copy(materials.begin(), materials.end(), guest.srt.begin() + 8);
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, 4096);
#endif

    auto writtenKey = loadedKey;
    const auto store = std::find(writtenKey.begin(), writtenKey.end(), 0x80070000u);
    require(store != writtenKey.end(), "selected record fallbacks: the program has no buffer_store_dword");
    *store = 0x80030000u;
    const auto writtenRequest = TableRequest(writtenKey, userData);
    const auto writtenPlan = GetResourcePlan(writtenRequest);
    const auto& writtenColumn = writtenPlan->descriptorSources.at(writtenPlan->info.images.at(0).source).tableColumn;
    require(writtenColumn.has_value() && !writtenColumn->selected && writtenColumn->keyDomains.empty(), "selected record fallbacks: a key loaded from a buffer the shader writes selected its record");
    const auto written = CompileTable(writtenRequest);
    require(written.capture->snapshot.tables.tables.at(0).keys == 4u, "selected record fallbacks: a key loaded from a buffer the shader writes did not keep the whole table");
    require(!loadedPlan->descriptorSources.at(loadedPlan->info.images[0].source).tableColumn->keyDomains.empty(), "selected record fallbacks: a key loaded from a constant buffer has no key domain");
    auto pointerWritten = loadedKey;
    const auto end = std::find(pointerWritten.begin(), pointerWritten.end(), 0xbf810000u);
    require(end != pointerWritten.end(), "selected record fallbacks: the program has no s_endpgm");
    pointerWritten.insert(end, {0xdc708000u, 0x001c0100u});
    const auto pointerPlan = GetResourcePlan(TableRequest(pointerWritten, userData));
    const auto& pointerColumn = pointerPlan->descriptorSources.at(pointerPlan->info.images.at(0).source).tableColumn;
    require(pointerPlan->info.bdaWrites && pointerColumn.has_value() && !pointerColumn->selected && pointerColumn->keyDomains.empty(), "selected record fallbacks: a key loaded from a constant buffer in a shader that stores through a pointer selected its record or kept its key domain");

    const auto lanePlan = GetResourcePlan(TableRequest(TableWholeCode, userData));
    require(!lanePlan->info.images.empty() && !lanePlan->descriptorSources.at(lanePlan->info.images[0].source).tableColumn->selected, "selected record fallbacks: a T# selected per lane left the table");

    alignas(4096) static std::array<std::array<std::uint32_t, 8>, 70000> large{};
    large.back() = heap[1];
    FillTableSrt(guest, large.data(), 32u, static_cast<std::uint32_t>(large.size()));
    guest.srt[16] = static_cast<std::uint32_t>(large.size() - 1u);
    const auto pastCap = CompileTable(request);
    require(TableSelected(pastCap.capture->snapshot.tables, 0u, "selected record fallbacks: a record past the key cap does not hold one readable selected record") == heap[1] && !pastCap.capture->snapshot.tables.tables[0].outside && pastCap.result->imageTableFaults == 0u, "selected record fallbacks: a record past the key cap was not selected");
    FillTableSrt(guest, heap.data(), 32u, 4u);
}

void verifySelectedRecordOffsetCarry() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture texture;
    alignas(4096) static TableGuest guest;
    std::fill(guest.heap.begin(), guest.heap.end(), std::array<std::uint32_t, 8>{});
    const auto image = TableT(texture.bytes.data(), TableFormat8888UNorm, TableType2D);
    auto* words = guest.heap.front().data();
    std::copy(image.begin(), image.end(), words + 0x50u / 4u);
    FillTableSrt(guest, guest.heap.data(), 0u, static_cast<std::uint32_t>(sizeof(guest.heap)));
    const auto userData = TableUserData(guest);
    const std::vector<std::uint32_t> code{0xf4080100u, 0xfa000000u, 0xf4080700u, 0xfa000030u, 0xf4080800u, 0xfa000010u, 0xf4000440u, 0xfa000040u, 0x8711ff11u,
        0x000000ffu, 0x8f128511u, 0x8113ff12u, 0xffffff10u, 0x8114ff12u, 0xffffff20u, 0xf4280902u, 0x26000040u, 0xf4280a02u, 0x28000040u, 0xf09c0f08u,
        0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    const auto request = TableRequest(code, userData);
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "selected record offset carry: a T# read by two loads of its record was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.selected && !column.address && column.stride == 32u && column.addend == 0xffffff10u && column.offset == 0x40u && column.foldedAddend == 0xffffff10u && column.foldedCarry == 16u, "selected record offset carry: the column is wrong");
    guest.srt[16] = 7u;
    const auto carried = CompileTable(request);
    const auto& carriedTable = carried.capture->snapshot.tables.tables.at(0);
    require(carriedTable.keys == 1u && carriedTable.codes.at(0) == Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::OffsetCarry)) && !TableTouched(carried.regions, guest.heap.data(), sizeof(guest.heap)), "selected record offset carry: the record whose second load wraps past 32 bits is not OffsetCarry poison or was read");
    guest.srt[16] = 8u;
    const auto wrapped = CompileTable(request);
    require(TableSelected(wrapped.capture->snapshot.tables, 0u, "selected record offset carry: the record past the wrap does not hold one readable selected record") == image && TableCovered(wrapped.regions, words + 0x50u / 4u, sizeof(image)), "selected record offset carry: the record past the wrap was not selected");
}

std::vector<std::uint32_t> WithImageOperation(std::vector<std::uint32_t> code, std::uint32_t word0, bool keepSample) {
    const auto sample = std::find(code.begin(), code.end(), 0xf09c0f08u);
    require(sample != code.end() && sample + 1 != code.end(), "image operation variant: the program has no image_sample_lz");
    if (keepSample) {
        code.insert(sample + 2, {word0, 0x00050000u});
    } else {
        sample[0] = word0;
        sample[1] = 0x00050000u;
    }
    return code;
}

void verifySelectedRecordDirectOperations() {
    using namespace ShaderRecompiler;
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
    const std::array<std::tuple<const char*, std::uint32_t, std::uint32_t>, 5> operations{{{"image_store", 0xf0200f08u, TableFormat32UInt}, {"image_atomic_add", 0xf0440108u, TableFormat32UInt}, {"image_atomic64_add", 0xf0440308u, TableFormat32_32UInt}, {"image_load_pck", 0xf0080f08u, TableFormat32UInt}, {"image_load_by2", 0xf1080308u, TableFormat16UNorm}}};
    for (const auto& [name, word0, format] : operations) {
        const auto what = std::string("selected record direct operations: ") + name;
        heap[2] = TableT(textures[1].bytes.data(), format, TableType2D);
        const auto code = WithImageOperation(TableSrtKeyCode, word0, false);
        const auto request = TableRequest(code, userData);
        const auto plan = GetResourcePlan(request);
        require(plan->info.images.size() == 1u && plan->info.images[0].table == NoTable && !plan->descriptorSources.at(plan->info.images[0].source).tableColumn.has_value(), (what + " through a T# whose record the CPU evaluates was planned through an image table").c_str());
        const auto compiled = word0 == 0xf0440308u ? TableCompiled{} : CompileTable(request);
        require(word0 == 0xf0440308u || (TableBinding(*compiled.result, DescriptorRole::ImageTable) == nullptr && compiled.result->imageTableFaults == 0u && TableCovered(compiled.regions, &heap[2], sizeof(heap[2]))), (what + " through a T# whose record the CPU evaluates was not bound from its record").c_str());
        const auto wholeCode = WithImageOperation(TableWholeCode, word0, false);
        const auto whole = GetResourcePlan(TableRequest(wholeCode, userData));
        require(whole->info.images.size() == 1u && whole->info.images[0].table == 0u && whole->info.images[0].tableOperation == (word0 == 0xf0200f08u || word0 == 0xf0440108u || word0 == 0xf0440308u ? TableOperation::Supported : TableOperation::Unsupported), (what + " through a T# keyed per lane left the image table or has the wrong table operation").c_str());
    }
    const auto mixedCode = WithImageOperation(TableSrtKeyCode, 0xf0200f08u, true);
    const auto mixed = GetResourcePlan(TableRequest(mixedCode, userData));
    require(mixed->info.images.size() == 2u && std::ranges::all_of(mixed->info.images, [&](const ImageResource& image) { return image.table == NoTable && !mixed->descriptorSources.at(image.source).tableColumn.has_value(); }), "selected record direct operations: a T# sampled and stored was planned through an image table for one of its operations");
    const auto sampled = GetResourcePlan(TableRequest(TableSrtKeyCode, userData));
    require(sampled->info.images.size() == 1u && sampled->info.images[0].table == 0u && sampled->descriptorSources.at(sampled->info.images[0].source).tableColumn->selected, "selected record direct operations: a T# that is only sampled no longer selects its record");
}

void verifyEvaluableHeapSamplerDirect() {
    using namespace ShaderRecompiler;
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
    const auto heapSampler = [](std::vector<std::uint32_t> code) {
        const auto direct = std::find(code.begin(), code.end(), 0xf4080200u);
        require(direct != code.end() && direct[1] == 0xfa000010u, "evaluable heap sampler: the program has no S# load from the SRT");
        code.erase(direct, direct + 2);
        const auto shift = std::find(code.begin(), code.end(), 0x8f108510u);
        require(shift != code.end(), "evaluable heap sampler: the program has no key shift");
        code.insert(shift + 1, {0xf4280202u, 0x20000000u});
        return code;
    };
    const auto evaluatedCode = heapSampler(TableSrtKeyCode);
    const auto evaluated = GetResourcePlan(TableRequest(evaluatedCode, userData));
    require(evaluated->info.samplers.size() == 1u && evaluated->info.samplers[0].table == NoTable && !evaluated->descriptorSources.at(evaluated->info.samplers[0].source).tableColumn.has_value(), "evaluable heap sampler: an S# read from a heap at an offset the SRT walker evaluates was planned through a sampler table");
    const auto perLaneCode = heapSampler(TableWholeCode);
    const auto perLane = GetResourcePlan(TableRequest(perLaneCode, userData));
    require(perLane->info.samplers.size() == 1u && perLane->info.samplers[0].table != NoTable && perLane->descriptorSources.at(perLane->info.samplers[0].source).tableColumn.has_value(), "evaluable heap sampler: an S# read from a heap at an offset keyed per lane was not planned through a sampler table");
}

void verifySelectedRecordUniformIndex() {
    using namespace ShaderRecompiler;
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
    const auto twoWaves = [&](const std::vector<std::uint32_t>& code) {
        auto request = TableRequest(code, userData);
        request.context.compute->numThreads = {128u, 1u, 1u};
        return CompileTable(request);
    };
    const auto nonUniform = [](const TableCompiled& compiled) {
        const auto spirv = compiled.result->spirv.Words();
        std::uint32_t count = 0;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            if ((spirv[cursor] & 0xffffu) == spv::OpDecorate && (spirv[cursor] >> 16u) == 3u && spirv[cursor + 2u] == spv::DecorationNonUniform) ++count;
        }
        return count;
    };
    const auto selected = twoWaves(TableSrtKeyCode);
    require(TableBinding(*selected.result, DescriptorRole::ImageTable) != nullptr && nonUniform(selected) == 0u, "selected record uniform index: the image table index of a selected record in a two-wave workgroup is decorated NonUniform");
    const auto whole = twoWaves(TableWholeCode);
    require(nonUniform(whole) != 0u, "selected record uniform index: the image table index of a table keyed per lane in a two-wave workgroup is not decorated NonUniform");
}

void verifyChainedTableOffset() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(256) static std::array<std::uint32_t, 0x3000> root{};
    static std::array<std::uint32_t, 8> keys{};
    static std::array<std::uint32_t, 4> output{};
    const auto keysV = TableV(keys.data(), 0u, static_cast<std::uint32_t>(sizeof(keys)));
    const auto outputV = TableV(output.data(), 0u, static_cast<std::uint32_t>(sizeof(output)));
    std::copy(keysV.begin(), keysV.end(), root.begin());
    std::copy(outputV.begin(), outputV.end(), root.begin() + 4);
    const auto at = [&](std::uint32_t offset, std::array<std::uint32_t, 8> words) {
        std::copy(words.begin(), words.end(), root.begin() + offset / 4u);
    };
    at(0x60u + 3u * 32u, TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D));
    at(0x60u + 5u * 32u, TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D));
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u)};
    const std::vector<std::uint32_t> code{0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x7e200500u, 0x8f108210u, 0xf4200444u,
        0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0x8113a012u, 0x81149013u, 0xf4080900u, 0x26000040u, 0xf4080a00u, 0x28000040u, 0xf09c0f08u,
        0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    auto request = TableRequest(code, userData);
    request.shader.codeAddress = 0x37000u;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "chained table offset: a T# whose second load offset adds to the first one's was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.address && column.stride == 32u && column.addend == 0x20u && column.offset == 0x40u && column.maxKey == 0xffu && column.foldedAddend == 0x20u && column.foldedCarry == 16u, "chained table offset: the column is wrong");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    const auto& words = snapshot.words;
    require(snapshot.keys == 256u && (snapshot.codes[3] & Abi::PoisonFlag) == 0u && (snapshot.codes[5] & Abi::PoisonFlag) == 0u, "chained table offset: the records were not resolved");
    require(words.at(snapshot.codes[3]).dwords[0] == root[(0x60u + 3u * 32u) / 4u] && words.at(snapshot.codes[3]).dwords[7] == root[(0x60u + 3u * 32u) / 4u + 7u] && words.at(snapshot.codes[5]).dwords[0] == root[(0x60u + 5u * 32u) / 4u] && snapshot.codes[3] != snapshot.codes[5], "chained table offset: a record was read from the wrong offset");
}

void verifyPointerChainImageTable() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    constexpr std::uint32_t Entries = 256;
    static TableTexture textures[2];
    alignas(4096) static std::array<std::uint32_t, 16> root{};
    alignas(4096) static std::array<std::uint32_t, Entries * 4u> records{};
    alignas(4096) static std::array<std::array<std::uint32_t, 8>, 4> links{};
    alignas(4096) static std::array<std::array<std::uint32_t, 16>, 3> descriptors{};
    static std::array<std::uint32_t, 8> keys{};
    static std::array<std::uint32_t, 4> output{};
    const auto address = [](const void* pointer) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
    const auto keysV = TableV(keys.data(), 0u, static_cast<std::uint32_t>(sizeof(keys)));
    const auto outputV = TableV(output.data(), 0u, static_cast<std::uint32_t>(sizeof(output)));
    const auto recordsV = TableV(records.data(), 0u, static_cast<std::uint32_t>(sizeof(records)));
    std::copy(keysV.begin(), keysV.end(), root.begin());
    std::copy(outputV.begin(), outputV.end(), root.begin() + 4);
    std::copy(recordsV.begin(), recordsV.end(), root.begin() + 12);
    const auto store = [](std::uint32_t* at, std::uint64_t pointer) {
        at[0] = static_cast<std::uint32_t>(pointer);
        at[1] = static_cast<std::uint32_t>(pointer >> 32u);
    };
    const auto descriptor0 = TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D);
    const auto descriptor1 = TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D);
    std::copy(descriptor0.begin(), descriptor0.end(), descriptors[0].begin() + 8);
    std::copy(descriptor0.begin(), descriptor0.end(), descriptors[1].begin() + 8);
    std::copy(descriptor1.begin(), descriptor1.end(), descriptors[2].begin() + 8);
    for (std::uint32_t record = 0; record < 3u; record++) {
        store(records.data() + record * 4u + 2u, address(links[record].data()));
        store(links[record].data() + 4u, address(descriptors[record].data()));
    }
    store(records.data() + 3u * 4u + 2u, address(links[3].data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address(root.data())), static_cast<std::uint32_t>(address(root.data()) >> 32u)};
    const std::vector<std::uint32_t> code{0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0xf4080100u, 0xfa000030u, 0x7e200500u,
        0x8f108210u, 0xf4200444u, 0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128411u, 0xf4240b02u, 0x24000008u, 0xf4040b96u, 0xfa000010u, 0xf40c0917u,
        0xfa000020u, 0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    auto request = TableRequest(code, userData);
    request.shader.codeAddress = 0x37000u;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "pointer chain image table: a T# behind two pointers was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.indirect && !column.address && column.stride == 16u && column.addend == 0u && column.offset == 8u && column.dwordCount == 8u && column.maxKey == 0xffu, "pointer chain image table: the record column is wrong");
    require(column.indirectOffset == 0u && column.indirectImmediate == 0x20u && column.pointerHops == std::vector<PointerHop>{{0u, 0x10u}}, "pointer chain image table: the pointer hops are wrong");
    for (const auto& memory : plan->memoryInfo) {
        require(memory.kind != ResourceKind::ScalarAddress || memory.planningOnly, "pointer chain image table: a load along the pointer chain still reads through BDA");
    }
    require(!plan->info.usesDma, "pointer chain image table: the shader still reads through BDA");

    const auto compiled = CompileTable(request);
    const auto& tables = compiled.capture->snapshot.tables;
    const auto& snapshot = tables.tables.at(0);
    require(snapshot.keys == Entries && snapshot.codes[0] == snapshot.codes[1] && snapshot.codes[0] != snapshot.codes[2] && (snapshot.codes[0] & Abi::PoisonFlag) == 0u && (snapshot.codes[2] & Abi::PoisonFlag) == 0u, "pointer chain image table: the records behind two pointers do not resolve to one element per distinct T#");
    require(snapshot.words.at(snapshot.codes[0]).dwords[0] == descriptor0[0] && snapshot.words.at(snapshot.codes[2]).dwords[0] == descriptor1[0] && snapshot.words.at(snapshot.codes[2]).dwords[1] == descriptor1[1], "pointer chain image table: the snapshot words are wrong");
    const auto unmapped = Abi::PoisonCode(static_cast<std::uint32_t>(Abi::PoisonReason::Unmapped));
    require(snapshot.codes[3] == unmapped && snapshot.codes[4] == unmapped && snapshot.codes[255] == unmapped, "pointer chain image table: a record whose chain reaches a null pointer is not Unmapped poison");
    for (std::uint32_t record = 0; record < 3u; record++) {
        require(TableCovered(compiled.regions, links[record].data() + 4u, 8u) && TableCovered(compiled.regions, descriptors[record].data() + 8u, 32u), "pointer chain image table: the captured regions miss a link or a T#");
        const auto link = address(links[record].data() + 4u);
        require(std::ranges::any_of(tables.ranges, [&](const auto& range) { return range.first <= link && link + 8u <= range.first + range.second; }), "pointer chain image table: the snapshot ranges miss a link of the chain");
    }

    store(links[1].data() + 4u, address(descriptors[2].data()));
    const auto relinked = CompileTable(request);
    const auto& changed = relinked.capture->snapshot.tables.tables.at(0);
    require(changed.codes[1] == changed.codes[2] && changed.codes[1] != changed.codes[0], "pointer chain image table: changing the second pointer of a record did not change its T#");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(compiled.result->spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif
    store(links[1].data() + 4u, address(descriptors[1].data()));

    auto phiCode = code;
    phiCode.erase(phiCode.begin() + 15, phiCode.begin() + 17);
    phiCode.insert(phiCode.begin() + 15, {
        0xbf068010u, 0xbf850003u, 0xf4240b02u, 0x24000008u, 0xbf820002u, 0xf4240b02u, 0x24000008u
    });
    auto phiRequest = TableRequest(phiCode, userData);
    phiRequest.shader.codeAddress = 0x38000u;
    const auto phiPlan = GetResourcePlan(phiRequest);
    require(phiPlan->info.images.size() == 1u && phiPlan->info.images[0].table == 0u, "pointer chain image table: a pointer read through a phi was not planned as a table");
    const auto phiTable = CompileTable(phiRequest);
    require(phiTable.capture->snapshot.tables.tables.at(0).codes == snapshot.codes, "pointer chain image table: a pointer read through a phi captured different records");

    auto divergentCode = code;
    divergentCode.erase(divergentCode.begin() + 15, divergentCode.begin() + 17);
    divergentCode.insert(divergentCode.begin() + 15, {
        0xbf068010u, 0xbf850003u, 0xf4240b02u, 0x24000008u, 0xbf820002u, 0xf4240b02u, 0x24000018u
    });
    auto divergentRequest = TableRequest(divergentCode, userData);
    divergentRequest.shader.codeAddress = 0x39000u;
    expectFailure([&] { static_cast<void>(GetResourcePlan(divergentRequest)); }, "GetImageResource dword 0 is not a valid runtime value", "pointer chain image table: a phi reading different table offsets was planned as a table");

    auto nonTableCode = code;
    nonTableCode.erase(nonTableCode.begin() + 15, nonTableCode.begin() + 17);
    nonTableCode.insert(nonTableCode.begin() + 15, {
        0xbf068010u, 0xbf850003u, 0xf4240b02u, 0x24000008u, 0xbf820002u, 0xbe960380u, 0xbe970380u
    });
    auto nonTableRequest = TableRequest(nonTableCode, userData);
    nonTableRequest.shader.codeAddress = 0x3a000u;
    expectFailure([&] { static_cast<void>(GetResourcePlan(nonTableRequest)); }, "GetImageResource dword 0 is not a valid runtime value", "pointer chain image table: a phi with a non-table branch was planned as a table");

    auto armOffsetCode = code;
    armOffsetCode.erase(armOffsetCode.begin() + 14, armOffsetCode.begin() + 17);
    armOffsetCode.insert(armOffsetCode.begin() + 14, {
        0xbf068010u, 0xbf850004u, 0x8f128411u, 0xf4240b02u, 0x24000008u, 0xbf820003u, 0x8f128411u, 0xf4240b02u, 0x24000008u
    });
    auto armOffsetRequest = TableRequest(armOffsetCode, userData);
    armOffsetRequest.shader.codeAddress = 0x3b000u;
    expectFailure([&] { static_cast<void>(GetResourcePlan(armOffsetRequest)); }, "GetImageResource dword 0 is not a valid runtime value", "pointer chain image table: a phi whose arms compute their own table offset was planned as a table");
}

void verifyWaterfallImageTable() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture textures[2];
    alignas(256) static std::array<std::uint32_t, 0x3000> root{};
    static std::array<std::uint32_t, 8> keys{};
    static std::array<std::uint32_t, 4> output{};
    const auto keysV = TableV(keys.data(), 0u, static_cast<std::uint32_t>(sizeof(keys)));
    const auto outputV = TableV(output.data(), 0u, static_cast<std::uint32_t>(sizeof(output)));
    std::copy(keysV.begin(), keysV.end(), root.begin());
    std::copy(outputV.begin(), outputV.end(), root.begin() + 4);
    const auto at = [&](std::uint32_t offset, std::array<std::uint32_t, 8> words) {
        std::copy(words.begin(), words.end(), root.begin() + offset / 4u);
    };
    at(0x40u + 2u * 32u, TableT(textures[0].bytes.data(), TableFormat8888UNorm, TableType2D));
    at(0x40u + 4u * 32u, TableT(textures[1].bytes.data(), TableFormat32UInt, TableType2D));
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u)};
    const std::vector<std::uint32_t> code{0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x7e220500u, 0x8f128511u, 0xf40c0900u, 0x24000040u,
        0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    auto request = TableRequest(code, userData);
    request.shader.codeAddress = 0x38000u;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "waterfall image table: the T# was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.address && column.stride == 32u && column.offset == 0x40u && column.dwordCount == 8u && column.unkeyed, "waterfall image table: the column is wrong");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    require(snapshot.keys == Abi::MaxKeys && (snapshot.codes[2] & Abi::PoisonFlag) == 0u && (snapshot.codes[4] & Abi::PoisonFlag) == 0u, "waterfall image table: the snapshot failed");
    auto boundedCode = code;
    boundedCode.insert(boundedCode.begin() + 4, {0x7d8a0080u, 0x7e020284u, 0x02000282u});
    auto boundedRequest = TableRequest(boundedCode, userData);
    boundedRequest.shader.codeAddress = 0x39000u;
    const auto boundedPlan = GetResourcePlan(boundedRequest);
    require(boundedPlan->info.images.size() == 1u && boundedPlan->info.images[0].table == 0u, "waterfall image table: bounded key was not planned as a table");
    const auto& boundedColumn = *boundedPlan->descriptorSources.at(boundedPlan->info.images[0].source).tableColumn;
    require(boundedColumn.maxKey == 6u, "waterfall image table: bounded key maxKey is wrong");
    const auto boundedCompiled = CompileTable(boundedRequest);
    const auto& boundedSnapshot = boundedCompiled.capture->snapshot.tables.tables.at(0);
    require(boundedSnapshot.keys == 7u && boundedSnapshot.keys < Abi::MaxKeys && (boundedSnapshot.codes[2] & Abi::PoisonFlag) == 0u && (boundedSnapshot.codes[4] & Abi::PoisonFlag) == 0u, "waterfall image table: bounded key snapshot failed");
    IrResourcePlan planA;
    std::vector<std::unique_ptr<IrValue>> values;
    const auto make = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments = {}) -> IrValue& {
        values.push_back(std::make_unique<IrValue>(opcode, type, static_cast<std::uint32_t>(values.size())));
        for (auto* argument : arguments) values.back()->AddArgument(argument);
        return *values.back();
    };
    const auto constant = [&](std::uint32_t val) -> IrValue& {
        auto& leaf = make(IrOpcode::Void, IrType::U32);
        leaf.SetImmediateU32(val);
        return leaf;
    };
    auto& scalar = make(IrOpcode::Void, IrType::ScalarReg);
    scalar.SetRegister({RegisterBank::Scalar, 0});
    auto& vector = make(IrOpcode::Void, IrType::VectorReg);
    vector.SetRegister({RegisterBank::Vector, 0});
    auto& input = make(IrOpcode::GetUserData, IrType::U32, {&scalar});
    auto& lane = make(IrOpcode::GetRegister, IrType::U32, {&vector});
    auto& condA = make(IrOpcode::IEqual32, IrType::Bool, {&input, &constant(4)});
    auto& condB = make(IrOpcode::IEqual32, IrType::Bool, {&input, &lane});
    auto& conjMask = make(IrOpcode::LogicalAnd, IrType::Bool, {&condB, &condA});
    auto& selectConj = make(IrOpcode::SelectU32, IrType::U32, {&condB, &input, &lane});
    auto& firstConj = make(IrOpcode::ReadFirstLane, IrType::U32, {&selectConj, &conjMask});
    require(SrtWalker().ValidateRuntimeValue(planA, &firstConj), "waterfall image table: conjunct select was refused by validator");
    planA.srtPlanComplete = true;
    planA.descriptorSources.push_back({});
    planA.descriptorSources.back().dwordCount = 1;
    planA.descriptorSources.back().dwords[0] = &firstConj;
    planA.materializationSources.push_back(0);
    const std::array<std::uint32_t, 1> userDataA{42u};
    SrtRuntime runtimeA{userDataA};
    const auto* tapeA = Detail::SrtTape::For(planA);
    require(tapeA != nullptr, "waterfall image table: tape was not built for conjunct select");
    std::vector<DescriptorValue> resultsA;
    std::vector<std::uint32_t> flatA;
    std::vector<std::uint8_t> activeA;
    require(tapeA->Run(planA, runtimeA, resultsA, flatA, activeA), "waterfall image table: tape failed to run conjunct select");
    require(!resultsA.empty() && resultsA[0].dwords[0] == 42u, "waterfall image table: tape evaluated conjunct select to wrong value");
    auto& otherCond = make(IrOpcode::IEqual32, IrType::Bool, {&lane, &constant(5)});
    auto& selectOther = make(IrOpcode::SelectU32, IrType::U32, {&otherCond, &input, &lane});
    auto& firstOther = make(IrOpcode::ReadFirstLane, IrType::U32, {&selectOther, &conjMask});
    require(!SrtWalker().ValidateRuntimeValue(planA, &firstOther), "waterfall image table: non-implied condition was accepted by validator");
}

void verifySplitWaterfallImageTable() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    static TableTexture texture;
    alignas(256) static std::array<std::uint32_t, 0x3000> root{};
    static std::array<std::uint32_t, 4> output{};
    const auto outputV = TableV(output.data(), 0u, static_cast<std::uint32_t>(sizeof(output)));
    std::copy(outputV.begin(), outputV.end(), root.begin() + 4);
    const auto descriptor = TableT(texture.bytes.data(), TableFormat8888UNorm, TableType2D);
    std::copy(descriptor.begin(), descriptor.end(), root.begin() + 0x40u / 4u);
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u)};
    const auto code = [](std::uint32_t addend) {
        return std::vector<std::uint32_t>{0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x7da80086u, 0x7e220500u, 0xd7460001u, 0x00010b00u | (addend << 18u), 0x7e260501u, 0x8f128511u,
            0xf4080900u, 0x24000040u, 0xf4080a00u, 0x26000040u, 0xf09c0f08u, 0x01090000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    };
    const auto split = code(0x90u);
    auto request = TableRequest(split, userData);
    request.shader.codeAddress = 0x3a000u;
    const auto plan = GetResourcePlan(request);
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u, "split waterfall image table: the T# was not planned as a table");
    const auto& column = *plan->descriptorSources.at(plan->info.images[0].source).tableColumn;
    require(column.address && column.stride == 32u && column.offset == 0x40u && column.dwordCount == 8u && column.foldedCarry == 16u, "split waterfall image table: the column is wrong");
    const auto compiled = CompileTable(request);
    const auto& snapshot = compiled.capture->snapshot.tables.tables.at(0);
    require((snapshot.codes[0] & Abi::PoisonFlag) == 0u && snapshot.words.at(snapshot.codes[0]).dwords == descriptor, "split waterfall image table: record 0 was not read whole");
    for (const std::uint32_t addend : {0xa0u, 0x88u}) {
        const auto otherCode = code(addend);
        auto other = TableRequest(otherCode, userData);
        other.shader.codeAddress = 0x3b000u + addend * 0x100u;
        expectFailure([&] { static_cast<void>(GetResourcePlan(other)); }, "GetImageResource dword 0 is not a valid runtime value", "split waterfall image table: a second half outside the record was accepted");
    }
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
        const auto memoryRegions = memory.Regions();
        request.context.memory = memoryRegions;
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
    const auto twoLaneRegions = twoLaneMemory.Regions();
    twoLane.context.memory = twoLaneRegions;
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
    auto loopRequest = makeRequest(loopCode);
    const auto loopPlan = GetResourcePlan(loopRequest);
    require(std::ranges::all_of(loopPlan->info.images, [](const ImageResource& image) { return image.optionalDescriptor; }), "descriptor Phi: the arms of a descriptor web without a folded path were not planned as optional");

    const std::vector<std::uint32_t> sequentialCode{0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0xfa000030u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    auto sequential = makeRequest(sequentialCode);
    const auto sequentialPlan = GetResourcePlan(sequential);
    require(sequentialPlan->info.images.size() == 2u && sequentialPlan->info.samplers.size() == 2u, "descriptor Phi: a T# and an S# chosen in separate blocks were not split into their SRT descriptors");
    AgcDriver::ShaderMemory sequentialMemory({});
    const auto sequentialCapture = sequentialMemory.Capture(sequential);
    const auto sequentialRegions = sequentialMemory.Regions();
    sequential.context.memory = sequentialRegions;
    static_cast<void>(Recompile(sequential, *sequentialCapture));

    const auto planRestoredSampler = [&](std::span<const std::uint32_t> code, const char* message) {
        auto request = makeRequest(code);
        const auto plan = GetResourcePlan(request);
        require(plan->info.samplers.size() == 1u && plan->info.sampledPairs.size() == 1u, message);
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        const auto memoryRegions = memory.Regions();
        request.context.memory = memoryRegions;
        static_cast<void>(Recompile(request, *capture));
    };
    std::vector<std::uint32_t> restoredSamplerCode{0xf40c0200u, 0xfa000000u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbe930380u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20400u, 0xbe900380u, 0xbe910380u, 0xbe9c0414u, 0xbe9e0416u, 0xbf068011u, 0xbf850001u, 0x80128112u, 0xbe94041cu, 0xbe96041eu, 0x80118111u, 0xbf0a8211u, 0xbf85fff6u, 0xf09c0f08u, 0x00a20000u, 0x80108110u, 0xbf0a8210u, 0xbf85fff0u, 0x80138113u, 0xbf0a8213u, 0xbf85ffecu, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xe0700000u, 0x80060400u, 0x7d880090u, 0xbea23c6au, 0x4a020081u, 0xe0700000u, 0x80060100u, 0xbf810000u};
    planRestoredSampler(restoredSamplerCode, "descriptor Phi: an S# copied out and back in three nested loops was planned as more than one sampler");
    restoredSamplerCode.erase(restoredSamplerCode.end() - 6, restoredSamplerCode.end() - 1);
    planRestoredSampler(restoredSamplerCode, "descriptor Phi: an S# copied out and back in three nested loops without a masked write was planned as more than one sampler");

    const auto entryWrites = [&](std::initializer_list<std::uint32_t> words) {
        auto code = loopCode;
        code.insert(code.begin() + 8, words);
        return code;
    };
    const std::array<std::pair<std::vector<std::uint32_t>, const char*>, 3> controlEntries{{
        {entryWrites({0xbe8e1f00u}), "descriptor Phi: a loop entry whose T# registers hold the program counter was not folded onto the loop's two SRT T#s with a fault"},
        {entryWrites({0xbe8e037eu}), "descriptor Phi: a loop entry whose T# registers hold EXEC was not folded onto the loop's two SRT T#s with a fault"},
        {entryWrites({0x7d840080u, 0xbe8e036au}), "descriptor Phi: a loop entry whose T# registers hold a compare mask was not folded onto the loop's two SRT T#s with a fault"},
    }};
    for (const auto& [code, message] : controlEntries) {
        auto request = makeRequest(code);
        const auto plan = GetResourcePlan(request);
        require(plan->info.images.size() == 2u && plan->info.usesFaultBuffer, message);
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
    expectFailure([&] { static_cast<void>(planImages(distinctLoads(64u))); }, "; descriptor web: more than 64 distinct descriptors", "descriptor Phi: the failure did not name the descriptor limit");

    const auto splitReloads = [](std::uint32_t joins) {
        std::vector<std::uint32_t> body;
        for (std::uint32_t join = 0; join < joins; join++) {
            body.insert(body.end(), {0xbf068014u, 0xbf850003u, 0xf40003c0u, join % 2u == 0u ? 0xfa000088u : 0xfa00008cu, 0xbf820006u, 0xf4080200u, 0xfa000060u, 0xf4040300u, 0xfa000070u, 0xf4000380u, 0xfa000078u});
        }
        return body;
    };
    require(planImages(splitReloads(8u)) == 4u, "descriptor Phi: separate reloads of T# dword 7 and dwords 0-6 were not split into their 4 T#s");
    expectFailure([&] { static_cast<void>(planImages(splitReloads(260u))); }, "GetImageResource dword 0 is not a valid runtime value", "descriptor Phi: a web over more than 512 descriptor tuples was accepted");
    expectFailure([&] { static_cast<void>(planImages(splitReloads(260u))); }, "; descriptor web: more than 512 descriptor states", "descriptor Phi: the failure did not name the state limit");
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
    require(plan->info.images.size() == 1u && plan->info.images[0].table == 0u && column.has_value() && column->stride == 48u && column->dwordCount == 8u && column->selected, "uniform table image: the T# read from a constant buffer at a user data offset does not select its record");
    for (const auto& [key, expected] : {std::pair{2u, second}, std::pair{1u, first}}) {
        userData[2] = key;
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        const auto& tables = capture->snapshot.tables;
        require(tables.tables.size() == 1u && TableSelected(tables, 0u, "uniform table image: a table does not hold one readable selected record") == expected, "uniform table image: the T# of the record the user data selects was not read");
        const auto regions = memory.Regions();
        require(TableCovered(regions, guest.table[key].data() + 4, sizeof(expected)) && !TableCovered(regions, guest.table[3u - key].data() + 4, sizeof(std::uint32_t)), "uniform table image: the record the user data does not select was read");
    }
}

void verifyUndefinedImageFormats() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t UndefinedFormat = 300;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const auto imageDescriptor = [&](std::uint32_t format) {
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (format << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    const auto valid = imageDescriptor(Format8888UNorm);
    const auto undefined = imageDescriptor(UndefinedFormat);
    require(IsDefinedImageFormat(static_cast<IrBufferFormat>(Format8888UNorm)) && !IsDefinedImageFormat(static_cast<IrBufferFormat>(UndefinedFormat)), "undefined image formats: the GFX10 format table is wrong");
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::array<std::uint32_t, 32> srt{};
    const auto fill = [&](const std::array<std::uint32_t, 8>& first, const std::array<std::uint32_t, 8>& second) {
        srt = {};
        std::copy(first.begin(), first.end(), srt.begin());
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
        srt[20] = 1u;
        std::copy(second.begin(), second.end(), srt.begin() + 24);
    };
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const auto makeRequest = [&](const std::vector<std::uint32_t>& code, std::uint64_t address) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, address, code, 0, {}};
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
        return request;
    };
    const auto isNull = [](const DescriptorValue& value) {
        return std::ranges::all_of(value.dwords, [](std::uint32_t word) { return word == 0u; });
    };

    const std::vector<std::uint32_t> armCode{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const auto armRequest = makeRequest(armCode, 0x24000u);
    const auto armPlan = GetResourcePlan(armRequest);
    require(armPlan->info.images.size() == 2u && std::ranges::none_of(armPlan->info.images, [](const ImageResource& image) { return image.optionalDescriptor; }) && !armPlan->info.usesFaultBuffer, "undefined image formats: the arms of a split along incoming edges were planned as optional");
    fill(valid, undefined);
    {
        AgcDriver::ShaderMemory memory({});
        expectFailure([&] { static_cast<void>(memory.Capture(armRequest)); }, "runtime image descriptor is invalid", "undefined image formats: an arm of a split along incoming edges was bound as a null image");
    }

    const std::vector<std::uint32_t> webCode{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xbf068010u, 0xbf850007u, 0xbf068110u, 0xbf850004u, 0xf40c0200u, 0xfa000060u, 0xbf8cc07fu, 0xbf820001u, 0xbe8e1f00u, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    auto webRequest = makeRequest(webCode, 0x27000u);
    const auto webPlan = GetResourcePlan(webRequest);
    require(webPlan->info.images.size() == 2u && std::ranges::all_of(webPlan->info.images, [](const ImageResource& image) { return image.optionalDescriptor; }) && webPlan->info.usesFaultBuffer, "undefined image formats: the arms of a web with a folded path were not planned as optional images with a fault");
    fill(valid, undefined);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(webRequest);
        const auto& images = capture->snapshot.images;
        const auto holdsValid = std::ranges::any_of(images, [&](const DescriptorValue& value) { return std::equal(valid.begin(), valid.end(), value.dwords.begin()); });
        require(images.size() == 2u && holdsValid && std::ranges::count_if(images, isNull) == 1, "undefined image formats: an arm whose T# has no GFX10 format was not bound as a null image");
        const auto regions = memory.Regions();
        webRequest.context.memory = regions;
        static_cast<void>(Recompile(webRequest, *capture));
    }

    const std::vector<std::uint32_t> directCode{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xbf068010u, 0xbf850006u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const auto directRequest = makeRequest(directCode, 0x25000u);
    fill(undefined, valid);
    {
        AgcDriver::ShaderMemory memory({});
        expectFailure([&] { static_cast<void>(memory.Capture(directRequest)); }, "runtime image descriptor is invalid", "undefined image formats: a sampled T# with no GFX10 format was accepted");
    }
    fill(valid, valid);
    {
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(directRequest);
        require(capture->snapshot.images.size() == 1u && std::equal(valid.begin(), valid.end(), capture->snapshot.images[0].dwords.begin()), "undefined image formats: a valid T# was not read");
    }

    const std::vector<std::uint32_t> samplerCode{0xf4000400u, 0xfa000050u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0xfa000060u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const auto samplerRequest = makeRequest(samplerCode, 0x26000u);
    const auto samplerPlan = GetResourcePlan(samplerRequest);
    require(samplerPlan->info.images.size() == 1u && samplerPlan->info.samplers.size() == 2u && !samplerPlan->info.images[0].optionalDescriptor, "undefined image formats: a T# shared by every arm of a split S# was planned as optional");
    fill(undefined, {});
    {
        AgcDriver::ShaderMemory memory({});
        expectFailure([&] { static_cast<void>(memory.Capture(samplerRequest)); }, "runtime image descriptor is invalid", "undefined image formats: a T# shared by every arm of a split S# was bound as a null image");
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
    const auto memoryRegions = memory.Regions();
    request.context.memory = memoryRegions;
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
    const auto memoryRegions = memory.Regions();
    request.context.memory = memoryRegions;
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

void verifyFusedExactArithmetic() {
    using namespace ShaderRecompiler;
    const auto pixel = [](bool fused) {
        static const std::vector<std::uint32_t> code{0x7e000200u, 0x10020100u, 0x06040101u, 0x08060302u, 0x7e000303u, 0xf800180fu, 0u, 0xbf810000u};
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
        request.target.subgroupSize = 64;
        request.target.fusedExactArithmetic = fused;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    struct Counts {
        std::uint32_t exactArithmetic = 0;
        std::uint32_t fmas = 0;
    };
    const auto count = [](std::span<const std::uint32_t> spirv) {
        std::set<std::uint32_t> exact;
        std::vector<std::uint32_t> arithmetic;
        Counts result;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            const auto op = spirv[cursor] & 0xffffu;
            const auto words = spirv[cursor] >> 16u;
            if (op == spv::OpDecorate && words == 3u && spirv[cursor + 2] == spv::DecorationNoContraction) exact.insert(spirv[cursor + 1]);
            if (op == spv::OpFAdd || op == spv::OpFSub || op == spv::OpFMul) arithmetic.push_back(spirv[cursor + 2]);
            if (op == spv::OpExtInst && words >= 5u && spirv[cursor + 4] == 50u) ++result.fmas;
        }
        result.exactArithmetic = static_cast<std::uint32_t>(std::count_if(arithmetic.begin(), arithmetic.end(), [&](std::uint32_t id) { return exact.contains(id); }));
        return result;
    };
    const auto plain = count(pixel(false));
    const auto fused = count(pixel(true));
    require(plain.exactArithmetic >= 3u, "fused exact arithmetic: the f32 multiply, add and subtract were not exact operations without the flag");
    require(fused.exactArithmetic == 0u && fused.fmas >= plain.fmas + 3u, "fused exact arithmetic: the f32 multiply, add and subtract did not become fused multiply-adds with the flag");
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
    auto loopScan = scan(0xffffffffu, 0x26000000u, 0x83800000u);
    const auto savedMaskLoop = AgcTest::SavedExecMaskLoop(8u);
    loopScan.insert(loopScan.begin(), savedMaskLoop.begin(), savedMaskLoop.end());
    auto helperLoop = loopScan;
    helperLoop[0] = 0xbea204c1u;
    expectFailure([&] { static_cast<void>(pixel(helperLoop, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a loop initialized with helper lanes was proven to exclude them");
    auto upperHelperLoop = loopScan;
    upperHelperLoop[0] = 0xbea2037eu;
    upperHelperLoop.insert(upperHelperLoop.begin() + 1, 0xbea303c1u);
    expectFailure([&] { static_cast<void>(pixel(upperHelperLoop, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: the low-word loop proof was reused for helper-inclusive upper lanes");
    require(reductions(pixel(loopScan, 32u, true), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: a loop-carried saved EXEC mask exhausted the entry-lane proof on 32 lanes");
    require(halves(pixel(loopScan, 64u, true), spv::OpGroupNonUniformUMin, 0xffffffffu), "half-wave reduction: a loop-carried saved EXEC mask did not preserve the two half-wave UMin reductions");
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

void verifyEntryLaneProofBudget() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic, spv::CapabilityClipDistance};
    std::vector<std::uint32_t> code{0xbe98047eu, 0x7e160f00u, 0x8786187eu, 0xbeea287eu, 0xd501000du, 0x001a16c1u};
    for (const auto control : {0xff01110du, 0xff01120du, 0xff01140du, 0xff01180du}) code.insert(code.end(), {0x261a1afau, control});
    code.insert(code.end(), {0xd778100cu, 0x0305830du, 0x261a190du, 0xbefe046au, 0xd7600006u, 0x00013f0du, 0xd7600007u, 0x00017f0du, 0x83880706u, 0x7e020208u, 0xf800180fu, 0x01010101u, 0xbf810000u});
    std::vector<std::uint32_t> savedMaskLoops;
    for (std::uint32_t loop = 0; loop < 3u; ++loop) {
        std::vector<std::uint32_t> savedMaskLoop{0xbea2047eu, 0xbea40380u};
        savedMaskLoop.insert(savedMaskLoop.end(), {0xbefe0422u, 0x7c080080u, 0x8aa26a7eu});
        const auto branch = 0xbf850000u | static_cast<std::uint16_t>(2 - static_cast<int>(savedMaskLoop.size() + 3u));
        savedMaskLoop.insert(savedMaskLoop.end(), {0x80248124u, 0xbf0a8424u, branch, 0xbefe0422u});
        savedMaskLoops.insert(savedMaskLoops.end(), savedMaskLoop.begin(), savedMaskLoop.end());
    }
    code.insert(code.begin(), savedMaskLoops.begin(), savedMaskLoops.end());

    ShaderPixelStageInfo pixel{};
    pixel.inputAddr = PixelInputBit(PixelInput::PositionX);
    pixel.posX = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64u;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32u;
    request.target.supportedCapabilities = capabilities;
    request.layout.pushConstantSizeBytes = 128u;
    request.useCache = false;
    const auto spirv = Recompile(request).spirv;
    std::uint32_t reductions = 0;
    for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
        if ((spirv[cursor] & 0xffffu) == static_cast<std::uint32_t>(spv::OpGroupNonUniformUMin) && (spirv[cursor] >> 16u) == 6u && spirv[cursor + 4u] == spv::GroupOperationReduce) ++reductions;
    }
    require(reductions == 1u, "a wave64 pixel UMin scan after loop-carried saved EXEC masks did not use a 32-lane subgroup reduction");
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

void verifySingleLaneElectedAtomicMax() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 7> capabilities{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic, spv::CapabilityGroupNonUniformShuffleRelative, spv::CapabilityClipDistance, spv::CapabilityInt64};
    static const std::vector<std::uint32_t> userData{0x10000u, 0u, 0x100u, 0x31016facu, 0x20000u, 0x100000u, 0x1000u, 0x31016facu};
    const auto compiled = [](std::span<const std::uint32_t> code, bool vertexSubgroups) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x5c000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 8;
        request.context.userData = userData;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32u;
        request.target.supportedCapabilities = capabilities;
        request.target.vertexSubgroups = vertexSubgroups;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv.Copy();
    };
    const auto usesSubgroups = [](std::span<const std::uint32_t> spirv) {
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            const auto op = spirv[cursor] & 0xffffu;
            if (op >= spv::OpGroupNonUniformElect && op <= spv::OpGroupNonUniformQuadSwap) return true;
        }
        return false;
    };
    static constexpr std::array<std::uint32_t, 41> elected{
        0xe00c2000u, 0x80030005u, 0xbf8c3f70u, 0x7e0c0d05u, 0xd5088006u, 0x02020cffu, 0x3d000000u, 0xbe94047eu, 0xbe9625c1u, 0x7e1402ffu,
        0xff800000u, 0xd501000au, 0x00520d0au, 0x201414fau, 0xff01110au, 0x201414fau, 0xff01120au, 0x201414fau, 0xff01140au, 0x201414fau,
        0xff01180au, 0xd7780009u, 0x0305830au, 0x2014130au, 0xbefe0416u, 0xd7600018u, 0x02013f0au, 0xd7600019u, 0x02017f0au, 0xd5100008u,
        0x02003218u, 0x7c240d08u, 0xbf880002u, 0xe0e00000u, 0x80020800u, 0xbefe0414u, 0xf80008cfu, 0x03020100u, 0xf800020fu, 0x00000000u,
        0xbf810000u,
    };
    const auto variant = [](std::initializer_list<std::pair<std::size_t, std::uint32_t>> words) {
        std::vector<std::uint32_t> code(elected.begin(), elected.end());
        for (const auto& [index, word] : words) code[index] = word;
        return code;
    };
    auto maskedAddress = variant({{32u, 0xbf880003u}, {33u, 0xe0e01000u}, {34u, 0x80020809u}});
    maskedAddress.insert(maskedAddress.begin() + 33, 0x7e120280u);
    auto unbranched = variant({});
    unbranched.erase(unbranched.begin() + 32);
    auto electedWrite = variant({{32u, 0xbf880003u}, {39u, 0x00000007u}});
    electedWrite.insert(electedWrite.begin() + 33, 0x7e0e02f2u);
    auto laneAddress = maskedAddress;
    laneAddress[33] = 0x7e120305u;
    auto negativeZero = variant({{4u, 0x7e0c02ffu}, {5u, 0x80000000u}});
    negativeZero.erase(negativeZero.begin() + 6);
    const std::array<std::pair<const char*, std::vector<std::uint32_t>>, 4> accepted{{
        {"buffer_atomic_umax", variant({})},
        {"buffer_atomic_smax", variant({{33u, 0xe0dc0000u}})},
        {"an address the permutation register keeps only in lanes without the atomic", maskedAddress},
        {"an atomic without s_cbranch_execz", unbranched},
    }};
    for (const auto& [name, code] : accepted) {
        const auto spirv = compiled(code, false);
        require(!spirv.empty() && !usesSubgroups(spirv), (std::string("single-lane elected atomic max: ") + name + " did not build without subgroup operations").c_str());
    }
    require(usesSubgroups(compiled(elected, true)), "single-lane elected atomic max: the reduction left subgroup operations with vertex subgroups");
    const std::array<std::pair<const char*, std::vector<std::uint32_t>>, 11> rejected{{
        {"keys without a clamp", variant({{4u, 0xd5080006u}})},
        {"an atomic whose result is read", variant({{33u, 0xe0e04000u}, {39u, 0x08080808u}})},
        {"a maximum exported beside the atomic", variant({{39u, 0x00000008u}})},
        {"buffer_atomic_add", variant({{33u, 0xe0c80000u}})},
        {"a comparison against another value", variant({{31u, 0x7c240308u}})},
        {"an atomic addressed by the permutation", variant({{33u, 0xe0e01000u}, {34u, 0x80020809u}})},
        {"a register written for the elected lanes and exported", electedWrite},
        {"an atomic indexed by the vertex", variant({{33u, 0xe0e02000u}, {34u, 0x80020805u}})},
        {"an address the elected lanes take from the vertex", laneAddress},
        {"a key of -0", negativeZero},
        {"a maximum written under EXEC -1", variant({{24u, 0xd7600018u}, {25u, 0x02013f0au}, {26u, 0xd7600019u}, {27u, 0x02017f0au}, {28u, 0xd5100008u}, {29u, 0x02003218u}, {30u, 0xbefe0416u}})},
    }};
    for (const auto& [name, code] : rejected) {
        expectFailure([&] { static_cast<void>(compiled(code, false)); }, "v_permlanex16_b32 from lanes 16-31 is outside the 1-lane host subgroup", (std::string("single-lane elected atomic max: ") + name + " was accepted").c_str());
    }
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
    for (const auto primitiveSize : {1u, 2u}) {
        otherMesh = mesh;
        otherMesh.outputPrimitiveSize = primitiveSize;
        other.graphics = GraphicsCompileContext{0u, {}, otherMesh, std::nullopt, {}};
        const auto primitiveReplay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(other));
        require(primitiveReplay.request.graphics.has_value() && primitiveReplay.request.graphics->mesh.has_value() && primitiveReplay.request.graphics->mesh->outputPrimitiveSize == primitiveSize, "the mesh output primitive size was lost in serialization");
        RecompileCacheKey::Build(other, key);
        require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "the cache keys ignore the mesh output primitive size");
    }
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
                        value.targetOutputMode, value.targetExportMapping, value.alphaToCoverage, value.stencilRefExportEnable, value.rasterPrimitive, value.layerExported, value.ditheredAlphaToCoverage, value.reversedBlend, value.targetMerge);
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
    pixel.targetMerge = {1u, 0x0ff0u, 1u, {0x100u, 0x2u, 0x3u, 0x4u, 0x5u, 0x6u, 0x7u, 0x8u}};
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
        auto movedMerge = request;
        movedMerge.context.pixel->targetMerge.descriptor[0] = 0x200u;
        RecompileCacheKey::Build(movedMerge, replayKey);
        require(key == replayKey, "the merged target's descriptor changed the static shader identity");
        auto depthWritingMerge = request;
        depthWritingMerge.context.pixel->targetMerge.depthStencilWrites = true;
        RecompileCacheKey::Build(depthWritingMerge, replayKey);
        require(key != replayKey, "the merge draw's depth or stencil writes did not change the static shader identity");
        auto depthTestingMerge = request;
        depthTestingMerge.context.pixel->targetMerge.depthStencilTests = true;
        RecompileCacheKey::Build(depthTestingMerge, replayKey);
        require(key != replayKey, "the merge draw's depth or stencil tests did not change the static shader identity");
        auto blending = request;
        blending.context.pixel->targetMerge.blend = {true, false, 6u, 7u, 0u, 1u, 0u, 0u, 0xfu, {}};
        RecompileCacheKey::Build(blending, replayKey);
        require(key != replayKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(blending), "an unclamped color target blend did not change the static shader identity");
        const auto blendKey = replayKey;
        for (const auto change : {0, 1, 2, 3, 4, 5, 6, 7, 8}) {
            auto other = blending;
            auto& blend = other.context.pixel->targetMerge.blend;
            if (change == 0) blend.snorm = true;
            if (change == 1) blend.colorSource = 2u;
            if (change == 2) blend.colorDestination = 2u;
            if (change == 3) blend.colorOperation = 1u;
            if (change == 4) blend.alphaSource = 2u;
            if (change == 5) blend.alphaDestination = 2u;
            if (change == 6) blend.alphaOperation = 1u;
            if (change == 7) blend.writeMask = 0x5u;
            if (change == 8) blend.constants[2] = 0x3f800000u;
            RecompileCacheKey::Build(other, replayKey);
            require(blendKey != replayKey, "a field of the unclamped color target blend did not change the static shader identity");
        }
        auto disabled = blending;
        disabled.context.pixel->targetMerge.blend.enabled = false;
        RecompileCacheKey::Build(disabled, replayKey);
        require(key == replayKey && RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(disabled), "a disabled color target blend changed the static shader identity");
        auto withoutMerge = request;
        withoutMerge.context.pixel->targetMerge = {};
        RecompileCacheKey::Build(withoutMerge, replayKey);
        require(key != replayKey, "a color target merge did not change the static shader identity");
        auto withoutCoverage = request;
        withoutCoverage.context.pixel->alphaToCoverage = false;
        RecompileCacheKey::Build(withoutCoverage, replayKey);
        require(key != replayKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(withoutCoverage), "alpha-to-coverage did not change the static shader identity");
        auto withoutStencilRef = request;
        withoutStencilRef.context.pixel->stencilRefExportEnable = false;
        RecompileCacheKey::Build(withoutStencilRef, replayKey);
        require(key != replayKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(withoutStencilRef), "the stencil reference export did not change the static shader identity");
    }
    auto blendedDepth = request;
    blendedDepth.context.pixel = pixel;
    blendedDepth.context.pixel->depthExportEnable = true;
    blendedDepth.context.pixel->depthExportMin = -0.25f;
    blendedDepth.context.pixel->depthExportMax = 0.75f;
    blendedDepth.context.pixel->targetMerge.blend = {true, true, 6u, 7u, 1u, 2u, 3u, 4u, 0xau, {0x3f800000u, 0x40000000u, 0x40400000u, 0x40800000u}};
    const auto blendedReplay = serializer.Deserialize(serializer.Serialize(blendedDepth));
    require(blendedReplay.request.context.pixel.has_value() && fields(*blendedReplay.request.context.pixel) == fields(*blendedDepth.context.pixel), "a pixel request with a target blend and depth export bounds lost pixel fields in serialization");
    require(blendedReplay.request.context.pixel->targetMerge.blend == blendedDepth.context.pixel->targetMerge.blend, "the target blend did not round-trip beside the depth export bounds");
    require(blendedReplay.request.context.pixel->depthExportMin == -0.25f && blendedReplay.request.context.pixel->depthExportMax == 0.75f, "the depth export bounds did not round-trip beside the target blend");
    require(blendedReplay.request.context.vertex.has_value() && blendedReplay.request.context.vertex->fetchAttribReg == 17u && blendedReplay.request.context.memory.size() == 1u && blendedReplay.request.context.memory[0].guestAddress == 0x60000u, "a target blend with depth export bounds displaced the following guest context");
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
    require(requestPrefix(encoded, 8u) == "NVNQQTEAAAA=", "new requests did not use serialization version 49");
    auto withBarycentric = minimal;
    withBarycentric.target.fragmentBarycentricBuiltins = true;
    require(serializer.Deserialize(serializer.Serialize(withBarycentric)).request.target.fragmentBarycentricBuiltins && !serializer.Deserialize(encoded).request.target.fragmentBarycentricBuiltins, "barycentric built-ins were lost in serialization");
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
    for (const auto unsupported : {"NVNQQQAAAAA=", "NVNQQTIAAAA="}) {
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

void verifyDepthExportClamp() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 3> code{0xf8001881u, 0x00000000u, 0xbf810000u};
    ShaderPixelStageInfo pixel{};
    pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.depthExportEnable = true;
    pixel.depthExportMin = 0.25f;
    pixel.depthExportMax = 0.75f;
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = Recompile(request);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> floats;
    std::vector<std::array<std::uint32_t, 3>> clamps;
    std::vector<std::uint32_t> stored;
    std::uint32_t depthVariable = 0;
    bool depthReplacing = false;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpConstant && (words[at] >> 16u) == 4u) floats[words[at + 2]] = words[at + 3];
        if (op == spv::OpExtInst && words[at + 4] == 81u) clamps.push_back({words[at + 2], words[at + 6], words[at + 7]});
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationBuiltIn && words[at + 3] == spv::BuiltInFragDepth) depthVariable = words[at + 1];
        if (op == spv::OpStore) stored.push_back(words[at + 1]), stored.push_back(words[at + 2]);
        if (op == spv::OpExecutionMode && words[at + 2] == spv::ExecutionModeDepthReplacing) depthReplacing = true;
    }
    require(depthVariable != 0u && depthReplacing, "depth export: the pixel shader does not replace gl_FragDepth");
    require(clamps.size() == 1u && floats[clamps[0][1]] == 0x3e800000u && floats[clamps[0][2]] == 0x3f400000u, "depth export: the exported depth was not NClamped to [ZMIN, ZMAX]");
    bool clampStored = false;
    for (std::size_t i = 0; i + 1 < stored.size(); i += 2) clampStored = clampStored || (stored[i] == depthVariable && stored[i + 1] == clamps[0][0]);
    require(clampStored, "depth export: gl_FragDepth was not written from the clamped value");
    const auto replay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(request));
    require(replay.request.context.pixel->depthExportMin == 0.25f && replay.request.context.pixel->depthExportMax == 0.75f, "depth export: the clamp did not survive serialization");
    {
        auto tail = request;
        tail.target.largePixelFunctionLdsBuffer = true;
        tail.target.pixelFunctionStorageDwords = 2048u;
        const auto both = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(tail));
        require(both.request.target.largePixelFunctionLdsBuffer && both.request.target.pixelFunctionStorageDwords == 2048u &&
                    both.request.context.pixel->depthExportMin == 0.25f && both.request.context.pixel->depthExportMax == 0.75f,
            "depth export: a version 49 request lost the bounds or the version 45 target tail");
        auto bytes = decodeBase64(RequestSerializer{}.Serialize(tail));
        require(bytes.size() > 8u && bytes[4] == 49u, "depth export: a depth exporting request did not use serialization version 49");
        static constexpr std::array<std::uint8_t, 8> boundBytes{0x00u, 0x00u, 0x80u, 0x3eu, 0x00u, 0x00u, 0x40u, 0x3fu};
        const auto found = std::search(bytes.begin(), bytes.end(), boundBytes.begin(), boundBytes.end());
        require(found != bytes.end() && std::search(found + 1, bytes.end(), boundBytes.begin(), boundBytes.end()) == bytes.end(),
            "depth export: the clamp bounds were not written exactly once");
        bytes.erase(found, found + boundBytes.size());
        for (const std::uint8_t legacyVersion : {48u, 47u, 46u}) {
            if (legacyVersion == 47u) bytes.pop_back();
            bytes[4] = legacyVersion;
            const auto legacy = RequestSerializer{}.Deserialize(encodeBase64(bytes));
            require(legacy.request.context.pixel->depthExportEnable && legacy.request.context.pixel->depthExportMin == 0.0f &&
                        legacy.request.context.pixel->depthExportMax == 1.0f && legacy.request.target.largePixelFunctionLdsBuffer &&
                        legacy.request.target.pixelFunctionStorageDwords == 2048u,
                legacyVersion == 46u   ? "depth export: a version 46 depth exporting request did not read with default bounds and its target tail"
                : legacyVersion == 47u ? "depth export: a version 47 depth exporting request did not read with default bounds and its target tail"
                                       : "depth export: a version 48 depth exporting request did not read with default bounds and its target tail");
        }
        for (const bool depthExport : {false, true}) {
            auto dithered = request;
            auto ditheredPixel = *dithered.context.pixel;
            ditheredPixel.depthExportEnable = depthExport;
            ditheredPixel.ditheredAlphaToCoverage = true;
            ditheredPixel.ditheredAlphaSamples = 2u;
            dithered.context.pixel = ditheredPixel;
            auto ditheredBytes = decodeBase64(RequestSerializer{}.Serialize(dithered));
            require(ditheredBytes.size() > 8u && ditheredBytes[4] == 49u, "a two-sample dithered request did not use serialization version 49");
            if (depthExport) {
                const auto bounds = std::search(ditheredBytes.begin(), ditheredBytes.end(), boundBytes.begin(), boundBytes.end());
                require(bounds != ditheredBytes.end(), "a two-sample dithered depth exporting request lacks the clamp bounds");
                ditheredBytes.erase(bounds, bounds + boundBytes.size());
            }
            ditheredBytes[4] = 48u;
            const auto main48 = RequestSerializer{}.Deserialize(encodeBase64(ditheredBytes));
            require(main48.request.context.pixel->ditheredAlphaToCoverage && main48.request.context.pixel->ditheredAlphaSamples == 2u && main48.request.context.pixel->depthExportEnable == depthExport &&
                        (!depthExport || (main48.request.context.pixel->depthExportMin == 0.0f && main48.request.context.pixel->depthExportMax == 1.0f)),
                "a version 48 two-sample dithered request, as main writes it with no depth tail, did not decode as written");
        }
    }
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    const auto first = key;
    auto other = request;
    auto changed = pixel;
    changed.depthExportMax = 1.0f;
    other.context.pixel = changed;
    RecompileCacheKey::Build(other, key);
    require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "depth export: the cache keys ignore the clamp");
    auto plain = pixel;
    plain.depthExportEnable = false;
    auto unexported = request;
    unexported.context.pixel = plain;
    RecompileCacheKey::Build(unexported, key);
    const auto plainKey = key;
    plain.depthExportMin = 0.0f;
    plain.depthExportMax = 1.0f;
    unexported.context.pixel = plain;
    RecompileCacheKey::Build(unexported, key);
    require(key == plainKey, "depth export: the clamp keyed a shader without depth export");
    auto open = pixel;
    open.depthExportMin = -std::numeric_limits<float>::infinity();
    open.depthExportMax = std::numeric_limits<float>::infinity();
    auto unclamped = request;
    unclamped.context.pixel = open;
    const auto unclampedResult = Recompile(unclamped);
    const auto& openWords = unclampedResult.spirv.Words();
    bool openClamp = false;
    bool openStore = false;
    std::uint32_t openDepth = 0;
    for (std::size_t at = 5; at < openWords.size() && (openWords[at] >> 16u) != 0; at += openWords[at] >> 16u) {
        const auto op = static_cast<spv::Op>(openWords[at] & 0xffffu);
        if (op == spv::OpExtInst && openWords[at + 4] == 81u) openClamp = true;
        if (op == spv::OpDecorate && openWords[at + 2] == spv::DecorationBuiltIn && openWords[at + 3] == spv::BuiltInFragDepth) openDepth = openWords[at + 1];
        if (op == spv::OpStore && openWords[at + 1] == openDepth && openDepth != 0u) openStore = true;
    }
    require(!openClamp && openStore, "depth export: an unclamped export (DISABLE_VIEWPORT_CLAMP) was clamped or not stored");
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
    version20Vertex.resize(version20Vertex.size() - sizeof(std::uint32_t));
    version20Vertex.pop_back();
    version20Vertex.pop_back();
    version20Vertex.pop_back();
    version20Vertex.pop_back();
    version20Vertex.pop_back();
    version20Vertex[4] = 20u;
    const auto replay20Vertex = serializer.Deserialize(encodeBase64(version20Vertex));
    require(replay20Vertex.request.context.vertex.has_value() && replay20Vertex.request.context.vertex->paClVsOutCntl == 0x89abcdefu, "a main version20 request lost PA_CL_VS_OUT_CNTL");
    require(serializer.Serialize(replay20Vertex.request) == marked, "a main version20 vertex request misread trailing fields");
    vertex.context.vertex->paClVsOutCntl = (1u << 18u) | (1u << 21u) | 0xfu;
    const auto layered = serializer.Deserialize(serializer.Serialize(vertex));
    require(layered.request.context.vertex.has_value() && layered.request.context.vertex->paClVsOutCntl == vertex.context.vertex->paClVsOutCntl, "a layered vertex info lost its clip distances or render target index");
    auto legacy = plain;
    legacy.resize(legacy.size() - sizeof(std::uint32_t));
    legacy.pop_back();
    legacy.pop_back();
    legacy.pop_back();
    legacy.pop_back();
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
    constexpr std::size_t iterationBytes = 35u;
    constexpr std::size_t sampleOrderBytes = 16u;
    constexpr std::size_t mergeBytes = 40u;
    constexpr std::size_t sampleMaskBytes = 16u;
    const auto orderOffset = blendOffset + 4u + iterationBytes;
    const auto mergeOffset = static_cast<std::ptrdiff_t>(orderOffset + sampleOrderBytes);
    auto merging = pixel;
    merging.context.pixel->targetMerge = {3u, 0xf00fu, 2u, {1u, 2u, 3u, 4u, 5u, 6u, 7u, 0x89abcdefu}, true, true};
    const auto mergingBytes = decodeBase64(serializer.Serialize(merging));
    require(mergingBytes.size() == plainPixel.size() && std::equal(plainPixel.begin(), plainPixel.begin() + mergeOffset, mergingBytes.begin()) && mergingBytes[mergeOffset] == 3u && mergingBytes[mergeOffset + 1] == 0x0fu && mergingBytes[mergeOffset + 2] == 0xf0u && mergingBytes[mergeOffset + 5] == 2u && mergingBytes[mergeOffset + 6] == 1u && mergingBytes[mergeOffset + 34] == 0xefu && mergingBytes[mergeOffset + 37] == 0x89u && mergingBytes[mergeOffset + 38] == 1u && mergingBytes[mergeOffset + 39] == 1u && std::equal(plainPixel.begin() + mergeOffset + static_cast<std::ptrdiff_t>(mergeBytes), plainPixel.end(), mergingBytes.begin() + mergeOffset + static_cast<std::ptrdiff_t>(mergeBytes)), "the color target merge is not the 40 bytes the version 46 pixel info adds after the sample order");
    require(serializer.Deserialize(serializer.Serialize(merging)).request.context.pixel->targetMerge == merging.context.pixel->targetMerge, "a serialized request lost its color target merge");
    auto blendMerging = pixel;
    blendMerging.context.pixel->targetMerge = {1u, 0xffffu, 1u, {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}, true, false, {true, true, 6u, 7u, 1u, 1u, 0u, 4u, 0xbu, {0x3fc00000u, 0xbe800000u, 0x3f000000u, 0x40000000u}}};
    const auto blendMergingBytes = decodeBase64(serializer.Serialize(blendMerging));
    constexpr std::size_t blendPayloadBytes = 24u;
    require(blendMergingBytes.size() == plainPixel.size() + blendPayloadBytes && std::equal(plainPixel.begin(), plainPixel.begin() + mergeOffset, blendMergingBytes.begin()) && blendMergingBytes[mergeOffset] == 1u && blendMergingBytes[mergeOffset + 1] == 0xffu && blendMergingBytes[mergeOffset + 2] == 0xffu && blendMergingBytes[mergeOffset + 3] == 0x01u && blendMergingBytes[mergeOffset + 4] == 0x00u, "a color target blend is not flagged in the merge mask word, or changed the bytes before the merge");
    require(serializer.Deserialize(serializer.Serialize(blendMerging)).request.context.pixel->targetMerge == blendMerging.context.pixel->targetMerge, "a serialized request lost its color target blend");
    auto disabledBlend = merging;
    disabledBlend.context.pixel->targetMerge.blend = {false, true, 6u, 7u, 1u, 1u, 0u, 4u, 0xbu, {1u, 2u, 3u, 4u}};
    require(serializer.Serialize(disabledBlend) == serializer.Serialize(merging), "a disabled color target blend changed the serialized request");
    // A version 43 request: no color target merge, and no fused arithmetic flag, pixel function LDS flag, storage size or barycentric built-ins flag at the end.
    const auto version43Bytes = [&](std::vector<std::uint8_t> bytes) {
        require(bytes.end() - bytes.begin() > mergeOffset + static_cast<std::ptrdiff_t>(mergeBytes) && std::equal(plainPixel.begin() + mergeOffset, plainPixel.begin() + mergeOffset + static_cast<std::ptrdiff_t>(mergeBytes), bytes.begin() + mergeOffset), "a replay fixture does not hold the default color target merge after the sample order");
        bytes.erase(bytes.begin() + mergeOffset, bytes.begin() + mergeOffset + static_cast<std::ptrdiff_t>(mergeBytes));
        bytes.resize(bytes.size() - sizeof(std::uint32_t) - 3u);
        return bytes;
    };
    auto version42 = version43Bytes(plainPixel);
    version42[4] = 42u;
    const auto replay42 = serializer.Deserialize(encodeBase64(version42));
    require(replay42.request.context.pixel.has_value() && replay42.request.context.pixel->targetMerge == TargetMerge{} && serializer.Serialize(replay42.request) == encodedPixel, "a version 42 request read a color target merge or misread the fields after it");
    auto version39 = version42;
    version39.pop_back();
    version39.erase(version39.begin() + static_cast<std::ptrdiff_t>(orderOffset), version39.begin() + mergeOffset);
    version39[4] = 39u;
    const auto replay39 = serializer.Deserialize(encodeBase64(version39));
    require(replay39.request.context.pixel.has_value() && replay39.request.context.pixel->targetMerge == TargetMerge{} && replay39.request.context.pixel->sampleOrder == std::array<std::uint32_t, 4>{} && serializer.Serialize(replay39.request) == encodedPixel, "a version 39 request read a sample order or color target merge, or misread the fields after them");
    auto ordered = pixel;
    ordered.context.pixel->sampleOrder = {0x76543210u, 0x76543210u, 0x76543201u, 0x76543201u};
    const auto orderedBytes = decodeBase64(serializer.Serialize(ordered));
    require(orderedBytes.size() == plainPixel.size() && std::equal(plainPixel.begin(), plainPixel.begin() + static_cast<std::ptrdiff_t>(orderOffset), orderedBytes.begin()) && orderedBytes[orderOffset] == 0x10u && orderedBytes[orderOffset + 11u] == 0x76u && orderedBytes[orderOffset + 12u] == 0x01u && std::equal(plainPixel.begin() + static_cast<std::ptrdiff_t>(orderOffset + sampleOrderBytes), plainPixel.end(), orderedBytes.begin() + static_cast<std::ptrdiff_t>(orderOffset + sampleOrderBytes)), "the sample order is not the four words the version 42 pixel info adds after the stencil operation value export flag");
    auto orderedVersion34 = version43Bytes(orderedBytes);
    orderedVersion34.pop_back();
    orderedVersion34.pop_back();
    orderedVersion34.pop_back();
    orderedVersion34.erase(orderedVersion34.begin() + static_cast<std::ptrdiff_t>(orderOffset - 1u), orderedVersion34.begin() + static_cast<std::ptrdiff_t>(orderOffset + sampleOrderBytes));
    orderedVersion34[4] = 34u;
    const auto orderedReplay34 = serializer.Deserialize(encodeBase64(orderedVersion34));
    require(orderedReplay34.request.context.pixel.has_value() && orderedReplay34.request.context.pixel->sampleOrder == std::array<std::uint32_t, 4>{} && serializer.Serialize(orderedReplay34.request) == encodedPixel, "a version 34 request read a sample order, or misread the fields after its pixel iteration fields");
    auto orderedVersion30 = version43Bytes(orderedBytes);
    orderedVersion30.resize(orderedVersion30.size() - 4u);
    orderedVersion30.erase(orderedVersion30.begin() + static_cast<std::ptrdiff_t>(blendOffset + 4u), orderedVersion30.begin() + static_cast<std::ptrdiff_t>(orderOffset + sampleOrderBytes));
    orderedVersion30[4] = 30u;
    const auto orderedReplay30 = serializer.Deserialize(encodeBase64(orderedVersion30));
    require(orderedReplay30.request.context.pixel.has_value() && orderedReplay30.request.context.pixel->sampleOrder == std::array<std::uint32_t, 4>{} && serializer.Serialize(orderedReplay30.request) == encodedPixel, "a version 30 request read a sample order, or misread the fields after its reversed-order blend code");
    auto orderedSpanned = ordered;
    orderedSpanned.target.spannedWaves = true;
    auto orderedVersion32 = version43Bytes(decodeBase64(serializer.Serialize(orderedSpanned)));
    orderedVersion32.pop_back();
    require(orderedVersion32.size() > 2u && orderedVersion32.back() == 0u && orderedVersion32[orderedVersion32.size() - 2u] == 0u && orderedVersion32[orderedVersion32.size() - 3u] == 1u, "the spanned-waves flag is not the byte before the depth remap flag");
    orderedVersion32.pop_back();
    orderedVersion32.pop_back();
    orderedVersion32.erase(orderedVersion32.begin() + static_cast<std::ptrdiff_t>(blendOffset + 4u), orderedVersion32.begin() + static_cast<std::ptrdiff_t>(orderOffset + sampleOrderBytes));
    orderedVersion32[4] = 32u;
    const auto orderedReplay32 = serializer.Deserialize(encodeBase64(orderedVersion32));
    auto unordered = pixel;
    unordered.target.spannedWaves = true;
    require(orderedReplay32.request.context.pixel.has_value() && orderedReplay32.request.context.pixel->sampleOrder == std::array<std::uint32_t, 4>{} && orderedReplay32.request.target.spannedWaves && serializer.Serialize(orderedReplay32.request) == serializer.Serialize(unordered), "a version 32 request read a sample order or lost its spanned-waves flag");
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
    auto version30 = version43Bytes(sampledBytes);
    version30.pop_back();
    version30.pop_back();
    version30.pop_back();
    version30.pop_back();
    version30.erase(version30.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u), version30.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
    version30[4] = 30u;
    const auto replay30 = serializer.Deserialize(encodeBase64(version30));
    require(replay30.request.context.pixel.has_value() && !replay30.request.context.pixel->iteratesSamples && replay30.request.context.pixel->positionLocation == PixelPositionLocation::Center && replay30.request.context.pixel->sampleLocations == std::array<std::uint8_t, 32>{} && decodeBase64(serializer.Serialize(replay30.request)) == sampledBytes, "a version 30 request read pixel iteration, a position location or sample locations, or misread the fields after its reversed-order blend code");
    auto spanned = sampledPixel;
    spanned.target.spannedWaves = true;
    auto version32 = version43Bytes(decodeBase64(serializer.Serialize(spanned)));
    version32.pop_back();
    require(version32.size() > 2u && version32.back() == 0u && version32[version32.size() - 2u] == 0u && version32[version32.size() - 3u] == 1u, "the spanned-waves flag is not the byte before the depth remap flag");
    version32.pop_back();
    version32.pop_back();
    version32.erase(version32.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u), version32.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
    version32[4] = 32u;
    const auto replay32 = serializer.Deserialize(encodeBase64(version32));
    require(replay32.request.context.pixel.has_value() && !replay32.request.context.pixel->iteratesSamples && replay32.request.context.pixel->sampleLocations == std::array<std::uint8_t, 32>{} && replay32.request.target.spannedWaves && serializer.Serialize(replay32.request) == serializer.Serialize(spanned), "a version 32 request read pixel iteration or sample locations, or lost its spanned-waves flag");
    auto version29 = version43Bytes(sampledBytes);
    version29.pop_back();
    version29.pop_back();
    version29.pop_back();
    version29.pop_back();
    version29.erase(version29.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes), version29.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
    auto version36 = version43Bytes(sampledBytes);
    version36.pop_back();
    version36.erase(version36.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes - 1u), version36.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
    version36[4] = 36u;
    const auto replay36 = serializer.Deserialize(encodeBase64(version36));
    require(replay36.request.context.pixel.has_value() && !replay36.request.context.pixel->stencilOperationValueExport && decodeBase64(serializer.Serialize(replay36.request)) == sampledBytes, "a version 36 request read a stencil operation value export or misread the fields after the sample locations");
    version29[4] = 29u;
    const auto replay29 = serializer.Deserialize(encodeBase64(version29));
    require(replay29.request.context.pixel.has_value() && replay29.request.context.pixel->quadSampleMask == std::array<std::uint16_t, 4>{0x1u, 0x2u, 0x4u, 0x8u} && replay29.request.context.pixel->reversedBlend == 0u && decodeBase64(serializer.Serialize(replay29.request)) == sampledBytes, "a version 29 request read a reversed-order blend code or misread the fields after the quad sample masks");
    auto version28 = version43Bytes(sampledBytes);
    version28.pop_back();
    version28.pop_back();
    version28.pop_back();
    version28.pop_back();
    version28.erase(version28.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u), version28.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
    version28[4] = 28u;
    const auto replay28 = serializer.Deserialize(encodeBase64(version28));
    require(replay28.request.context.pixel.has_value() && replay28.request.context.pixel->quadPixelMask == 0x2u && replay28.request.context.pixel->quadSampleMask == std::array<std::uint16_t, 4>{0xffffu, 0xffffu, 0xffffu, 0xffffu} && replay28.request.context.pixel->reversedBlend == 0u && decodeBase64(serializer.Serialize(replay28.request)) == maskedBytes, "a version 28 request read quad sample masks or a reversed-order blend code, or misread the fields after them");
    auto version27 = version43Bytes(maskedBytes);
    version27.pop_back();
    version27.pop_back();
    version27.pop_back();
    version27.pop_back();
    version27.erase(version27.begin() + static_cast<std::ptrdiff_t>(maskOffset), version27.begin() + static_cast<std::ptrdiff_t>(maskOffset + 1u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
    version27[4] = 27u;
    const auto replay27 = serializer.Deserialize(encodeBase64(version27));
    require(replay27.request.context.pixel.has_value() && replay27.request.context.pixel->quadPixelMask == 0xfu && replay27.request.context.pixel->reversedBlend == 0u && serializer.Serialize(replay27.request) == encodedPixel, "a version 27 request read a quad pixel mask or reversed-order blend code, or misread the fields after it");
    auto legacyPixel = version43Bytes(plainPixel);
    legacyPixel.pop_back();
    legacyPixel.pop_back();
    legacyPixel.pop_back();
    legacyPixel.pop_back();
    legacyPixel.erase(legacyPixel.begin() + static_cast<std::ptrdiff_t>(allBitsOffset), legacyPixel.begin() + static_cast<std::ptrdiff_t>(allBitsOffset + 2u + sampleMaskBytes + 4u + iterationBytes + sampleOrderBytes));
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

void verifyDepthRemapRequests() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    const RequestSerializer serializer;
    RecompileRequest vertex{};
    vertex.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
    vertex.context.waveSize = 64;
    vertex.context.vertex = ShaderVertexStageInfo{};
    vertex.context.vertex->paClVsOutCntl = 0x89abcdefu;
    const auto plain = serializer.Serialize(vertex);
    vertex.target.depthRangeRemap = true;
    vertex.context.vertex->depthRemapMode = 2u;
    const auto remapped = serializer.Serialize(vertex);
    const auto replay = serializer.Deserialize(remapped);
    require(replay.request.target.depthRangeRemap && replay.request.context.vertex.has_value() && replay.request.context.vertex->paClVsOutCntl == 0x89abcdefu, "the depth remap target flag or the vertex info did not round-trip");
    require(replay.request.context.vertex->depthRemapMode == 2u, "the depth remap mode did not round-trip");
    require(serializer.Serialize(replay.request) == remapped, "the depth remap request changed on re-serialization");
    const auto plainBytes = decodeBase64(plain);
    const auto remappedBytes = decodeBase64(remapped);
    constexpr std::size_t pointTailBytes = 13u;
    require(plainBytes.size() == remappedBytes.size() && plainBytes.size() > 14u + pointTailBytes && std::equal(plainBytes.begin(), plainBytes.end() - 14 - pointTailBytes, remappedBytes.begin()) && std::equal(plainBytes.end() - 7 - pointTailBytes, plainBytes.end(), remappedBytes.end() - 7 - pointTailBytes), "the depth remap flag and mode are not the 6 bytes before the f32 flush and denormal preserve flags, the point tail, the fused arithmetic flag, the pixel function LDS flag, the pixel function storage size and the barycentric built-in flag of a vertex request");
    for (const std::uint8_t legacyVersion : {32u, 34u}) {
        auto legacyBytes = remappedBytes;
        legacyBytes.resize(legacyBytes.size() - sizeof(std::uint32_t));
        legacyBytes.pop_back();
        legacyBytes.pop_back();
        legacyBytes.pop_back();
        legacyBytes.pop_back();
        legacyBytes[4] = legacyVersion;
        legacyBytes.resize(legacyBytes.size() - 6u - pointTailBytes);
        const auto legacy = serializer.Deserialize(encodeBase64(legacyBytes));
        require(!legacy.request.target.depthRangeRemap && legacy.request.context.vertex.has_value() && legacy.request.context.vertex->depthRemapMode == 0u, ("a version " + std::to_string(legacyVersion) + " vertex request read depth remap fields").c_str());
        require(serializer.Serialize(legacy.request) == plain, ("a version " + std::to_string(legacyVersion) + " vertex request misread the fields before the depth remap").c_str());
    }
    auto version35 = remappedBytes;
    version35.resize(version35.size() - sizeof(std::uint32_t));
    version35.pop_back();
    version35.pop_back();
    version35.pop_back();
    version35.pop_back();
    version35[4] = 35u;
    version35.resize(version35.size() - 1u - pointTailBytes);
    const auto replay35 = serializer.Deserialize(encodeBase64(version35));
    require(replay35.request.target.depthRangeRemap && replay35.request.context.vertex->depthRemapMode == 2u && !replay35.request.target.float32FlushToZero && serializer.Serialize(replay35.request) == remapped, "a version 35 request misread the depth remap fields or read an f32 flush flag");
    auto truncated = version35;
    truncated.resize(truncated.size() - 2u);
    expectFailure([&] { static_cast<void>(serializer.Deserialize(encodeBase64(truncated))); }, "truncated data", "a truncated depth remap mode was accepted");
    auto trailing = remappedBytes;
    trailing.push_back(0u);
    expectFailure([&] { static_cast<void>(serializer.Deserialize(encodeBase64(trailing))); }, "unexpected trailing data", "a byte after the depth remap fields was accepted");
    RecompileRequest pixel{};
    pixel.shader = {ShaderStage::Fragment, 0x20000u, code, 0, {}};
    pixel.context.waveSize = 64;
    pixel.context.pixel = ShaderPixelStageInfo{};
    pixel.target.depthRangeRemap = true;
    const auto pixelReplay = serializer.Deserialize(serializer.Serialize(pixel));
    require(pixelReplay.request.target.depthRangeRemap && !pixelReplay.request.context.vertex.has_value(), "a pixel request lost the depth remap target flag or gained vertex info");
    std::vector<std::uint64_t> key;
    std::vector<std::uint64_t> other;
    RecompileCacheKey::Build(vertex, key);
    auto zeroToOne = vertex;
    zeroToOne.context.vertex->depthRemapMode = 1u;
    RecompileCacheKey::Build(zeroToOne, other);
    require(key != other, "the depth remap mode did not change the shader cache key");
}

void verifyMergedHullConfiguration() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 1> code{0xbf810000u};
    const RequestSerializer serializer;
    RecompileRequest request{};
    request.shader = {ShaderStage::TessellationControl, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.vertex = ShaderVertexStageInfo{};
    request.graphics = GraphicsCompileContext{0u, {}, std::nullopt, TessellationConfiguration{3u, 3u, 1u, 2u, 2u}, {}};
    const auto separate = serializer.Serialize(request);
    std::vector<std::uint64_t> separateKey;
    RecompileCacheKey::Build(request, separateKey);
    request.graphics->tessellation->mergedHull = true;
    const auto merged = serializer.Serialize(request);
    std::vector<std::uint64_t> mergedKey;
    RecompileCacheKey::Build(request, mergedKey);
    const auto replay = serializer.Deserialize(merged);
    require(replay.request.graphics.has_value() && replay.request.graphics->tessellation.has_value() && replay.request.graphics->tessellation->mergedHull, "a merged hull configuration was lost in serialization");
    require(!serializer.Deserialize(separate).request.graphics->tessellation->mergedHull, "a separate hull configuration was read as merged");
    constexpr std::size_t pointTailBytes = 13u;
    auto legacy = decodeBase64(merged);
    require(legacy.size() > 55u + pointTailBytes && legacy[4] == 49u, "a merged hull request did not use serialization version 49");
    legacy.resize(legacy.size() - sizeof(std::uint32_t) - 3u - pointTailBytes);
    legacy.pop_back();
    require(legacy[legacy.size() - 52u] == 1u, "the merged hull flag is not the last byte of the tessellation configuration");
    legacy.erase(legacy.end() - 52);
    for (const auto version : {30u, 31u, 32u, 34u, 35u, 36u, 37u, 38u}) {
        auto older = legacy;
        older[4] = static_cast<std::uint8_t>(version);
        if (version < 38u) older.erase(older.end() - 31);
        if (version < 36u) older.pop_back();
        if (version < 35u) older.resize(older.size() - 5u);
        if (version < 32u) older.pop_back();
        const auto upgraded = serializer.Deserialize(encodeBase64(older));
        const auto what = "a main version " + std::to_string(version) + " tessellation request";
        require(upgraded.request.graphics.has_value() && upgraded.request.graphics->tessellation.has_value() && !upgraded.request.graphics->tessellation->mergedHull, (what + " read a merged hull flag").c_str());
        require(serializer.Serialize(upgraded.request) == separate, (what + " misread the fields after its tessellation configuration").c_str());
    }
    require(mergedKey != separateKey && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(serializer.Deserialize(separate).request), "the cache keys ignore whether the hull program is merged");
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
        require(request.useCache == (version == 1u) && request.target.nonConstantImageOffsets == (version >= 6u) && request.target.srgbDecodeFormats == 0u && !request.target.narrowSubgroupClock && !request.target.float64DenormPreserve && !request.target.registrationModules && !request.target.float16DenormPreserve && !request.target.fragmentBarycentricBuiltins, "legacy request trailer was misread");
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
        const auto memoryRegions = memory.Regions();
        request.context.memory = memoryRegions;
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
        const auto memoryRegions = memory.Regions();
        request.context.memory = memoryRegions;
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

void verifySpecializedBufferAtomicBindings(bool finalAbiOnly = false) {
    using namespace ShaderRecompiler;
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const std::array<std::uint32_t, 4> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityInt64Atomics};
    alignas(256) static std::array<std::uint32_t, 64> buffer{};
    const auto bufferAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(buffer.data()));
    const std::array<std::uint32_t, 16> userData{
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(bufferAddress), static_cast<std::uint32_t>((bufferAddress >> 32u) & 0xffffu), 256u, 0x31016facu,
        0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u};
    const auto compile = [&](std::span<const std::uint32_t> supported, std::uint32_t opcode = 0xe0f05000u) {
        const std::array<std::uint32_t, 3> code{opcode, 0x80012803u, 0xbf810000u};
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
    if (finalAbiOnly) {
        for (const auto opcode : {0xe0f05000u, 0xe1705000u}) {
            const auto result = compile(capabilities, opcode);
            const auto faults = std::ranges::count_if(result.bindings, [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::FaultBuffer; });
            require(faults <= 1, "specialized buffer atomic: duplicate fault buffers");
            require(result.bdaAbiVersion == (faults == 0 ? 0u : BdaAbi::Version), "specialized buffer atomic: final fault binding and BDA ABI disagree");
            if (opcode == 0xe1705000u) require(faults == 1, "specialized buffer atomic: a 64-bit atomic lost its live fault buffer");
        }
        return;
    }
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
    require(faultCalls == 0, "specialized buffer atomic: a 32-bit atomic records an unaligned fault instead of acting on its dword");
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
        const auto memoryRegions = memory.Regions();
        request.context.memory = memoryRegions;
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
    const auto reservedDword3 = [&](std::uint32_t mimg) {
        auto code = program(mimg);
        const std::array<std::uint32_t, 4> computed{0x7d8a0080u, 0x870bff6au, 0x000000ffu, 0x8f0b8c0bu};
        code.insert(code.begin(), computed.begin(), computed.end());
        return code;
    };
    const auto adjusted = recompile(reservedDword3(0xf0800f09u), userData(normalized));
    require(proven(adjusted, false), "sampler dword 3: an image_sample_a whose S# dword 3 holds only a ballot in its reserved bits was not compiled");
    expectFailure([&] { static_cast<void>(recompile(reservedDword3(0xf0800f08u), userData(normalized))); }, "GetSamplerResource dword 3 is not a valid runtime value", "sampler dword 3: an image_sample whose S# dword 3 holds a ballot was accepted");
    for (const auto& [adjust, plain, compiled, accepted] : {
             std::tuple{0xf1200109u, 0xf1200108u, "sampler dword 3: an image_gather4_c_a whose S# dword 3 holds only a ballot in its reserved bits was not compiled", "sampler dword 3: an image_gather4_c with a computed S# dword 3 was accepted"},
             std::tuple{0xf1600109u, 0xf1600108u, "sampler dword 3: an image_gather4_c_o_a whose S# dword 3 holds only a ballot in its reserved bits was not compiled", "sampler dword 3: an image_gather4_c_o with a computed S# dword 3 was accepted"}}) {
        require(proven(recompile(reservedDword3(adjust), imageData(normalized, Type2D, Format32Float, 0u)), false), compiled);
        expectFailure([&] { static_cast<void>(recompile(reservedDword3(plain), imageData(normalized, Type2D, Format32Float, 0u))); }, "GetSamplerResource dword 3 is not a valid runtime value", accepted);
    }
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
    const std::array<std::uint32_t, 7> pixelOffsetCode{0x7e0202ffu, 0x00000201u, 0xf0c00f08u, 0x00400801u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};
    pixel.shader = {ShaderStage::Fragment, 0x50000u, pixelOffsetCode, 0, {}};
    pixel.context.userData = pixelData;
    pixel.target.supportedCapabilities = capabilities;
    pixel.target.nonConstantImageOffsets = true;
    const auto pixelOffsetUnnormalized = Recompile(pixel);
    require(proven(pixelOffsetUnnormalized, true), "unnormalized texel offsets: a pixel image_sample_o through an unnormalized S# was not flagged");
    auditSpirv(pixelOffsetUnnormalized.spirv.Words());
    require(foldedSamples(pixelOffsetUnnormalized.spirv.Words()), "unnormalized texel offsets: a pixel image_sample_o offset was not added to the base-level coordinates");
    pixel.context.userData = normalizedPixelData;
    const auto pixelOffsetNormalized = Recompile(pixel);
    std::vector<std::uint32_t> implicitMasks;
    const auto offsetWords = pixelOffsetNormalized.spirv.Words();
    for (std::size_t cursor = 5; cursor < offsetWords.size();) {
        const auto count = offsetWords[cursor] >> 16u;
        require(count != 0 && count <= offsetWords.size() - cursor, "unnormalized texel offsets: truncated SPIR-V instruction");
        if ((offsetWords[cursor] & 0xffffu) == spv::OpImageSampleImplicitLod && count > 5u) implicitMasks.push_back(offsetWords[cursor + 5]);
        cursor += count;
    }
    require(proven(pixelOffsetNormalized, false) && implicitMasks.size() == 1u && (implicitMasks[0] & (spv::ImageOperandsConstOffsetMask | spv::ImageOperandsOffsetMask)) != 0u, "unnormalized texel offsets: a normalized pixel image_sample_o is not an implicit-LOD sample with its offset");
}

void verifyNullVolumeUnnormalizedSampler() {
    using namespace ShaderRecompiler;
    ImageResource volume{};
    volume.resourceClass = ImageResourceClass::Sampled;
    volume.numericClass = IrTextureNumericClass::Float;
    volume.dimension = RdnaImageDimension::Dim3D;
    volume.read = true;
    ImageResource plane = volume;
    plane.dimension = RdnaImageDimension::Dim2D;
    SamplerResource sampler{};
    sampler.uses = SamplerUseExplicitLod;
    const DescriptorValue nullDescriptor{{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}, 8u};
    const DescriptorValue volumeDescriptor{{0x00001000u, 0x03800000u, 0x0000c000u, 0xa0000facu, 3u, 0u, 0u, 0u}, 8u};
    const DescriptorValue planeDescriptor{{0x00002000u, 0x03800000u, 0x0000c000u, 0x90000facu, 0u, 0u, 0u, 0u}, 8u};
    const DescriptorValue unnormalized{{0x00008092u, 0x00fff000u, 0x02500000u, 0u}, 4u};
    const auto populate = [&](std::vector<ImageResource> images, std::vector<DescriptorValue> descriptors) {
        ShaderInfo info;
        info.images = images;
        info.samplers = {sampler};
        for (std::uint32_t index = 0; index < images.size(); ++index) {
            info.sampledPairs.push_back({index, 0u, 0u});
            info.runtimeImageModes.push_back(ResourceMaterializer::RuntimeImageModes(images[index]));
        }
        ResourceSnapshot snapshot;
        snapshot.images = descriptors;
        snapshot.samplers = {unnormalized};
        BindingAllocationResult allocation;
        for (std::uint32_t index = 0; index < images.size(); ++index) allocation.layout.descriptors.push_back({DescriptorBindingForImage(images[index]), {index}});
        allocation.layout.descriptors.push_back({DescriptorBindingKind::Samplers, {0u}});
        DescriptorBindingBuilder{}.Populate(allocation, info, IrShaderStage::Compute, 0u, snapshot, {});
        return allocation.bindings.back();
    };
    const auto nullOnly = populate({volume}, {nullDescriptor});
    require(nullOnly.samplerUnnormalized == std::vector<bool>{false} && nullOnly.samplerCoordinatesIgnored == std::vector<bool>{true}, "unnormalized samplers: an S# paired only with a null 3D T# was refused or kept its unnormalized coordinates");
    const DescriptorValue zeroSwizzle{{0x00003000u, 0x00100000u, 0u, 0xa0000000u, 0u, 0u, 0u, 0u}, 8u};
    const auto constantAndNull = populate({volume, volume}, {zeroSwizzle, nullDescriptor});
    require(constantAndNull.samplerUnnormalized == std::vector<bool>{false} && constantAndNull.samplerCoordinatesIgnored == std::vector<bool>{true}, "unnormalized samplers: an S# paired with a null 3D T# and a 3D T# whose channels all select constants was refused or kept its unnormalized coordinates");
    expectFailure([&] { static_cast<void>(populate({volume}, {volumeDescriptor})); }, "samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a 3D T# was accepted");
    expectFailure([&] { static_cast<void>(populate({volume, plane}, {nullDescriptor, planeDescriptor})); }, "samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: an S# shared by a null 3D T# and a 2D T# was accepted");
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
    ImageResource msaaArrayImage{};
    msaaArrayImage.resourceClass = ImageResourceClass::Sampled;
    msaaArrayImage.numericClass = IrTextureNumericClass::Float;
    msaaArrayImage.dimension = RdnaImageDimension::Dim2DMsaaArray;
    msaaArrayImage.read = true;
    ShaderInfo msaaArrayInfo;
    msaaArrayInfo.images = {msaaArrayImage};
    msaaArrayInfo.runtimeImageModes = {ResourceMaterializer::RuntimeImageModes(msaaArrayImage)};
    BindingAllocationResult msaaArrayAllocation;
    msaaArrayAllocation.layout.descriptors = {{DescriptorBindingForImage(msaaArrayImage), {0u}}};
    auto msaaArraySnapshot = snapshot;
    msaaArraySnapshot.images[0].dwords[3] = 0xf0020facu;
    msaaArraySnapshot.images[0].dwords[5] = 0x00000020u;
    DescriptorBindingBuilder{}.Populate(msaaArrayAllocation, msaaArrayInfo, IrShaderStage::Compute, 0u, msaaArraySnapshot, {});
    require(msaaArrayAllocation.bindings.size() == 1u && msaaArrayAllocation.bindings[0].imageShape == DescriptorImageShape::Image2DMultisampleArray, "multisampled array image resource did not receive Image2DMultisampleArray descriptor shape");
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
        const auto memoryRegions = memory.Regions();
        request.context.memory = memoryRegions;
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

void verifyBdaAnchors() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    alignas(256) static std::array<std::uint32_t, 32> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 8> userData{0x10000000u, 0u, 0u, 0u, static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 128u, 0x01016facu};
    const auto compile = [&](std::uint32_t scalarBase, std::uint32_t stride) {
        std::vector<std::uint32_t> code{0x7e020200u, 0x7e040201u};
        for (std::uint32_t load = 0; load < scalarBase; ++load) {
            code.push_back(0xdc308000u | (stride * load + 4u));
            code.push_back(((3u + load) << 24u) | 0x00040000u);
        }
        code.push_back(0xdc308000u | 4u);
        code.push_back(((3u + scalarBase) << 24u) | 0x007d0001u);
        code.push_back(0xbf8c3f70u);
        for (std::uint32_t load = 1; load <= scalarBase; ++load) code.push_back(0x4a060103u | ((3u + load) << 9u));
        code.insert(code.end(), {0xe0700000u, 0x80010300u, 0xbf810000u});
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x60000u, code, 0, {}};
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
    for (const std::uint32_t stride : {4u, 32u}) {
        for (const std::uint32_t loads : {0u, 1u, 2u, 4u}) {
            const bool anchored = stride == 32u && loads >= 2u;
            const auto words = compile(loads, stride);
            std::map<std::uint32_t, std::string> names;
            std::string function;
            std::map<std::string, std::size_t> mainCalls;
            std::size_t anchorFunctions = 0;
            std::size_t branchesBeforeAnchor = 0;
            std::size_t directLoads = 0;
            for (std::size_t cursor = 5; cursor < words.size();) {
                const auto length = words[cursor] >> 16u;
                require(length != 0 && length <= words.size() - cursor, "BDA anchors: truncated SPIR-V instruction");
                const auto op = words[cursor] & 0xffffu;
                if (op == spv::OpName) names[words[cursor + 1]] = reinterpret_cast<const char*>(&words[cursor + 2]);
                if (op == spv::OpFunction) {
                    function = names[words[cursor + 2]];
                    if (function == "bda_anchor_range") ++anchorFunctions;
                }
                if (op == spv::OpFunctionCall && function == "main") {
                    const auto& callee = names[words[cursor + 3]];
                    if (callee == "bda_anchor_range") require(mainCalls.empty(), "BDA anchors: the anchor is searched after a read");
                    ++mainCalls[callee.starts_with("read_bda_span") ? std::string("read_bda_span") : callee];
                }
                if (op == spv::OpBranchConditional && function == "main" && mainCalls["bda_anchor_range"] == 0u) ++branchesBeforeAnchor;
                if (op == spv::OpLoad && function == "main" && length > 4u && words[cursor + 4] == spv::MemoryAccessAlignedMask && words[cursor + 5] == 4u) ++directLoads;
                cursor += length;
            }
            const auto spans = loads == 0u ? 1u : stride == 4u ? 2u : loads + 1u;
            require(anchorFunctions == (anchored ? 1u : 0u), "BDA anchors: the anchor search is defined without anchored reads, or twice");
            require(mainCalls["bda_anchor_range"] == (anchored ? 1u : 0u), "BDA anchors: the reads of one user data base do not share one anchor");
            require(!anchored || branchesBeforeAnchor == 0u, "BDA anchors: the anchor is not searched at the program's entry");
            require(mainCalls["read_bda_span"] == spans, "BDA anchors: a read lost its lookup path");
            require(directLoads == (anchored ? loads : 0u), "BDA anchors: an anchored read does not load its dwords directly");
        }
    }
    RecompileRequest plain{};
    const std::vector<std::uint32_t> endOnly{0xbf810000u};
    plain.shader = {ShaderStage::Compute, 0x60000u, endOnly, 0, {}};
    plain.context.waveSize = 32;
    plain.context.userDataBaseRegister = 0;
    plain.context.userData = userData;
    plain.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    plain.target.vulkanVersion = 0x00401000u;
    plain.target.spirvVersion = 0x00010300u;
    plain.target.subgroupSize = 32;
    plain.target.bdaAbiVersion = BdaAbi::Version;
    plain.target.fragmentShaderBarycentricEnabled = false;
    plain.layout.pushConstantSizeBytes = 128;
    plain.useCache = false;
    const auto plainWords = Recompile(plain).spirv;
    for (std::size_t cursor = 5; cursor < plainWords.size();) {
        const auto length = plainWords[cursor] >> 16u;
        require(length != 0 && length <= plainWords.size() - cursor, "BDA anchors: truncated SPIR-V instruction");
        const auto op = plainWords[cursor] & 0xffffu;
        require(!(op == spv::OpTypeInt && plainWords[cursor + 2] == 64u), "BDA anchors: a program without anchors declares a 64-bit integer type");
        require(!(op == spv::OpCapability && plainWords[cursor + 1] == spv::CapabilityInt64), "BDA anchors: a program without anchors requires Int64");
        cursor += length;
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
    const std::vector<const IrValue*> order(block.Instructions().begin(), block.Instructions().end());
    const auto readAt = std::find(order.begin(), order.end(), &read);
    require(readAt != order.end() && readAt - order.begin() >= 2 && *(readAt - 1) == replacement && *(readAt - 2) == replacement->Argument(0) && (*(readAt - 2))->Opcode() == IrOpcode::GetSrtResource, "SRT read patching: the flat read is not right before the original read");
    require(order.back()->Opcode() == IrOpcode::ReferenceU32 && order.back()->Argument(0) == &read, "SRT read patching: the planning reference is not at the end of the block");
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

void verifyGuardedDirectRoot() {
    using namespace ShaderRecompiler;
    constexpr std::size_t Reserved = 0x10000;
    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, Reserved, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, Reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    require(reserved != nullptr, "guarded direct root: cannot reserve an inaccessible range");
    const auto unmapped = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(reserved));
    alignas(256) static std::array<std::uint32_t, 64> output{};
    alignas(256) static std::array<std::uint32_t, 16> root{};
    static std::uint32_t payload = 0x3f800000u;
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const auto rootAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root.data()));
    const auto payloadAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&payload));
    root = {static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(output)), 0x30027facu, 1u};
    const std::vector<std::uint32_t> branchCode{0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xbe8a0302u, 0xbe8b0303u, 0xbe8c03ffu, 0x11111111u,
        0xbf8cc07fu, 0xbf068008u, 0xbf850003u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu, 0x7e02020cu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u};
    for (const auto index : {4u, 5u}) {
        const auto move = DecodeRdnaInstruction(index * 4u, branchCode, index);
        require(move.op == RdnaOpcode::SMovB32 && move.destination.kind == RdnaOperandKind::ScalarRegister && move.destination.reg == index + 6u &&
            move.source0.kind == RdnaOperandKind::ScalarRegister && move.source0.reg == index - 2u, "guarded direct root: the fixture does not move the user-data pointer to s10:s11");
    }
    const auto compare = DecodeRdnaInstruction(0x24u, branchCode, 9u);
    require(compare.op == RdnaOpcode::SCmpEqU32 && compare.source0.kind == RdnaOperandKind::ScalarRegister && compare.source0.reg == 8u &&
        compare.source1.kind == RdnaOperandKind::IntegerInlineConstant && compare.source1.value == 0u, "guarded direct root: the fixture does not compare the branch word with zero");
    const auto condition = DecodeRdnaInstruction(0x28u, branchCode, 10u);
    require(condition.op == RdnaOpcode::SCbranchScc1 && condition.branchTarget == 0x38u, "guarded direct root: the fixture does not skip the scalar load when the branch word is zero");
    NestedRequest branch(branchCode, root.data());
    std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u),
        static_cast<std::uint32_t>(payloadAddress), static_cast<std::uint32_t>(payloadAddress >> 32u)};
    branch.request.context.userData = userData;
    AgcDriver::ShaderMemory validMemory({});
    const auto valid = validMemory.Capture(branch.request);
    require(valid->snapshot.srtPoison.empty(), "guarded direct root: a mapped direct pointer was poisoned");
    const auto read = std::find_if(valid->plan->srtReads.begin(), valid->plan->srtReads.end(), [](const auto& candidate) { return candidate.value->template Flags<MemoryFlags>().pc == 0x2cu; });
    require(read != valid->plan->srtReads.end(), "guarded direct root: the scalar read was not planned");
    require(!Detail::LoadedFromMemory(*read->value->Argument(0)), "guarded direct root: the pointer was classified as loaded from memory");
    require(valid->snapshot.flattenedSrt.at(read->flatOffset) == payload, "guarded direct root: the mapped scalar read has the wrong word");
    const auto validRegions = validMemory.Regions();
    branch.request.context.memory = validRegions;
    const auto validResult = Recompile(branch.request, *valid);
    require(validResult->poisonedSrtReads == 0u, "guarded direct root: the mapped program reports poison");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(validResult->spirv, branch.request.target.vulkanVersion, branch.request.target.spirvVersion));
#endif
    std::cout << "guarded direct root: mapped taken load passed\n";
    root[4] = 0u;
    branch.request.context.memory = {};
    AgcDriver::ShaderMemory mappedSkippedMemory({});
    const auto mappedSkipped = mappedSkippedMemory.Capture(branch.request);
    const auto mappedSkippedRegions = mappedSkippedMemory.Regions();
    branch.request.context.memory = mappedSkippedRegions;
    const auto mappedSkippedResult = Recompile(branch.request, *mappedSkipped);
    require(mappedSkippedResult->poisonedSrtReads == 0u, "guarded direct root: the mapped skipped program reports poison");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(mappedSkippedResult->spirv, branch.request.target.vulkanVersion, branch.request.target.spirvVersion));
#endif
    std::cout << "guarded direct root: mapped skipped load passed\n";
    root[4] = 1u;
    userData[2] = static_cast<std::uint32_t>(unmapped);
    userData[3] = static_cast<std::uint32_t>(unmapped >> 32u);
    branch.request.context.memory = {};
    expectFailure([&] {
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(branch.request));
    }, "guest memory is not readable", "guarded direct root: a taken inaccessible load was accepted");
    std::cout << "guarded direct root: inaccessible taken load rejected\n";
    root[4] = 0u;
    std::vector<std::byte> encodedPlan;
    require(ShaderDiskCache::EncodePlan(*valid->plan, encodedPlan), "guarded direct root: cannot encode the resource plan");
    IrResourcePlan decodedPlan;
    require(ShaderDiskCache::DecodePlan(encodedPlan, decodedPlan), "guarded direct root: cannot decode the resource plan");
    require(!valid->plan->controlFlow.empty() && decodedPlan.controlFlow.size() == valid->plan->controlFlow.size(), "guarded direct root: the scalar branch was not retained in the plan");
    for (std::size_t block = 0; block < decodedPlan.controlFlow.size(); ++block) {
        require(decodedPlan.controlFlow[block].flatSlots == valid->plan->controlFlow[block].flatSlots, "guarded direct root: cached control flow lost its flat slots");
    }
    IrResourcePlan decodedIrPlan;
    require(ShaderDiskCache::DecodePlan(encodedPlan, decodedIrPlan), "guarded direct root: cannot decode the IR control plan");
    decodedIrPlan.compact = {};
    struct ReadContext {
        std::uint64_t root;
        std::uint64_t payload;
        bool guardReadable;
        std::uint32_t invalidReads = 0;
        SrtMemoryReader reader = nullptr;
    };
    const auto walk = [&](const IrResourcePlan& plan, bool tape, bool guardReadable) {
        ReadContext context{rootAddress, payloadAddress, guardReadable};
        context.reader = [](void* opaque, std::uint64_t address, std::uint32_t* value) {
            auto& context = *static_cast<ReadContext*>(opaque);
            if (!((address >= context.root && address - context.root <= sizeof(root) - sizeof(*value)) || address == context.payload)) {
                ++context.invalidReads;
                return false;
            }
            std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
            return true;
        };
        SrtRuntime runtime{userData};
        runtime.userContext = &context;
        runtime.readMemory = context.reader;
        runtime.readSpecializationMemory = [](void* opaque, std::uint64_t address, std::uint32_t* value) {
            auto& context = *static_cast<ReadContext*>(opaque);
            if (!context.guardReadable && address == context.root + 16u) return false;
            return context.reader(opaque, address, value);
        };
        const std::vector<std::uint32_t> sourceCopy(plan.materializationSources.begin(), plan.materializationSources.end());
        const std::vector<std::uint8_t> cleanCopy(plan.cleanFlatSlots.begin(), plan.cleanFlatSlots.end());
        const auto sources = tape ? std::span<const std::uint32_t>(plan.materializationSources) : std::span<const std::uint32_t>(sourceCopy);
        const auto clean = tape ? std::span<const std::uint8_t>(plan.cleanFlatSlots) : std::span<const std::uint8_t>(cleanCopy);
        std::vector<DescriptorValue> descriptors;
        std::vector<std::uint32_t> flat;
        std::vector<std::uint8_t> active;
        std::vector<SrtReadPoison> poison;
        const auto* compiled = tape ? Detail::SrtTape::For(plan) : nullptr;
        if (tape) require(compiled != nullptr, "guarded direct root: the plan has no SRT tape");
        const bool ok = tape ? compiled->Run(plan, runtime, descriptors, flat, active) : Detail::EvaluateRuntimeSourcesImpl(plan, sources, runtime, descriptors, flat, true, clean, active, &poison, nullptr);
        require(ok == guardReadable, "guarded direct root: a known skipped load failed, or an unresolved guard silently succeeded");
        if (guardReadable) require(context.invalidReads == 0u && poison.empty(), "guarded direct root: the walker touched a known skipped load");
    };
    for (const IrResourcePlan* plan : {valid->plan.get(), static_cast<const IrResourcePlan*>(&decodedPlan), static_cast<const IrResourcePlan*>(&decodedIrPlan)}) {
        for (const bool tape : {false, true}) {
            walk(*plan, tape, true);
            walk(*plan, tape, false);
        }
    }
    auto repeatedCode = branchCode;
    repeatedCode.insert(repeatedCode.begin() + 14, {0xf4000305u, 0xfa000000u, 0xbf8cc07fu});
    NestedRequest repeated(std::move(repeatedCode), root.data());
    repeated.request.context.userData = userData;
    expectFailure([&] {
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(repeated.request));
    }, "guest memory is not readable", "guarded direct root: an unconditional repeated load was skipped with its guarded use");
    AgcDriver::ShaderMemory skippedMemory({});
    const auto skipped = skippedMemory.Capture(branch.request);
    const auto skippedRegions = skippedMemory.Regions();
    require(skipped->snapshot.srtPoison.empty() && !RegionsTouch(skippedRegions, unmapped, 4u), "guarded direct root: a skipped scalar load was captured or poisoned");
    branch.request.context.memory = skippedRegions;
    const auto skippedResult = Recompile(branch.request, *skipped);
    require(skippedResult->poisonedSrtReads == 0u, "guarded direct root: the skipped program reports poison");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(skippedResult->spirv, branch.request.target.vulkanVersion, branch.request.target.spirvVersion));
#endif
    std::cout << "guarded direct root: inaccessible skipped load passed\n";
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, Reserved);
#endif
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
    const auto guardSites = [](const RecompileResult& result) {
        const auto words = result.spirv.Words();
        std::map<std::uint32_t, std::string> names;
        std::map<std::uint32_t, std::uint32_t> constants;
        std::vector<std::uint32_t> guardWords;
        std::string function;
        std::size_t calls = 0;
        std::size_t stores = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            const auto opcode = words[cursor] & 0xffffu;
            require(count != 0 && count <= words.size() - cursor, "guarded pointer: truncated SPIR-V instruction");
            if (opcode == spv::OpName) names[words[cursor + 1]] = reinterpret_cast<const char*>(&words[cursor + 2]);
            if (opcode == spv::OpConstant && count == 4u) constants[words[cursor + 2]] = words[cursor + 3];
            if (opcode == spv::OpFunction) function = names[words[cursor + 2]];
            if (opcode == spv::OpAccessChain && count == 6u && names[words[cursor + 3]] == "fault_buffer" && constants.contains(words[cursor + 5]) && constants[words[cursor + 5]] == BdaAbi::SrtGuardWord) guardWords.push_back(words[cursor + 2]);
            if (function == "main" && opcode == spv::OpFunctionCall && names[words[cursor + 3]] == "record_bda_fault") ++calls;
            if (function == "main" && opcode == spv::OpAtomicStore && std::find(guardWords.begin(), guardWords.end(), words[cursor + 1]) != guardWords.end()) ++stores;
            cursor += count;
        }
        return std::pair{calls, stores};
    };

    NestedRequest branch(branchCode, root.data());
    point(payloadAddress);
    AgcDriver::ShaderMemory validMemory({});
    const auto valid = validMemory.Capture(branch.request);
    require(valid->snapshot.srtPoison.empty() && valid->plan->guardedSrtSlots.size() == 1u && slotPc(*valid->plan, valid->plan->guardedSrtSlots[0]) == 0x2cu && GuardRecords(*valid), "guarded pointer: a mapped nested pointer was poisoned, or its read has no guard");
    require(valid->snapshot.flattenedSrt.at(valid->plan->guardedSrtSlots[0]) == payload, "guarded pointer: a mapped nested read does not hold its word");
    const auto validRegions = validMemory.Regions();
    branch.request.context.memory = validRegions;
    const auto validResult = Recompile(branch.request, *valid);
    require(validResult->poisonedSrtReads == 0u && faultBinding(*validResult) && validResult->bdaAbiVersion == BdaAbi::Version, "guarded pointer: a guarded program has no fault buffer");
    const auto [branchCalls, branchStores] = guardSites(*validResult);
    require(branchCalls == 0u && branchStores == 1u, ("guarded pointer: a guarded read calls record_bda_fault " + std::to_string(branchCalls) + " times in main and stores its record to the fault buffer's guard word " + std::to_string(branchStores) + " times, expected one store and no call").c_str());

    for (const auto pointer : {std::uint64_t{0}, unmapped}) {
        for (const auto condition : {1u, 0u}) {
            point(pointer);
            root[4] = condition;
            branch.request.context.memory = {};
            AgcDriver::ShaderMemory memory({});
            const auto capture = memory.Capture(branch.request);
            const auto& poison = capture->snapshot.srtPoison;
            require(condition == 0u ? poison.empty() : poison.size() == 1u && poison[0].pc == 0x2cu && poison[0].address == pointer && slotPc(*capture->plan, poison[0].slot) == 0x2cu, "guarded pointer: a skipped nested read was poisoned, or an executed read has the wrong poison record");
            require(GuardRecords(*capture), "guarded pointer: the poison record is not where the guard reads it");
            const auto regions = memory.Regions();
            require(!RegionsTouch(regions, pointer, 4u) && RegionsCover(regions, root.data() + 6, 8u), "guarded pointer: the captured regions are wrong");
            branch.request.context.memory = regions;
            const auto result = Recompile(branch.request, *capture);
            require(result->poisonedSrtReads == condition && faultBinding(*result) && result->bdaAbiVersion == BdaAbi::Version, "guarded pointer: the capture has the wrong poison count or no fault buffer");
            require(samePipeline(*result, *validResult), "guarded pointer: poison changed the variant, the specialization or the module");
            const auto replay = Recompile(branch.request);
            require(replay.poisonedSrtReads == condition && samePipeline(replay, *result), "guarded pointer: a replay of the captured regions did not reproduce the poison");
            verifyResult(*result, replay);
#if ANYPS5_ENABLE_SPIRV_TOOLS
            static_cast<void>(ValidateAndOptimizeSpirv(result->spirv, branch.request.target.vulkanVersion, branch.request.target.spirvVersion));
#endif
        }
    }
    root[4] = 1u;

    auto belowCode = branchCode;
    belowCode[12] = 0xfa1ffffcu;
    belowCode.push_back(0xbf810000u);
    NestedRequest below(belowCode, root.data());
    point(payloadAddress + 4u);
    AgcDriver::ShaderMemory belowMemory({});
    const auto belowValid = belowMemory.Capture(below.request);
    require(belowValid->snapshot.srtPoison.empty() && belowValid->snapshot.flattenedSrt.at(belowValid->plan->guardedSrtSlots.at(0)) == payload, "guarded pointer: a read below a mapped nested pointer does not hold its word");
    for (const auto condition : {1u, 0u}) {
        point(0u);
        root[4] = condition;
        below.request.context.memory = {};
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(below.request);
        const auto& poison = capture->snapshot.srtPoison;
        require(poison.size() == condition, "guarded pointer: a read below a null nested pointer was poisoned on a path that skips it, or not poisoned on one that takes it");
        if (condition != 0u) {
            require(poison[0].pc == 0x2cu && poison[0].address == 0xfffffffffffcu && slotPc(*capture->plan, poison[0].slot) == 0x2cu, "guarded pointer: a read below a null nested pointer was not poisoned at its pc and wrapped address");
            require(GuardRecords(*capture), "guarded pointer: the poison record of a read below a null pointer is not where the guard reads it");
        }
        const auto regions = memory.Regions();
        below.request.context.memory = regions;
        const auto result = Recompile(below.request, *capture);
        require(result->poisonedSrtReads == condition && faultBinding(*result), "guarded pointer: a read below a null pointer does not report through the fault buffer");
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
    const auto chainRegions = chainMemory.Regions();
    chain.request.context.memory = chainRegions;
    const auto chainResult = Recompile(chain.request, *chained);
    indirect = 0;
    point(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&indirect)));
    AgcDriver::ShaderMemory nullChainMemory({});
    const auto nullChain = nullChainMemory.Capture(chain.request);
    require(nullChain->snapshot.srtPoison.size() == 1u && nullChain->snapshot.srtPoison[0].pc == 0x38u && nullChain->snapshot.srtPoison[0].address == 0u && GuardRecords(*nullChain), "guarded pointer: a null pointer read from memory did not poison the read through it");
    const auto nullChainRegions = nullChainMemory.Regions();
    chain.request.context.memory = nullChainRegions;
    const auto nullChainResult = Recompile(chain.request, *nullChain);
    indirect = payloadAddress;
    AgcDriver::ShaderMemory mappedChainMemory({});
    const auto mappedChain = mappedChainMemory.Capture(chain.request);
    require(mappedChain->snapshot.srtPoison.empty() && mappedChain->plan->guardedSrtSlots.size() == 3u && GuardRecords(*mappedChain), "guarded pointer: a mapped pointer chain was poisoned, or its reads have no guards");
    const auto mappedChainRegions = mappedChainMemory.Regions();
    chain.request.context.memory = mappedChainRegions;
    const auto mappedChainResult = Recompile(chain.request, *mappedChain);
    require(chainResult->poisonedSrtReads == 3u && nullChainResult->poisonedSrtReads == 1u && mappedChainResult->poisonedSrtReads == 0u, "guarded pointer: a chain result does not count its poisoned reads");
    const auto [chainCalls, chainStores] = guardSites(*mappedChainResult);
    require(chainCalls == 0u && chainStores == mappedChain->plan->guardedSrtSlots.size(), ("guarded pointer: a chain's guarded reads call record_bda_fault " + std::to_string(chainCalls) + " times in main and store to the guard word " + std::to_string(chainStores) + " times, expected one store per guard and no call").c_str());
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
    const auto zeroRegions = zeroMemory.Regions();
    descriptor.request.context.memory = zeroRegions;
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
        require(guard == 0u ? capture->snapshot.srtPoison.empty() && GuardRecords(*capture) : poisonedWords(*capture, 0x20u, 0x10u), "guarded pointer: a skipped vertex V# was poisoned, or an executed read has the wrong poison record");
        const auto regions = memory.Regions();
        vertex.request.context.memory = regions;
        const auto result = Recompile(vertex.request, *capture);
        require(result->poisonedSrtReads == guard * 4u && faultBinding(*result) && !result->spirv.empty(), "guarded pointer: a vertex capture has the wrong poison count or no fault buffer");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        static_cast<void>(ValidateAndOptimizeSpirv(result->spirv, vertex.request.target.vulkanVersion, vertex.request.target.spirvVersion));
#endif
    }
    NestedRequest rootless(branchCode, nullptr);
    AgcDriver::ShaderMemory rootlessMemory({});
    const auto rootlessCapture = rootlessMemory.Capture(rootless.request);
    require(rootlessCapture->snapshot.nullRootReads != 0u && GuardRecords(*rootlessCapture), "guarded pointer: reads through a null user-data pointer were not counted");
    require(std::all_of(rootlessCapture->snapshot.buffers.begin(), rootlessCapture->snapshot.buffers.end(), [](const DescriptorValue& value) { return std::all_of(value.dwords.begin(), value.dwords.end(), [](std::uint32_t word) { return word == 0u; }); }), "guarded pointer: a V# read through a null user-data pointer is not zero");
    const auto rootlessRegions = rootlessMemory.Regions();
    rootless.request.context.memory = rootlessRegions;
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

void verifyFmaskImageMode() {
    using namespace ShaderRecompiler;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2D;
    DescriptorValue descriptor{{0x100u, 166u << 20u, 0u, 0x91800004u, 0u, 0u, 0u, 0u}, 8u};
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const auto selected = ResourceMaterializer::RuntimeImageMode(image, descriptor, modes);
    require(modes[selected].numericClass == IrTextureNumericClass::Uint && modes[selected].packedFormat == IrBufferFormat::Invalid && !modes[selected].constantSwizzle, "FMASK did not select a bound integer image mode");
    for (const auto format : {156u, 157u, 158u, 159u, 160u, 161u, 163u, 165u}) {
        descriptor.dwords[1] = format << 20u;
        const auto expanded = ResourceMaterializer::RuntimeImageMode(image, descriptor, modes);
        require(expanded != selected && modes[expanded].numericClass == IrTextureNumericClass::Uint && modes[expanded].conversionFormat == FmaskExpansion && modes[expanded].dimension == RdnaImageDimension::Dim2D, "an FMASK format of up to 8 samples did not select the FMASK expansion mode");
    }
    require(std::ranges::count_if(modes, [](const ImageResource& mode) { return mode.conversionFormat == FmaskExpansion; }) == 1, "FMASK expansion needs exactly one runtime image mode");
    for (const auto format : {161u, 166u}) {
        descriptor.dwords[1] = format << 20u;
        descriptor.dwords[3] = 0x91800005u;
        expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)); }, "FMASK destination channel selector is not implemented", "an FMASK load selecting Y was accepted");
    }
    descriptor.dwords[3] = 0x91800004u;
    for (const auto format : {162u, 164u, 167u, 168u}) {
        descriptor.dwords[1] = format << 20u;
        expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)); }, "FMASK format is not implemented", "an unsupported FMASK format was accepted");
    }
    descriptor.dwords[1] = 166u << 20u;
    for (const auto compression : {1u << 20u, 1u << 21u}) {
        descriptor.dwords[6] = compression;
        expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)); }, "FMASK CMASK compression is not implemented", "compressed FMASK was accepted without decoding its metadata");
    }
    descriptor.dwords[6] = 0u;
    image.packed = true;
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)); }, "packed FMASK image reads are not implemented", "packed FMASK was accepted");
    image.packed = false;
    image.fmaskCompatible = false;
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)); }, "FMASK requires a direct 32-bit image load", "an incompatible FMASK operation was accepted");
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

void verifyShortCubeMode() {
    using namespace ShaderRecompiler;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2DArray;
    DescriptorValue descriptor{{0x02143930u, 71u << 20u, 0u, 0xb1b00facu, 4u, 0u, 0u, 0u}, 8u};
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    require(modes[ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)].cube, "a sampled cube with five slices did not select a cube runtime mode");
    image.sampledOnly = false;
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)); }, "fewer than six slices is implemented only for sampling", "a loaded cube with five slices was accepted");
    for (const auto range : {0u, 5u, 5u | (5u << 16u), 6u}) {
        descriptor.dwords[4] = range;
        require(modes[ResourceMaterializer::RuntimeImageMode(image, descriptor, modes)].cube, "a loaded cube with six faces or more was rejected");
    }
    image.sampledOnly = true;
    image.resourceClass = ImageResourceClass::Storage;
    image.written = true;
    descriptor.dwords[4] = 3u | (1u << 16u);
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, descriptor, ResourceMaterializer::RuntimeImageModes(image))); }, "fewer than six slices is implemented only for sampling", "a stored cube with three slices was accepted");
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
    image.volumeArraySampleCompatible = true;
    const auto sampleModes = ResourceMaterializer::RuntimeImageModes(image);
    const auto& sampled = sampleModes[ResourceMaterializer::RuntimeImageMode(image, descriptor, sampleModes)];
    require(sampled.dimension == RdnaImageDimension::Dim3D && !sampled.cube, "2D-array image sample on a 3D T# did not select a volume runtime mode");
    const DescriptorValue arrayDescriptor{{0x02466600u, 0x01400000u, 0u, 0xd0500204u, 1u, 0x00700000u, 0u, 0u}, 8u};
    require(sampleModes[ResourceMaterializer::RuntimeImageMode(image, arrayDescriptor, sampleModes)].dimension == RdnaImageDimension::Dim2DArray, "2D-array image sample on a 2D-array T# no longer selects the 2D-array runtime mode");
    image.volumeArraySampleCompatible = false;
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

void verifyMultisamplePlaneReadMode() {
    using namespace ShaderRecompiler;
    const DescriptorValue descriptor{{0x02754d00u, 0xc3800000u, 0x00cbc167u, 0xe1b20facu, 0u, 0x00700020u, 0u, 0u}, 8u};
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2D;
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const auto selected = ResourceMaterializer::RuntimeImageMode(image, descriptor, modes);
    require(modes[selected].dimension == RdnaImageDimension::Dim2D && !modes[selected].cube && modes[selected].numericClass == IrTextureNumericClass::Float, "a 2D read of a 2D MSAA T# (PPSA18216's) did not select a 2D runtime mode");
    for (const auto& [word1, word3, word5, numericClass] : std::array<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, IrTextureNumericClass>, 4>{{{0xc0100000u, 0xe1b20facu, 0x00700020u, IrTextureNumericClass::Float}, {0xc0500000u, 0xe1810facu, 0x00700010u, IrTextureNumericClass::Uint}, {0xc0c00000u, 0xe1b20facu, 0x00700020u, IrTextureNumericClass::Sint}, {0xc0d00000u, 0xe1830facu, 0x00700030u, IrTextureNumericClass::Float}}}) {
        const DescriptorValue narrow{{0x02754d00u, word1, 0x00cbc167u, word3, 0u, word5, 0u, 0u}, 8u};
        const auto mode = ResourceMaterializer::RuntimeImageMode(image, narrow, modes);
        require(modes[mode].dimension == RdnaImageDimension::Dim2D && !modes[mode].cube && modes[mode].numericClass == numericClass, "a 2D read of a 2D MSAA T# with 8-bit or 16-bit texels did not select a 2D runtime mode");
    }
    const DescriptorValue depthBits{{0x02754d00u, 0xc0b00000u, 0x00cbc167u, 0xe1820facu, 0u, 0x00700020u, 0u, 0u}, 8u};
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, depthBits, modes)); }, "incompatible with the static runtime image interface", "a 2D read of a 16_UINT SW_64KB_Z_X 2D MSAA T#, read as depth bits, was accepted");
    const auto refused = [&](const ImageResource& access, const char* what) {
        expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(access, descriptor, ResourceMaterializer::RuntimeImageModes(access))); }, "incompatible with the static runtime image interface", what);
    };
    auto other = image;
    other.cubeFaceCompatible = false;
    refused(other, "a 2D read of a 2D MSAA T# through a mip, gather, bias, gradient or offset access was accepted");
    other = image;
    other.depthCompare = true;
    refused(other, "a 2D depth comparison of a 2D MSAA T# was accepted");
    other = image;
    other.resourceClass = ImageResourceClass::Storage;
    other.written = true;
    refused(other, "a 2D storage access to a 2D MSAA T# was accepted");
}

void verifyVolumeBiasTableRejection() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2D;
    image.singleMipPlaneBiasCompatible = true;
    const DescriptorValue volume{{0x100u, TableFormat8888UNorm << 20u, 0u, (TableType3D << 28u) | 0xfacu, 0u, 0u, 0u, 0u}, 8u, 0xffu};
    const auto directModes = ResourceMaterializer::RuntimeImageModes(image);
    require(directModes[ResourceMaterializer::RuntimeImageMode(image, volume, directModes)].dimension == RdnaImageDimension::Dim3D, "volume bias: a supported direct one-slice volume was rejected");
    image.table = 0u;
    image.tableOperation = TableOperation::Supported;
    ShaderInfo info{};
    info.images.push_back(image);
    info.runtimeImageModes.push_back(ResourceMaterializer::RuntimeImageModes(image));
    ResourceSnapshot snapshot{};
    snapshot.tables.tables.resize(1u);
    auto& column = snapshot.tables.tables.front();
    column.records = 2u;
    column.keys = 2u;
    column.codes = SharedValues<std::uint32_t>(std::vector<std::uint32_t>{0u, 1u});
    auto plane = volume;
    plane.dwords[3] = (TableType2D << 28u) | 0xfacu;
    for (const auto [word, bits] : {std::pair{4u, 1u}, std::pair{3u, 1u << 12u}, std::pair{3u, 1u << 16u}, std::pair{5u, 1u << 4u}}) {
        auto invalid = volume;
        invalid.dwords[word] |= bits;
        column.words = SharedValues<DescriptorValue>(std::vector<DescriptorValue>{plane, invalid});
        const auto resolved = ResourceMaterializer::ResolveImageTables(info, snapshot);
        const auto mapStart = resolved.map.at(Abi::TableHeader(0u) + Abi::TableMapStart);
        const auto good = resolved.map.at(mapStart);
        const auto bad = resolved.map.at(mapStart + 1u);
        require(resolved.elements.size() == 1u && (resolved.map.at(good + Abi::ImageRecordFlags) & Abi::PoisonFlag) == 0u, "volume bias: an unsupported table neighbour prevented the valid plane binding");
        const auto fault = resolved.map.at(bad + Abi::ImageRecordFlags);
        require((fault & Abi::PoisonFlag) != 0u && resolved.faults == 1u, "volume bias: an unsupported volume was not marked to fault on selection");
        const auto& poison = resolved.poison.at(fault & ~Abi::PoisonFlag);
        require(poison.reason == Abi::PoisonReason::Dimension && poison.words == invalid.dwords, "volume bias: the table fault lost its reason or descriptor");
        image.table = NoTable;
        const auto match = ResourceMaterializer::TryRuntimeImageMode(image, invalid, directModes);
        require(!match.mode.has_value() && match.reason == Abi::PoisonReason::Dimension, "volume bias: an unsupported direct descriptor selected a mode");
        expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(image, invalid, directModes)); }, "one slice and mip level zero only", "volume bias: direct use of an unsupported descriptor was accepted");
    }
}

void verifySamplerTableDeduplication() {
    using namespace ShaderRecompiler;
    namespace Abi = ImageTableAbi;
    SamplerResource sampler0{};
    sampler0.table = 0u;
    SamplerResource sampler1{};
    sampler1.table = 1u;
    ShaderInfo info{};
    info.samplers.push_back(sampler0);
    info.samplers.push_back(sampler1);
    ResourceSnapshot snapshot{};
    snapshot.tables.tables.resize(2u);
    const DescriptorValue s0{{0x00000600u, 0x00fff000u, 0x06f03fa0u, 0u}, 4u};
    const DescriptorValue s1{{0x00000700u, 0x00fff000u, 0x06f03fa0u, 0u}, 4u};
    const DescriptorValue s2{{0x00000800u, 0x00fff000u, 0x06f03fa0u, 0u}, 4u};
    std::vector<std::uint32_t> codes(300u);
    for (std::size_t i = 0; i < 300u; ++i) codes[i] = static_cast<std::uint32_t>(i % 3u);
    for (std::uint32_t table = 0u; table < 2u; ++table) {
        auto& column = snapshot.tables.tables[table];
        column.records = 300u;
        column.keys = 300u;
        column.codes = SharedValues<std::uint32_t>(codes);
        column.words = SharedValues<DescriptorValue>(std::vector<DescriptorValue>{s0, s1, s2});
    }
    const auto resolved = ResourceMaterializer::ResolveImageTables(info, snapshot);
    require(resolved.samplers.size() == (3u + 1u) * Abi::SamplerEntryElements, "sampler table deduplication: duplicate sampler entries across tables were not deduplicated");
    for (std::uint32_t table = 0u; table < 2u; ++table) {
        const auto mapStart = resolved.map.at(Abi::TableHeader(table) + Abi::TableMapStart);
        for (std::uint32_t key = 0; key < 300u; ++key) {
            const auto record = resolved.map.at(mapStart + key);
            const auto element = resolved.map.at(record + Abi::SamplerRecordElement);
            require(element == (key % 3u) * Abi::SamplerEntryElements, "sampler table deduplication: record points to the wrong deduplicated sampler element");
            require((resolved.map.at(record + Abi::SamplerRecordFlags) & Abi::PoisonFlag) == 0u, "sampler table deduplication: deduplicated sampler was poisoned");
        }
    }

    constexpr std::uint32_t MaxUnique = Abi::SamplerElementBudget / Abi::SamplerEntryElements;
    std::vector<DescriptorValue> budgetWords;
    budgetWords.reserve(MaxUnique + 1u);
    for (std::uint32_t i = 0; i < MaxUnique + 1u; ++i) {
        budgetWords.push_back(DescriptorValue{{0x00000600u, 0x00fff000u, 0x06f03fa0u, i + 1u}, 4u});
    }
    std::vector<std::uint32_t> budgetCodes(MaxUnique + 2u);
    for (std::uint32_t i = 0; i < MaxUnique; ++i) budgetCodes[i] = i;
    budgetCodes[MaxUnique] = 0u;
    budgetCodes[MaxUnique + 1u] = MaxUnique;

    ShaderInfo budgetInfo{};
    SamplerResource budgetSampler{};
    budgetSampler.table = 0u;
    budgetInfo.samplers.push_back(budgetSampler);
    ResourceSnapshot budgetSnapshot{};
    budgetSnapshot.tables.tables.resize(1u);
    auto& budgetColumn = budgetSnapshot.tables.tables[0];
    budgetColumn.records = static_cast<std::uint32_t>(budgetCodes.size());
    budgetColumn.keys = static_cast<std::uint32_t>(budgetCodes.size());
    budgetColumn.codes = SharedValues<std::uint32_t>(budgetCodes);
    budgetColumn.words = SharedValues<DescriptorValue>(budgetWords);

    const auto budgetResolved = ResourceMaterializer::ResolveImageTables(budgetInfo, budgetSnapshot);
    require(budgetResolved.samplers.size() == Abi::SamplerElementBudget, "sampler table budget: valid unique samplers did not fill budget");
    const auto budgetMapStart = budgetResolved.map.at(Abi::TableHeader(0) + Abi::TableMapStart);

    const auto dupRecord = budgetResolved.map.at(budgetMapStart + MaxUnique);
    const auto dupFlags = budgetResolved.map.at(dupRecord + Abi::SamplerRecordFlags);
    require((dupFlags & Abi::PoisonFlag) == 0u, "sampler table budget: duplicate of existing sampler was poisoned when budget is full");
    require(budgetResolved.map.at(dupRecord + Abi::SamplerRecordElement) == 0u, "sampler table budget: duplicate sampler did not reuse existing element");

    const auto overflowRecord = budgetResolved.map.at(budgetMapStart + MaxUnique + 1u);
    const auto overflowFlags = budgetResolved.map.at(overflowRecord + Abi::SamplerRecordFlags);
    require((overflowFlags & Abi::PoisonFlag) != 0u, "sampler table budget: exceeding sampler budget did not poison record");
    require(budgetResolved.poison.at(overflowFlags & ~Abi::PoisonFlag).reason == Abi::PoisonReason::Budget, "sampler table budget: poisoned reason is not Budget");

    const auto outsideRecord = budgetResolved.map.at(Abi::TableHeader(0) + Abi::TableOutsideCode);
    const auto outsideFlags = budgetResolved.map.at(outsideRecord + Abi::SamplerRecordFlags);
    require((outsideFlags & Abi::PoisonFlag) != 0u, "sampler table budget: TableOutsideCode did not poison when budget is full");
    require(budgetResolved.poison.at(outsideFlags & ~Abi::PoisonFlag).reason == Abi::PoisonReason::Budget, "sampler table budget: TableOutsideCode poisoned reason is not Budget");
}

void verifyDynamicStorageMipChain() {
    using namespace ShaderRecompiler;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Storage;
    image.numericClass = IrTextureNumericClass::Float;
    image.dimension = RdnaImageDimension::Dim2D;
    image.mipMode = ImageMipMode::DynamicStorage;
    image.written = true;
    image.fmaskCompatible = false;
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const DescriptorValue tenLevels{{0x12366e00u, 0xc2400000u, 0x007fc07fu, 0xd1b903acu, 0x17u, 0x00700090u, 0u, 0u}, 8u};
    const auto selected = ResourceMaterializer::TryRuntimeImageMode(image, tenLevels, modes);
    require(selected.mode.has_value(), "a dynamic-mip storage image of a 512x512 surface with ten levels was rejected");
    require(modes[*selected.mode].mipCount >= 10u, "a dynamic-mip storage mode does not hold ten levels");
    auto allLevels = tenLevels;
    allLevels.dwords[3] |= 0xf0000u;
    require(ResourceMaterializer::TryRuntimeImageMode(image, allLevels, modes).mode.has_value(), "a dynamic-mip storage image with sixteen levels was rejected");
}

void verifyArrayVolumeStoreMode() {
    using namespace ShaderRecompiler;
    const DescriptorValue descriptor{{0x11800100u, 0xc3800000u, 0x0003c003u, 0xd0900facu, 0u, 0x00700000u, 0u, 0u}, 8u};
    ImageResource store{};
    store.resourceClass = ImageResourceClass::Storage;
    store.dimension = RdnaImageDimension::Dim3D;
    store.written = true;
    store.fmaskCompatible = false;
    const auto modes = ResourceMaterializer::RuntimeImageModes(store);
    require(modes[ResourceMaterializer::RuntimeImageMode(store, descriptor, modes)].dimension == RdnaImageDimension::Dim2DArray, "3D image store on a 2D-array T# did not select an array runtime mode");
    store.read = true;
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(store, descriptor, ResourceMaterializer::RuntimeImageModes(store))); }, "incompatible with the static runtime image interface", "3D image load and store on a 2D-array T# was accepted");
    store.read = false;
    store.atomic = true;
    const DescriptorValue uintDescriptor{{0x11800100u, 0xc1400000u, 0x0003c003u, 0xd0900facu, 0u, 0x00700000u, 0u, 0u}, 8u};
    expectFailure([&] { static_cast<void>(ResourceMaterializer::RuntimeImageMode(store, uintDescriptor, ResourceMaterializer::RuntimeImageModes(store))); }, "incompatible with the static runtime image interface", "3D image atomic on a 2D-array T# was accepted");
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
#ifdef _WIN32
        _putenv_s("ANYPS5_NO_SHADER_CACHE", "1");
#else
        setenv("ANYPS5_NO_SHADER_CACHE", "1", 1);
#endif
        using namespace ShaderRecompiler;
        if (argc == 2 && std::string_view(argv[1]) == "--specialized-bda-abi") {
            verifySpecializedBufferAtomicBindings(true);
            std::cout << "Specialized programs retain the BDA ABI only while a fault buffer remains\n";
            return 0;
        }
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
        if (argc == 2 && std::string_view(argv[1]) == "--result-memo-budget") {
            verifyResultMemoBudget();
            std::cout << "The result memo keeps its most recently used results within its byte budget\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--result-memo-budget-masked") {
            verifyResultMemoBudgetMasked();
            std::cout << "The result memo budget counts and trims the masked table's entries\n";
            return 0;
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--region-merge" || std::string_view(argv[1]) == "--region-merge-bench")) {
            verifyRegionMerge(false, std::string_view(argv[1]) == "--region-merge-bench");
            verifyRegionMerge(true);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--result-memo-budget-threads") {
            verifyResultMemoBudgetThreads();
            std::cout << "The result memo ends within its byte budget when results are inserted during a trim\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--layout-pair") {
            verifyLayoutPair();
            std::cout << "Both push-constant layouts of a pixel shader compile from one program unchanged\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--entry-lane-proof-budget") {
            verifyEntryLaneProofBudget();
            std::cout << "The wave64 pixel entry-lane proof handles loop-carried saved EXEC masks on a 32-lane host\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--bindless") {
            verifyImageTableKeys();
            verifyImageTables();
            verifyImageTableBulkReads();
            verifyImageTableScratchReuse();
            verifyImageTableBulkKeyScan();
            verifyImageTableSrtKey();
            verifyImageTableMaskedOffset();
            verifyImageTableFirstLaneScale();
            verifyImageTableKeyUnion();
            verifyImageTableLateKey();
            verifyImageTableUnkeyedAccess();
            verifyImageTableRegistration();
            verifyScalarAddressRule();
            verifyPointerImageTable();
            verifyPointerTableOffsetCarry();
            verifySelectedRecordFallbacks();
            verifySelectedRecordOffsetCarry();
            verifySelectedRecordDirectOperations();
            verifyEvaluableHeapSamplerDirect();
            verifySelectedRecordUniformIndex();
            verifyChainedTableOffset();
            verifyPointerChainImageTable();
            verifyWaterfallImageTable();
            verifySplitWaterfallImageTable();
            std::cout << "Image table keys, maps, records and registration passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--table-snapshot-reuse") {
            verifyTableSnapshotReuse();
            verifyTableStampRace();
            verifyTableVouch();
            verifyTableSpanQuery();
            verifyTableEdgeStore();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--selected-records-image-limit") {
            verifySelectedRecordsWithinLimits(false, false);
            std::cout << "Selected image-table records past the image limit merge back into one table per heap\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--selected-records-pair-limit") {
            verifySelectedRecordsWithinLimits(true, false);
            std::cout << "Selected image-table records past the sampled pair limit merge back into one table per heap\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--selected-records-direct-image-limit") {
            verifySelectedRecordsWithinLimits(false, true);
            std::cout << "Direct images count toward the image limit beside selected image-table records\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--selected-records-direct-pair-limit") {
            verifySelectedRecordsWithinLimits(true, true);
            std::cout << "Direct samplers count toward the sampled pair limit beside selected image-table records\n";
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
        verifyPlanConditionPruning();
        verifyPlanRetention();
        verifyPlanRetentionChains();
        verifyPlanCopyAnswers();
        verifyGuardedDescriptorSources();
        verifyLaneMaskGuardedDescriptorSources();
        verifyExecGuardedDescriptorSources();
        verifySrtTape();
        verifyTargetFeatureKey();
        verifyImageTableKeys();
        verifyImageTables();
        verifySpecializedModuleRelease();
        verifyImageTableRegistration();
        verifyScalarAddressRule();
        verifyPointerImageTable();
        verifyPointerTableOffsetCarry();
        verifyChainedTableOffset();
        verifyPointerChainImageTable();
        verifyWaterfallImageTable();
        verifySplitWaterfallImageTable();
        verifyDescriptorPhis();
        verifyUniformTableImage();
        verifyUndefinedImageFormats();
        verifyLaneSelectedDescriptors();
        verifyProgramCounterRelativeData();
        verifyLanesOutsideHostSubgroup();
        verifyHalfWaveReduction();
        verifyFusedExactArithmetic();
        verifyNarrowVertexWave();
        verifySingleLaneVertexWave();
        verifySingleLaneElectedAtomicMax();
        verifyMeshConfiguration();
        verifyPixelInputs();
        verifyCanonicalVersion20And21Requests();
        verifyPixelRequestSerialization();
        verifyVertexInfoVersion19();
        verifyDepthRemapRequests();
        verifyMergedHullConfiguration();
        verifyDepthExportClamp();
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
        verifyNullVolumeUnnormalizedSampler();
        verifyWaveUniformValues();
        verifyTwoLaneUniformValues();
        verifyBdaReadFallbackFunctions();
        verifyBdaAnchors();
        verifyFunctionLdsBound();
        verifyFunctionLdsUpdates();
        verifySrtReadPatching();
        verifyGuardedDirectRoot();
        verifyGuardedNullPointers();
        verifyFmaskImageMode();
        verifyCubeDepthCompareMode();
        verifyShortCubeMode();
        verifyVolumeArrayLoadMode();
        verifyArrayVolumeStoreMode();
        verifyMultisamplePlaneReadMode();
        verifyVolumeBiasTableRejection();
        verifySamplerTableDeduplication();
        verifyDynamicStorageMipChain();
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
        auto float32Flush = uncached;
        float32Flush.target.float32FlushToZero = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(float32Flush)).request.target.float32FlushToZero, "f32 hardware flush was lost in serialization");
        std::vector<std::uint64_t> float32FlushKey;
        RecompileCacheKey::Build(float32Flush, float32FlushKey);
        require(plainKey != float32FlushKey, "f32 hardware flush did not change the cache key");
        auto float32Preserve = uncached;
        float32Preserve.target.float32DenormPreserve = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(float32Preserve)).request.target.float32DenormPreserve, "f32 denormal preservation was lost in serialization");
        require(!RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(uncached)).request.target.float32DenormPreserve, "f32 denormal preservation appeared from nowhere in serialization");
        std::vector<std::uint64_t> float32PreserveKey;
        RecompileCacheKey::Build(float32Preserve, float32PreserveKey);
        require(plainKey != float32PreserveKey && float32FlushKey != float32PreserveKey, "f32 denormal preservation did not change the cache key");
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
        const auto readOnly = [&](const char* name) {
            std::optional<std::uint32_t> id;
            bool decorated = false;
            for (std::size_t at = 5; at < first.spirv.size() && (first.spirv[at] >> 16u) != 0u; at += first.spirv[at] >> 16u) {
                const auto opcode = first.spirv[at] & 0xffffu;
                if (opcode == spv::OpName && std::strcmp(reinterpret_cast<const char*>(&first.spirv[at + 2u]), name) == 0) id = first.spirv[at + 1u];
            }
            for (std::size_t at = 5; id && at < first.spirv.size() && (first.spirv[at] >> 16u) != 0u; at += first.spirv[at] >> 16u) {
                decorated = decorated || ((first.spirv[at] & 0xffffu) == spv::OpDecorate && first.spirv[at + 1u] == *id && first.spirv[at + 2u] == spv::DecorationNonWritable);
            }
            return std::pair{id.has_value(), decorated};
        };
        require(readOnly("flattened_srt") == std::pair{true, true}, "the flattened SRT is not decorated NonWritable");
        for (const auto* table : {"bda_pagetable", "table_map"}) require(!readOnly(table).first || readOnly(table).second, "a read-only BDA page table or image table map is not decorated NonWritable");
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
            const auto changedRegions = changedMemory.Regions();
            changed.context.memory = changedRegions;
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
        const auto nullRootRegions = nullRoot.Regions();
        request.context.memory = nullRootRegions;
        require(Recompile(request, *nullRootCapture)->poisonedSrtReads >= nullRootCapture->snapshot.nullRootReads, "a capture through a null user-data pointer is cacheable");
        std::cout << "Shader memory capture, strict validation and deterministic replay passed\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
