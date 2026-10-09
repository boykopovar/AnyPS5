#include "ControlFlow/UserDataCalls.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

constexpr std::size_t MaxCalleeWords = 16384;
constexpr std::size_t MaxCombinedWords = 262144;

void requireLeafInstruction(const RdnaInstruction& instruction, bool last, std::uint32_t link) {
    if (instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) {
        throw std::invalid_argument("nested scalar calls in a captured callee are unsupported");
    }
    if (instruction.op == RdnaOpcode::SGetpcB64) throw std::invalid_argument("PC-relative captured callees are unsupported");
    if (instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) {
        throw std::invalid_argument("captured callee ends without returning");
    }
    if (instruction.op == RdnaOpcode::SSetpcB64 && (!last || instruction.source0.kind != RdnaOperandKind::ScalarRegister || instruction.source0.reg != link)) {
        throw std::invalid_argument("captured callee has an unsupported return");
    }
}

}

CapturedCallProgram ResolveUserDataCalls(const RecompileRequest& request, SrtMemoryReader reader, void* context) {
    if (!request.shader.capturedCalls.empty()) throw std::invalid_argument("shader calls have already been captured");
    if (request.shader.code.size() > MaxCombinedWords) throw std::invalid_argument("captured shader exceeds the combined program size limit");
    const auto program = RdnaInstructionDecoder{}.Decode(request.shader.code);
    const SwappcInfo info{request.context.vertex.has_value(), request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size())};
    const auto targets = AnalyzeUserDataCalls(program, info);
    CapturedCallProgram result;
    if (targets.empty()) return result;
    if (reader == nullptr) throw std::invalid_argument("captured scalar calls require a memory reader");
    result.code.assign(request.shader.code.begin(), request.shader.code.end());
    for (const auto& target : targets) {
        const auto address = static_cast<std::uint64_t>(request.context.userData[target.userDataIndex]) |
            (static_cast<std::uint64_t>(request.context.userData[target.userDataIndex + 1u]) << 32u);
        if (address == 0 || (address & 3u) != 0u) throw std::invalid_argument("captured scalar call target is null or misaligned");
        if (address >= request.shader.codeAddress && address - request.shader.codeAddress < request.shader.code.size_bytes()) {
            throw std::invalid_argument("recursive captured scalar call targets the caller");
        }
        const auto& call = program.instructions[target.callIndex];
        const auto start = static_cast<std::uint32_t>(result.code.size() * 4u);
        std::vector<std::uint32_t> callee;
        bool returned = false;
        while (callee.size() < MaxCalleeWords) {
            const auto wordIndex = static_cast<std::uint32_t>(callee.size());
            RdnaInstruction instruction;
            for (std::size_t words = 0; ; ++words) {
                if (words >= MaxRdnaInstructionRawWords || callee.size() >= MaxCalleeWords) throw std::invalid_argument("captured callee instruction exceeds the size limit");
                const auto offset = callee.size() * 4u;
                if (address > std::numeric_limits<std::uint64_t>::max() - offset - 4u) throw std::invalid_argument("captured scalar call target arithmetic overflow");
                std::uint32_t word = 0;
                if (!reader(context, address + offset, &word)) throw std::invalid_argument("captured scalar call target is unmapped");
                callee.push_back(word);
                try {
                    instruction = DecodeRdnaInstruction(wordIndex * 4u, callee, wordIndex);
                    break;
                } catch (const std::out_of_range&) {
                    if (words + 1u == MaxRdnaInstructionRawWords) throw;
                }
            }
            returned = instruction.op == RdnaOpcode::SSetpcB64;
            requireLeafInstruction(instruction, returned, call.destination.reg);
            for (const auto& source : targets) {
                const auto reg = request.context.userDataBaseRegister + source.userDataIndex;
                if (WritesScalarRegister(instruction, reg) || WritesScalarRegister(instruction, reg + 1u)) {
                    throw std::invalid_argument("captured callee clobbers a user-data call target");
                }
            }
            if (returned) break;
        }
        if (!returned) throw std::invalid_argument("captured callee exceeds the size limit without returning");
        if (callee.size() > MaxCombinedWords - result.code.size()) throw std::invalid_argument("captured shader exceeds the combined program size limit");
        result.calls.push_back({call.programCounter, start, static_cast<std::uint32_t>(start + callee.size() * 4u - 4u), address, target.userDataIndex});
        result.code.insert(result.code.end(), callee.begin(), callee.end());
    }
    auto shader = request.shader;
    shader.code = result.code;
    shader.capturedCalls = result.calls;
    const auto combined = DecodeShaderProgram(shader);
    auto capturedInfo = info;
    capturedInfo.capturedCalls = result.calls;
    static_cast<void>(GraphBuilder{}.Build(combined, &capturedInfo));
    return result;
}

RdnaProgram DecodeShaderProgram(const ShaderBinary& shader) {
    if (shader.capturedCalls.size() > MaxCapturedShaderCalls) throw std::invalid_argument("too many captured scalar calls");
    if (!shader.capturedCalls.empty() && shader.code.size() > MaxCombinedWords) throw std::invalid_argument("captured shader exceeds the combined program size limit");
    auto program = RdnaInstructionDecoder{}.Decode(shader.code);
    const auto callerEnd = program.instructions.back().programCounter + program.instructions.back().wordCount * 4u;
    auto end = callerEnd;
    for (const auto& call : shader.capturedCalls) {
        const auto* instruction = FindInstructionAtProgramCounter(program, call.callProgramCounter);
        if (instruction == nullptr || instruction->op != RdnaOpcode::SSwappcB64 || call.callProgramCounter >= callerEnd ||
            call.targetProgramCounter < end || call.returnProgramCounter < call.targetProgramCounter || (call.targetProgramCounter & 3u) != 0 ||
            (call.returnProgramCounter & 3u) != 0 || static_cast<std::uint64_t>(call.returnProgramCounter) + 4u > shader.code.size_bytes()) {
            throw std::invalid_argument("invalid captured scalar call boundaries");
        }
        const auto bytes = static_cast<std::uint64_t>(call.returnProgramCounter) + 4u - call.targetProgramCounter;
        if (bytes > MaxCalleeWords * 4u || call.targetAddress == 0 || (call.targetAddress & 3u) != 0 || call.targetAddress > std::numeric_limits<std::uint64_t>::max() - bytes) {
            throw std::invalid_argument("invalid captured scalar call target or size");
        }
        const auto link = instruction->destination.reg;
        for (auto pc = call.targetProgramCounter; pc <= call.returnProgramCounter; ) {
            auto decoded = DecodeRdnaInstruction(pc, shader.code, pc / 4u);
            const bool last = pc == call.returnProgramCounter;
            requireLeafInstruction(decoded, last, link);
            if (last && decoded.op != RdnaOpcode::SSetpcB64) throw std::invalid_argument("captured callee has no return");
            if (decoded.wordCount * 4u > call.returnProgramCounter + 4u - pc) throw std::invalid_argument("captured callee return is not an instruction boundary");
            pc += decoded.wordCount * 4u;
            program.instructions.push_back(std::move(decoded));
        }
        end = call.returnProgramCounter + 4u;
    }
    return program;
}

}
