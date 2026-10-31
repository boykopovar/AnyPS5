#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SHADERFUSION_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SHADERFUSION_HPP

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

struct SizeAlign {
    std::uint64_t m_size;
    std::size_t m_align;
};

extern "C" int APS5_VABI sceAgcFuseShaderHalves_0200(Shader* fusedResult, const Shader* front, const Shader* back, void* scratchMem);
extern "C" int APS5_VABI sceAgcGetFusedShaderSize_0080(SizeAlign* dst, const Shader* front, const Shader* back);
extern "C" int APS5_VABI sceAgcGetFusedShaderSize(SizeAlign* dst, const Shader* front, const Shader* back);

#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcFuseShaderHalves_nid_postfix(Shader* fusedResult, const Shader* front, const Shader* back, void* scratchMem);
int APS5_VABI sceAgcUpdateInterpolantMapping_0100(ShaderRegister* regs, const Shader* vs, const Shader* ps);

}

#endif
