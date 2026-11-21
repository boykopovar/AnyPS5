#ifndef CODEGEN_X86_HOSTCPUFEATURECLASSIFIER_HPP
#define CODEGEN_X86_HOSTCPUFEATURECLASSIFIER_HPP

#include <cstddef>
#include <cstdint>

namespace Codegen {

[[nodiscard]] std::uint64_t RequiredHostCpuFeatures(const std::uint8_t* data, std::size_t length);

}

#endif
