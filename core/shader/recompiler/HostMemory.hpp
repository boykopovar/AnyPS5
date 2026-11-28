#ifndef CORE_SHADER_RECOMPILER_HOSTMEMORY_HPP
#define CORE_SHADER_RECOMPILER_HOSTMEMORY_HPP

#include <cstdint>

namespace ShaderRecompiler {

[[nodiscard]] std::uint64_t HostPhysicalMemoryBytes();

}

#endif
