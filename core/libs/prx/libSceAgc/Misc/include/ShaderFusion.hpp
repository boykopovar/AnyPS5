#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SHADERFUSION_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SHADERFUSION_HPP

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

struct SizeAlign {
    std::uint64_t m_size;
    std::size_t m_align;
};

extern "C" int APS5_VABI sceAgcFuseShaderHalves_0200(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem);
extern "C" int APS5_VABI sceAgcGetFusedShaderSize_0080(SizeAlign* dst, const Shader* front, const Shader* back);

#endif
