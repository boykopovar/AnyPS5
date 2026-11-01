#ifndef CORE_SHADER_RECOMPILER_SHADERCACHESOURCEVERSION_HPP
#define CORE_SHADER_RECOMPILER_SHADERCACHESOURCEVERSION_HPP

#include <cstdint>
#include <span>
#include <string_view>

namespace ShaderRecompiler::ShaderDiskCache::Generated {

extern const std::uint64_t SourceVersion;
extern const std::span<const std::string_view> RecompilerSwitches;

}

#endif
