#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_ORACLEDISPATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_ORACLEDISPATCH_HPP

#include "OracleCodeObject.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

std::vector<std::byte> RunOracleKernel(AgcDriver::VulkanDevice& device, const OracleKernel& kernel, std::uint32_t threads, std::span<const std::byte> input);

#endif
