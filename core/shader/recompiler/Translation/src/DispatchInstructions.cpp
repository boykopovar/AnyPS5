#include "Translation/InstructionTranslator.hpp"
#include "Translation/TranslationContext.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

void TranslationContext::TranslateInstruction(const RdnaInstruction& decoded) {
    RdnaInstruction instruction = decoded;
    instruction.destination = destinationOperand(decoded);
    currentOpcode = instruction.op;
    currentProgramCounter = instruction.programCounter;
    currentInstruction = &instruction;
    vectorsRead.reset();
    relativeVectorRead = false;
    if (instruction.op == RdnaOpcode::Unknown || instruction.op == RdnaOpcode::Count) {
        throw std::runtime_error("decoded opcode has no IR translation at pc " + std::to_string(instruction.programCounter));
    }
    if (instruction.op == RdnaOpcode::Unsupported) {
        throw std::runtime_error(instruction.unsupportedReason.empty() ? "unsupported decoded instruction at pc " + std::to_string(instruction.programCounter) : std::string(instruction.unsupportedReason));
    }
    checkRoundingUse(instruction);
    f32DenormalFlush = f32DenormalFlushFor(instruction);
    bool translated = false;
    switch (instruction.family) {
        case RdnaInstructionFamily::SOP1:
        case RdnaInstructionFamily::SOP2:
        case RdnaInstructionFamily::SOPK:
        case RdnaInstructionFamily::SOPC:
        case RdnaInstructionFamily::SOPP:
            translated = emitScalar(instruction);
            break;
        case RdnaInstructionFamily::VOP1:
        case RdnaInstructionFamily::VOP2:
        case RdnaInstructionFamily::VOP3:
        case RdnaInstructionFamily::VOP3P:
        case RdnaInstructionFamily::VOPC:
            translated = emitVector(instruction) || emitInterpolation(instruction);
            break;
        case RdnaInstructionFamily::SMEM:
        case RdnaInstructionFamily::MUBUF:
        case RdnaInstructionFamily::MTBUF:
        case RdnaInstructionFamily::FLAT:
        case RdnaInstructionFamily::DS:
        case RdnaInstructionFamily::MIMG:
            translated = emitMemory(instruction);
            break;
        case RdnaInstructionFamily::VINTRP:
            translated = emitInterpolation(instruction);
            break;
        case RdnaInstructionFamily::EXP:
            eXP(instruction);
            translated = true;
            break;
        default:
            break;
    }
    if (!translated) {
        throw std::runtime_error("opcode has no IR translation at pc " + std::to_string(instruction.programCounter));
    }
    if (const DebugProbe probe = DebugProbeConfig(); probe.enabled && instruction.programCounter == probe.programCounter) {
        RdnaOperand source{};
        source.kind = RdnaOperandKind::VectorRegister;
        source.reg = probe.vgpr;
        RdnaOperand target{};
        target.kind = RdnaOperandKind::VectorRegister;
        target.reg = 255u;
        writeOperand(target, &readRawU32(source).Value());
    }
    currentInstruction = nullptr;
    trackInactiveExecHalves(instruction);
}

namespace {
std::atomic<bool> g_debugProbeActive{false};
}

void SetDebugProbeActive(bool active) {
    g_debugProbeActive.store(active);
}

bool DebugProbeActive() {
    return g_debugProbeActive.load();
}

bool RayTracingStrict() {
    static const bool strict = [] {
        const char* text = std::getenv("APS5_RAYTRACING");
        return text != nullptr && std::strcmp(text, "strict") == 0;
    }();
    return strict;
}

bool RayTracingMiss() {
    static const bool miss = [] {
        const char* text = std::getenv("APS5_RAYTRACING");
        return text != nullptr && std::strcmp(text, "miss") == 0;
    }();
    return miss;
}

