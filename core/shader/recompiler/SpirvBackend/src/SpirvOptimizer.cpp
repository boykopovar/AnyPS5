#include "SpirvBackend/SpirvOptimizer.hpp"
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <atomic>
#include <cstdlib>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t ScalarReplacementLimit = 64;
constexpr std::uintptr_t InlineOptimizerStackBytes = 256 * 1024;

std::atomic<std::uint64_t> optimizerHostThreads{0};

bool StackHoldsOptimizer() {
#ifdef _WIN32
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
#else
    struct Bounds {
        std::uintptr_t low = 0;
        std::uintptr_t high = 0;
        Bounds() {
            pthread_attr_t attributes;
            if (pthread_getattr_np(pthread_self(), &attributes) != 0) return;
            void* base = nullptr;
            std::size_t size = 0;
            if (pthread_attr_getstack(&attributes, &base, &size) == 0 && base != nullptr) {
                low = reinterpret_cast<std::uintptr_t>(base);
                high = low + size;
            }
            pthread_attr_destroy(&attributes);
        }
    };
    thread_local const Bounds bounds;
    const auto low = bounds.low;
    const auto high = bounds.high;
#endif
    const auto frame = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    return frame >= low && frame < high && frame - low >= InlineOptimizerStackBytes;
}

bool HasLimitedUseTypes(std::span<const std::uint32_t> spirv) {
    if (spirv.size() < 5) throw std::runtime_error("SPIRV-Tools: incomplete module header");
    bool int8 = false, int16 = false, float16 = false;
    bool int8Capability = false, int16Capability = false, float16Capability = false;
    const bool swapped = spirv[0] != spv::MagicNumber;
    const auto wordAt = [spirv, swapped](std::size_t index) {
        const auto word = spirv[index];
        return swapped ? (word >> 24u) | ((word >> 8u) & 0x0000ff00u) |
                             ((word << 8u) & 0x00ff0000u) | (word << 24u)
                       : word;
    };
    if (wordAt(0) != spv::MagicNumber) throw std::runtime_error("SPIRV-Tools: invalid module magic");
    for (std::size_t offset = 5; offset < spirv.size();) {
        const auto instruction = wordAt(offset);
        const auto wordCount = instruction >> spv::WordCountShift;
        const auto opcode = instruction & spv::OpCodeMask;
        if (wordCount == 0 || wordCount > spirv.size() - offset ||
            (opcode == spv::OpCapability && wordCount < 2) ||
            (opcode == spv::OpTypeInt && wordCount < 4) ||
            (opcode == spv::OpTypeFloat && wordCount < 3)) {
            throw std::runtime_error("SPIRV-Tools: malformed instruction while scanning narrow types");
        }
        switch (opcode) {
        case spv::OpCapability:
            int8Capability |= wordAt(offset + 1) == spv::CapabilityInt8 ||
                              wordAt(offset + 1) == spv::CapabilityDotProductInput4x8Bit;
            int16Capability |= wordAt(offset + 1) == spv::CapabilityInt16;
            float16Capability |= wordAt(offset + 1) == spv::CapabilityFloat16;
            break;
        case spv::OpTypeInt:
            int8 |= wordAt(offset + 2) == 8;
            int16 |= wordAt(offset + 2) == 16;
            break;
        case spv::OpTypeFloat:
            float16 |= wordCount == 3 && wordAt(offset + 2) == 16;
            break;
        case spv::OpFunction:
            return (int8 && !int8Capability) || (int16 && !int16Capability) || (float16 && !float16Capability);
        }
        offset += wordCount;
    }
    return (int8 && !int8Capability) || (int16 && !int16Capability) || (float16 && !float16Capability);
}

