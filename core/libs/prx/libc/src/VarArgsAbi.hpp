#ifndef CORE_LIBS_PRX_LIBC_VARARGSABI_HPP
#define CORE_LIBS_PRX_LIBC_VARARGSABI_HPP

#include <cstdarg>
#include <cstdint>
#include "SceTypes.hpp"

namespace LibcDetail {

struct RegSaveArea {
    std::uint64_t gp[6];
    __m128 fp[8];
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
    __m128 fp0, __m128 fp1, __m128 fp2, __m128 fp3,
    __m128 fp4, __m128 fp5, __m128 fp6, __m128 fp7
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
