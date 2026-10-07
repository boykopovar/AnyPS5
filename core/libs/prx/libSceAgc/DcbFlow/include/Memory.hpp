#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_MEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_MEMORY_HPP

#include "SceTypes.hpp"

extern "C" std::uint32_t* APS5_VABI sceAgcDcbAcquireMem(CommandBuffer* buf, std::uint8_t engine, std::uint32_t cbDbOp,
                                                        std::uint32_t gcrControl, const volatile void* base,
                                                        std::uint64_t sizeBytes, std::uint32_t pollCycles);

#endif
