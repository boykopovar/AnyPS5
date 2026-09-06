#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_VARARGSABI_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_VARARGSABI_HPP

#include <cstdarg>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/specifics/x86_64/SimdTypes.hpp"

namespace LibcDetail {

struct RegSaveArea {
    std::uint64_t gp[6];
    X86_64::Xmm fp[8];
};

struct VaListLayout {
    unsigned int gpOffset;
    unsigned int fpOffset;
    void* overflowArgArea;
    void* regSaveArea;
};

inline void FillRegSaveArea(
    RegSaveArea& regs,
    std::uint64_t gp0, std::uint64_t gp1, std::uint64_t gp2,
    std::uint64_t gp3, std::uint64_t gp4, std::uint64_t gp5,
    X86_64::Xmm fp0, X86_64::Xmm fp1, X86_64::Xmm fp2, X86_64::Xmm fp3,
    X86_64::Xmm fp4, X86_64::Xmm fp5, X86_64::Xmm fp6, X86_64::Xmm fp7
) {
    regs.gp[0] = gp0;
    regs.gp[1] = gp1;
    regs.gp[2] = gp2;
    regs.gp[3] = gp3;
    regs.gp[4] = gp4;
    regs.gp[5] = gp5;
    regs.fp[0] = fp0;
    regs.fp[1] = fp1;
    regs.fp[2] = fp2;
    regs.fp[3] = fp3;
    regs.fp[4] = fp4;
    regs.fp[5] = fp5;
    regs.fp[6] = fp6;
    regs.fp[7] = fp7;
}

inline std::va_list* BuildVaList(
    VaListLayout& layout, RegSaveArea& regs,
    unsigned int consumedGpRegisters, void* overflowArgArea
) {
    layout.gpOffset = consumedGpRegisters * 8u;
    layout.fpOffset = 6u * 8u;
    layout.overflowArgArea = overflowArgArea;
    layout.regSaveArea = &regs;
    return reinterpret_cast<std::va_list*>(&layout);
}

}

#endif
