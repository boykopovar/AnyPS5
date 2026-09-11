#ifndef CODEGEN_X86_DECODEDINSTRUCTION_HPP
#define CODEGEN_X86_DECODEDINSTRUCTION_HPP

#include <cstdint>
#include <cstddef>

namespace Codegen {

struct DecodedInstruction {
    const std::uint8_t* Data;
    std::size_t Length;

    [[nodiscard]] bool IsShaNi() const;
    [[nodiscard]] bool IsExtrq() const;
    [[nodiscard]] bool IsInsertq() const;
    [[nodiscard]] bool IsMonitorx() const;
    [[nodiscard]] bool IsMwaitx() const;
    [[nodiscard]] bool IsClzero() const;
    [[nodiscard]] bool IsRdpru() const;
    [[nodiscard]] bool IsMcommit() const;
    [[nodiscard]] bool IsMovntss() const;
    [[nodiscard]] bool IsMovntsd() const;

private:
    [[nodiscard]] std::size_t _skipPrefixesAndRex(bool* outHasOperandSizePrefix, bool* outHasRepnePrefix, bool* outHasRepPrefix) const;
};

}

#endif