const NanSpeculationConfig& NanSpeculation() {
    static const NanSpeculationConfig config = [] {
        NanSpeculationConfig result;
        const char* enabled = std::getenv("APS5_NAN_SPECULATION");
        result.enabled = enabled != nullptr && *enabled != '\0' && std::strcmp(enabled, "0") != 0;
        const auto number = [](const char* text, std::uint32_t fallback) {
            if (text == nullptr || *text == '\0') return fallback;
            char* end = nullptr;
            const auto value = std::strtoul(text, &end, 10);
            return end != text && *end == '\0' && value > 0u && value <= 4096u ? static_cast<std::uint32_t>(value) : fallback;
        };
        constexpr std::array<std::pair<std::string_view, ShaderStageKind>, 6> names{{{"compute", ShaderStageKind::Compute}, {"vertex", ShaderStageKind::Vertex}, {"pixel", ShaderStageKind::Pixel}, {"fetch", ShaderStageKind::Fetch}, {"mesh", ShaderStageKind::Mesh}, {"unknown", ShaderStageKind::Unknown}}};
        if (const char* stages = std::getenv("APS5_NAN_SPECULATION_STAGES"); stages != nullptr && *stages != '\0') {
            result.stageMask = 0u;
            std::string_view rest(stages);
            while (!rest.empty()) {
                const auto comma = rest.find(',');
                const auto name = rest.substr(0u, comma);
                rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1u);
                for (const auto& [text, kind] : names) {
                    if (name == text || name == "all") result.stageMask |= 1u << static_cast<std::uint32_t>(kind);
                }
            }
        }
        result.minimumVectorInstructions = number(std::getenv("APS5_NAN_SPECULATION_MIN"), result.minimumVectorInstructions);
        result.stageMinimum.fill(result.minimumVectorInstructions);
        if (const char* minimum = std::getenv("APS5_NAN_SPECULATION_MIN"); minimum != nullptr && std::strchr(minimum, ':') != nullptr) {
            std::string_view rest(minimum);
            while (!rest.empty()) {
                const auto comma = rest.find(',');
                const auto entry = rest.substr(0u, comma);
                rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1u);
                const auto colon = entry.find(':');
                if (colon == std::string_view::npos) continue;
                const auto name = entry.substr(0u, colon);
                const std::string digits(entry.substr(colon + 1u));
                char* end = nullptr;
                const auto value = std::strtoul(digits.c_str(), &end, 10);
                if (end == digits.c_str() || *end != '\0' || value == 0u || value > 4096u) continue;
                for (const auto& [text, kind] : names) {
                    if (name == text || name == "all") result.stageMinimum[static_cast<std::size_t>(kind)] = static_cast<std::uint32_t>(value);
                }
            }
        }
        std::uint32_t lowest = result.stageMinimum[0];
        for (const auto value : result.stageMinimum) lowest = std::min(lowest, value);
        result.minimumVectorInstructions = lowest;
        result.maximumVectorInstructions = std::max(*std::ranges::max_element(result.stageMinimum), number(std::getenv("APS5_NAN_SPECULATION_MAX"), result.maximumVectorInstructions));
        const auto percent = [](const char* text) {
            if (text == nullptr || *text == '\0') return 0u;
            char* end = nullptr;
            const auto value = std::strtoul(text, &end, 10);
            return end != text && *end == '\0' && value <= 4096u ? static_cast<std::uint32_t>(value) : 0u;
        };
        result.minimumFastOps = percent(std::getenv("APS5_NAN_SPECULATION_MIN_OPS"));
        result.minimumDensityPercent = percent(std::getenv("APS5_NAN_SPECULATION_MIN_DENSITY"));
        const char* count = std::getenv("APS5_NAN_SPECULATION_COUNT");
        result.count = result.enabled && count != nullptr && *count != '\0' && std::strcmp(count, "0") != 0;
        return result;
    }();
    return config;
}

namespace {

std::mutex& CounterMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<NanSpeculationCounterSite>& CounterSites() {
    static std::vector<NanSpeculationCounterSite> sites;
    return sites;
}

}

namespace {

std::atomic<std::uint32_t>& SuppressedCount() {
    static std::atomic<std::uint32_t> count{0u};
    return count;
}

std::vector<std::uint64_t>& SuppressedAddresses() {
    static std::vector<std::uint64_t> addresses;
    return addresses;
}

}

bool NanSpeculationSuppressed(std::uint64_t address) {
    if (SuppressedCount().load(std::memory_order_acquire) == 0u) return false;
    const std::lock_guard lock(CounterMutex());
    return std::ranges::find(SuppressedAddresses(), address) != SuppressedAddresses().end();
}

bool SuppressNanSpeculation(std::uint64_t address) {
    const std::lock_guard lock(CounterMutex());
    if (std::ranges::find(SuppressedAddresses(), address) != SuppressedAddresses().end()) return false;
    SuppressedAddresses().push_back(address);
    SuppressedCount().fetch_add(1u, std::memory_order_release);
    return true;
}

std::uint32_t NanSpeculationFallbacks() {
    return SuppressedCount().load(std::memory_order_acquire);
}

std::uint32_t NanSpeculationCounterSlot(std::uint64_t address, std::uint32_t stage) {
    const std::lock_guard lock(CounterMutex());
    auto& sites = CounterSites();
    for (const auto& site : sites) {
        if (site.address == address && site.stage == stage) return site.slot;
    }
    if (sites.size() >= NanSpeculationCounterSlots) return NanSpeculationCounterSlots;
    sites.push_back({address, stage, static_cast<std::uint32_t>(sites.size())});
    return sites.back().slot;
}

std::vector<NanSpeculationCounterSite> NanSpeculationCounterSites() {
    const std::lock_guard lock(CounterMutex());
    return CounterSites();
}

bool ExactF32Arithmetic() {
    static const bool exact = [] {
        const char* text = std::getenv("APS5_FAST_F32");
        return text == nullptr || *text == '\0' || std::strcmp(text, "0") == 0;
    }();
    return exact;
}

DebugProbe DebugProbeConfig() {
    static const DebugProbe parsed = [] {
        DebugProbe result;
        const char* text = std::getenv("APS5_PROBE");
        if (text == nullptr) return result;
        char* end = nullptr;
        result.programCounter = static_cast<std::uint32_t>(std::strtoul(text, &end, 16));
        if (end == nullptr || *end != ':') return result;
        result.vgpr = static_cast<std::uint32_t>(std::strtoul(end + 1, &end, 10));
        if (end != nullptr && *end == ':') result.shift = static_cast<std::uint32_t>(std::strtoul(end + 1, nullptr, 10));
        result.enabled = result.vgpr < 255u;
        return result;
    }();
    DebugProbe probe = parsed;
    probe.enabled = parsed.enabled && DebugProbeActive();
    return probe;
}

}