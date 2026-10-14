#ifndef CORE_SHADER_RECOMPILER_STATICBUFFERDESCRIPTORABI_HPP
#define CORE_SHADER_RECOMPILER_STATICBUFFERDESCRIPTORABI_HPP

#include <cstdint>

namespace ShaderRecompiler {

enum class StaticBufferDescriptorStatus : std::uint8_t { Valid, UnsupportedType };

}

#endif
