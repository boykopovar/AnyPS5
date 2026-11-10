#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SHADERFUSION_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SHADERFUSION_HPP

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

// Firmware layout: the console writes the size as a 64-bit value at +0 and the
// alignment as a 32-bit value at +8, leaving the high 32 bits of m_align (+12)
// untouched. Callers must read only the low 32 bits of m_align.
struct SizeAlign {
    std::uint64_t m_size;
    std::size_t m_align;
};
static_assert(offsetof(SizeAlign, m_align) == 8);

extern "C" int APS5_VABI sceAgcFuseShaderHalves_0200(Shader* fusedResult, const Shader* front, const Shader* back, void* scratchMem);
extern "C" int APS5_VABI sceAgcGetFusedShaderSize_0080(SizeAlign* dst, const Shader* front, const Shader* back);
extern "C" int APS5_VABI sceAgcGetFusedShaderSize(SizeAlign* dst, const Shader* front, const Shader* back);

#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcFuseShaderHalves_nid_postfix(Shader* fusedResult, const Shader* front, const Shader* back, void* scratchMem);

}

#endif