std::vector<std::uint32_t> CollapseBlockBallots(std::span<const std::uint32_t> spirv) {
    if (spirv.size() < 5) throw std::runtime_error("SPIRV-Tools: incomplete module header");
    const bool swapped = spirv[0] != spv::MagicNumber;
    const auto native = [swapped](std::uint32_t word) {
        return swapped ? (word >> 24u) | ((word >> 8u) & 0x0000ff00u) |
                             ((word << 8u) & 0x00ff0000u) | (word << 24u)
                       : word;
    };
    std::vector<std::uint32_t> collapsed(spirv.begin(), spirv.begin() + 5);
    collapsed.reserve(spirv.size());
    std::map<std::array<std::uint32_t, 3>, std::uint32_t> ballots;
    for (std::size_t offset = 5; offset < spirv.size();) {
        const auto instruction = native(spirv[offset]);
        const auto wordCount = instruction >> spv::WordCountShift;
        const auto opcode = instruction & spv::OpCodeMask;
        if (wordCount == 0 || wordCount > spirv.size() - offset ||
            (opcode == spv::OpGroupNonUniformBallot && wordCount != 5)) {
            throw std::runtime_error("SPIRV-Tools: malformed instruction while collapsing block ballots");
        }
        switch (opcode) {
        case spv::OpLabel:
        case spv::OpFunction:
        case spv::OpFunctionEnd:
        case spv::OpFunctionCall:
        case spv::OpDemoteToHelperInvocation:
            ballots.clear();
            break;
        case spv::OpGroupNonUniformBallot: {
            const auto [found, inserted] = ballots.try_emplace({spirv[offset + 1], spirv[offset + 3], spirv[offset + 4]}, spirv[offset + 2]);
            if (!inserted) {
                collapsed.insert(collapsed.end(), {native((4u << spv::WordCountShift) | spv::OpCopyObject), spirv[offset + 1], spirv[offset + 2], found->second});
                offset += wordCount;
                continue;
            }
            break;
        }
        }
        collapsed.insert(collapsed.end(), spirv.begin() + offset, spirv.begin() + offset + wordCount);
        offset += wordCount;
    }
    return collapsed;
}

