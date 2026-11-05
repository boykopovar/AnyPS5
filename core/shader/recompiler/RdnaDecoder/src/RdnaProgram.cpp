#include "RdnaDecoder/RdnaProgram.hpp"
#include <algorithm>

namespace ShaderRecompiler {

    const RdnaInstruction* FindInstructionAtProgramCounter(const RdnaProgram& program, std::uint32_t programCounter) {
        const auto it = std::lower_bound(program.instructions.begin(), program.instructions.end(), programCounter,
            [](const RdnaInstruction& instruction, std::uint32_t pc) { return instruction.programCounter < pc; });
        if (it == program.instructions.end() || it->programCounter != programCounter) {
            return nullptr;
        }
        return &(*it);
    }

}
