#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BDASHADER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BDASHADER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "BdaAbi.hpp"
#include <cstdint>
#include <initializer_list>
#include <vector>

std::vector<std::uint32_t> MakeBdaTestShader(std::uint64_t address, std::uint32_t bits, std::int64_t offset = 0);
std::vector<std::uint32_t> MakeBdaDwordReadTestShader(std::uint64_t address, std::uint32_t dwords, bool coherent, bool stops);
std::vector<std::uint32_t> MakeBdaSpanReadTestShader(std::uint64_t address, std::uint32_t offset, std::uint32_t extracted, bool coherent, bool stops);
std::vector<std::uint32_t> MakeBdaFaultSequenceTestShader(std::initializer_list<ShaderRecompiler::BdaAbi::FaultReason> reasons);
std::vector<std::uint32_t> MakeBdaDwordWriteTestShader(std::uint64_t address, std::uint32_t dwords, const std::uint32_t* values);
std::vector<std::uint32_t> MakeBdaStoreTestShader(std::uint64_t address, std::uint32_t value, std::uint32_t bits, bool coherent);
std::vector<std::uint32_t> MakeBdaWriteTestShader(std::uint64_t address, std::uint32_t value, std::uint32_t bits, bool coherent);
inline constexpr std::uint32_t BdaAnchoredReadInputWord = 256;
struct BdaAnchoredRead {
    std::int32_t immediate = 0;
    std::uint32_t bits = 32;
    std::uint32_t dwords = 1;
    std::uint32_t used = 1;
};
struct BdaAnchoredReadProgram {
    std::vector<BdaAnchoredRead> reads;
    bool stops = true;
    bool elect = false;
    bool coherent = false;
    std::uint32_t lanes = 1;
    bool sentinel = false;
};
inline constexpr std::uint32_t BdaAnchoredReadSentinel = 0xdeadbeefu;
std::vector<std::uint32_t> MakeBdaAnchoredReadTestShader(const BdaAnchoredReadProgram& reads, bool anchored);
void RunBdaStoreExecutionTests(const AgcDriver::Graphics::Context& context);
void RunBdaExecutionTests(const AgcDriver::Graphics::Context& context);
void RunBdaContractTests();

#endif
