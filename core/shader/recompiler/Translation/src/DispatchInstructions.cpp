#include "Translation/DispatchInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include "Recompiler.hpp"
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

void DispatchInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const ControlFlowGraph& cfg, const TranslateOptions& options) {
    throw std::runtime_error("DispatchInstruction not implemented");
}

void TranslationContext::TranslateInstruction(const RdnaInstruction& instruction) {
    currentOpcode = instruction.op;
    currentProgramCounter = instruction.programCounter;
    if (instruction.op == RdnaOpcode::Unknown || instruction.op == RdnaOpcode::Count) {
        throw std::runtime_error("decoded opcode has no IR translation at pc " + std::to_string(instruction.programCounter));
    }
    if (instruction.op == RdnaOpcode::Unsupported) {
        throw std::runtime_error(instruction.unsupportedReason.empty() ? "unsupported decoded instruction at pc " + std::to_string(instruction.programCounter) : std::string(instruction.unsupportedReason));
    }
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
            translated = emitVector(instruction);
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

void DispatchInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("DispatchInstruction not implemented");
}

}
