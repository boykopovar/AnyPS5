#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GPU_BDASHADER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GPU_BDASHADER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <vector>

struct BdaTestDevice {
    const AgcDriver::Graphics::Context& context;
    bool runsOnCpu;
};

const BdaTestDevice& SharedBdaTestDevice();
std::vector<std::uint32_t> MakeBdaTestShader(std::uint64_t address, std::uint32_t bits, std::int64_t offset = 0);
std::vector<std::uint32_t> MakeBdaDwordReadTestShader(std::uint64_t address, std::uint32_t dwords, bool coherent, bool stops);
std::vector<std::uint32_t> MakeBdaSpanReadTestShader(std::uint64_t address, std::uint32_t offset, std::uint32_t extracted, bool coherent, bool stops);

#endif // CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GPU_BDASHADER_HPP