std::vector<std::uint32_t> ValidateAndOptimizeSpirvInline(std::span<const std::uint32_t> spirv, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, bool allowOffsetTextureOperand, bool optimize, bool specialize) {
    spv_target_env environment;
    const auto apiVersion = vulkanVersion & ~0xfffu;
    std::uint32_t maxSpirvVersion = 0;
    switch (apiVersion) {
    case 0x00400000u: environment = SPV_ENV_VULKAN_1_0; maxSpirvVersion = 0x00010000u; break;
    case 0x00401000u: environment = spirvVersion == 0x00010400u ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1; maxSpirvVersion = 0x00010400u; break;
    case 0x00402000u: environment = SPV_ENV_VULKAN_1_2; maxSpirvVersion = 0x00010500u; break;
    case 0x00403000u: environment = SPV_ENV_VULKAN_1_3; maxSpirvVersion = 0x00010600u; break;
    case 0x00404000u: environment = SPV_ENV_VULKAN_1_4; maxSpirvVersion = 0x00010600u; break;
    default: throw std::runtime_error("SPIRV-Tools: unsupported Vulkan target");
    }
    if (spirvVersion < 0x00010000u || spirvVersion > maxSpirvVersion || (spirvVersion & 0xffu) != 0) {
        throw std::runtime_error("SPIRV-Tools: unsupported Vulkan/SPIR-V target");
    }
    if (spirv.size() >= 5 && spirv[1] > spirvVersion) {
        throw std::runtime_error("SPIRV-Tools: module version exceeds requested SPIR-V target");
    }
    std::string diagnostics;
    const auto consumer = [&diagnostics](spv_message_level_t level, const char* source, const spv_position_t& position, const char* message) {
        if (!diagnostics.empty()) diagnostics += '\n';
        diagnostics += "level=" + std::to_string(static_cast<int>(level)) + " word=" + std::to_string(position.index);
        if (source != nullptr && *source != '\0') diagnostics += " source=" + std::string(source);
        if (message != nullptr) diagnostics += ": " + std::string(message);
    };
    spvtools::SpirvTools tools(environment);
    if (!tools.IsValid()) throw std::runtime_error("SPIRV-Tools: cannot create validator");
    tools.SetMessageConsumer(consumer);
    spvtools::ValidatorOptions validatorOptions;
    validatorOptions.SetAllowOffsetTextureOperand(allowOffsetTextureOperand);
    validatorOptions.SetFriendlyNames(false);
    if (!tools.Validate(spirv.data(), spirv.size(), validatorOptions)) {
        throw std::runtime_error("SPIR-V validation before optimization failed:\n" + diagnostics);
    }
    if (!optimize) return CollapseBlockBallots(spirv);
    spvtools::Optimizer optimizer(environment);
    optimizer.SetMessageConsumer(consumer);
    static const char* mode = std::getenv("APS5_SPIRV_OPT");
    if (mode != nullptr && std::string(mode) == "none") return CollapseBlockBallots(spirv);
    // The recompiler emits helpers (BDA lookup, fault reporting) as functions called from every memory
    // access. Exhaustive inlining multiplies module size by ~10x and optimization time by ~20x, so the
    // default pipeline keeps the performance passes that work per function and leaves inlining to the driver.
    // APS5_SPIRV_OPT=full restores the SPIRV-Tools performance pipeline, =none skips optimization.
    const bool allowFolding = !HasLimitedUseTypes(spirv);
    const bool full = mode != nullptr && std::string(mode) == "full";
    if (full && allowFolding) {
        optimizer.RegisterPerformancePasses(true);
    } else {
        optimizer.RegisterPass(spvtools::CreateWrapOpKillPass())
            .RegisterPass(spvtools::CreateDeadBranchElimPass());
        if (full) {
            optimizer.RegisterPass(spvtools::CreateMergeReturnPass())
                .RegisterPass(spvtools::CreateInlineExhaustivePass())
                .RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());
        }
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreatePrivateToLocalPass())
            .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreateScalarReplacementPass(ScalarReplacementLimit))
            .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
            .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreateLocalMultiStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true));
        if (allowFolding) optimizer.RegisterPass(spvtools::CreateCCPPass());
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass(true));
        if (full) optimizer.RegisterPass(spvtools::CreateLoopUnrollPass(true));
        optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass())
            .RegisterPass(spvtools::CreateRedundancyEliminationPass());
        if (full) optimizer.RegisterPass(spvtools::CreateCombineAccessChainsPass());
        if (allowFolding) optimizer.RegisterPass(spvtools::CreateSimplificationPass());
        if (full) {
            optimizer.RegisterPass(spvtools::CreateScalarReplacementPass(ScalarReplacementLimit))
                .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
                .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
                .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
                .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
                .RegisterPass(spvtools::CreateSSARewritePass())
                .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
                .RegisterPass(spvtools::CreateVectorDCEPass())
                .RegisterPass(spvtools::CreateDeadInsertElimPass())
                .RegisterPass(spvtools::CreateDeadBranchElimPass())
                .RegisterPass(spvtools::CreateIfConversionPass())
                .RegisterPass(spvtools::CreateCopyPropagateArraysPass())
                .RegisterPass(spvtools::CreateReduceLoadSizePass());
        }
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreateBlockMergePass());
        if (full) {
            optimizer.RegisterPass(spvtools::CreateRedundancyEliminationPass())
                .RegisterPass(spvtools::CreateDeadBranchElimPass())
                .RegisterPass(spvtools::CreateBlockMergePass());
        }
        if (allowFolding) optimizer.RegisterPass(spvtools::CreateSimplificationPass());
    }
    // Validating after every pass costs more than the passes on large shaders; the result is validated below.
    optimizer.SetValidateAfterAll(false);
    spvtools::OptimizerOptions options;
    options.set_run_validator(false);
    options.set_preserve_bindings(true);
    options.set_preserve_spec_constants(!specialize);
    options.set_validator_options(validatorOptions);
    std::vector<std::uint32_t> optimized;
    diagnostics.clear();
    if (!optimizer.Run(spirv.data(), spirv.size(), &optimized, options)) {
        throw std::runtime_error("SPIR-V optimization failed:\n" + diagnostics);
    }
    optimized = CollapseBlockBallots(optimized);
    diagnostics.clear();
    if (!tools.Validate(optimized.data(), optimized.size(), validatorOptions)) {
        throw std::runtime_error("SPIR-V validation after optimization failed:\n" + diagnostics);
    }
    return optimized;
}

}

std::vector<std::uint32_t> ValidateAndOptimizeSpirv(std::span<const std::uint32_t> spirv, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, bool allowOffsetTextureOperand, bool optimize, bool specialize) {
    if (StackHoldsOptimizer()) return ValidateAndOptimizeSpirvInline(spirv, vulkanVersion, spirvVersion, allowOffsetTextureOperand, optimize, specialize);
    std::vector<std::uint32_t> result;
    std::exception_ptr failure;
    std::thread host([&] {
        try {
            result = ValidateAndOptimizeSpirvInline(spirv, vulkanVersion, spirvVersion, allowOffsetTextureOperand, optimize, specialize);
        } catch (...) {
            failure = std::current_exception();
        }
    });
    optimizerHostThreads.fetch_add(1, std::memory_order_relaxed);
    host.join();
    if (failure) std::rethrow_exception(failure);
    return result;
}

std::uint64_t OptimizerHostThreads() {
    return optimizerHostThreads.load(std::memory_order_relaxed);
}

}
