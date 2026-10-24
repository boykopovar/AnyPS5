#if defined(ANYPS5_ENABLE_SPIRV_TOOLS) && ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using GuestFiberEntry = void (APS5_VABI*)(std::uint64_t, std::uint64_t);

extern "C" {
std::int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix(FiberObject*, const char*, GuestFiberEntry, std::uint64_t, void*, std::uint64_t, const void*, std::uint32_t);
std::int32_t APS5_VABI sceFiberFinalize(FiberObject*);
std::int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject*, std::uint64_t, std::uint64_t*);
std::int32_t APS5_VABI sceFiberReturnToThread(std::uint64_t, std::uint64_t*);
}

namespace {

constexpr std::size_t ContextBytes = 64 * 1024;
constexpr std::size_t FiberStackLimit = 8 * 1024;
constexpr std::uint32_t VulkanVersion = 0x00401000u;
constexpr std::uint32_t SpirvVersion = 0x00010300u;
constexpr std::uint64_t Fill = 0x5aa5c33c5aa5c33cull;

alignas(16) std::array<std::uint64_t, ContextBytes / sizeof(std::uint64_t)> FiberContext;
struct alignas(8) FiberStorage {
    unsigned char bytes[0x100];
};
FiberStorage FiberObjectStorage;

constexpr std::array<std::uint32_t, 410> Module{
    0x07230203u, 0x00010300u, 0x0008000bu, 0x00000044u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0006000fu, 0x00000005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000bu, 0x00060010u, 0x00000004u,
    0x00000011u, 0x00000040u, 0x00000001u, 0x00000001u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u,
    0x00000004u, 0x6e69616du, 0x00000000u, 0x00040005u, 0x00000008u, 0x65646e69u, 0x00000078u, 0x00080005u,
    0x0000000bu, 0x475f6c67u, 0x61626f6cu, 0x766e496cu, 0x7461636fu, 0x496e6f69u, 0x00000044u, 0x00030005u,
    0x00000010u, 0x006d7573u, 0x00040005u, 0x00000011u, 0x74617473u, 0x00000065u, 0x00040005u, 0x00000013u,
    0x61746144u, 0x00000000u, 0x00050006u, 0x00000013u, 0x00000000u, 0x756c6176u, 0x00007365u, 0x00030005u,
    0x00000015u, 0x00000000u, 0x00030005u, 0x0000001cu, 0x00000069u, 0x00040047u, 0x0000000bu, 0x0000000bu,
    0x0000001cu, 0x00040047u, 0x00000012u, 0x00000006u, 0x00000004u, 0x00030047u, 0x00000013u, 0x00000002u,
    0x00050048u, 0x00000013u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00040047u, 0x00000015u, 0x00000021u,
    0x00000000u, 0x00040047u, 0x00000015u, 0x00000022u, 0x00000000u, 0x00040047u, 0x00000043u, 0x0000000bu,
    0x00000019u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00040015u, 0x00000006u,
    0x00000020u, 0x00000000u, 0x00040020u, 0x00000007u, 0x00000007u, 0x00000006u, 0x00040017u, 0x00000009u,
    0x00000006u, 0x00000003u, 0x00040020u, 0x0000000au, 0x00000001u, 0x00000009u, 0x0004003bu, 0x0000000au,
    0x0000000bu, 0x00000001u, 0x0004002bu, 0x00000006u, 0x0000000cu, 0x00000000u, 0x00040020u, 0x0000000du,
    0x00000001u, 0x00000006u, 0x0003001du, 0x00000012u, 0x00000006u, 0x0003001eu, 0x00000013u, 0x00000012u,
    0x00040020u, 0x00000014u, 0x0000000cu, 0x00000013u, 0x0004003bu, 0x00000014u, 0x00000015u, 0x0000000cu,
    0x00040015u, 0x00000016u, 0x00000020u, 0x00000001u, 0x0004002bu, 0x00000016u, 0x00000017u, 0x00000000u,
    0x00040020u, 0x00000019u, 0x0000000cu, 0x00000006u, 0x0004002bu, 0x00000006u, 0x00000023u, 0x00000010u,
    0x00020014u, 0x00000024u, 0x0004002bu, 0x00000006u, 0x00000027u, 0x00000001u, 0x0004002bu, 0x00000006u,
    0x00000038u, 0x0019660du, 0x0004002bu, 0x00000006u, 0x0000003au, 0x3c6ef35fu, 0x0004002bu, 0x00000016u,
    0x0000003du, 0x00000001u, 0x0004002bu, 0x00000006u, 0x00000042u, 0x00000040u, 0x0006002cu, 0x00000009u,
    0x00000043u, 0x00000042u, 0x00000027u, 0x00000027u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u,
    0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003bu, 0x00000007u, 0x00000008u, 0x00000007u, 0x0004003bu,
    0x00000007u, 0x00000010u, 0x00000007u, 0x0004003bu, 0x00000007u, 0x00000011u, 0x00000007u, 0x0004003bu,
    0x00000007u, 0x0000001cu, 0x00000007u, 0x00050041u, 0x0000000du, 0x0000000eu, 0x0000000bu, 0x0000000cu,
    0x0004003du, 0x00000006u, 0x0000000fu, 0x0000000eu, 0x0003003eu, 0x00000008u, 0x0000000fu, 0x0003003eu,
    0x00000010u, 0x0000000cu, 0x0004003du, 0x00000006u, 0x00000018u, 0x00000008u, 0x00060041u, 0x00000019u,
    0x0000001au, 0x00000015u, 0x00000017u, 0x00000018u, 0x0004003du, 0x00000006u, 0x0000001bu, 0x0000001au,
    0x0003003eu, 0x00000011u, 0x0000001bu, 0x0003003eu, 0x0000001cu, 0x0000000cu, 0x000200f9u, 0x0000001du,
    0x000200f8u, 0x0000001du, 0x000400f6u, 0x0000001fu, 0x00000020u, 0x00000000u, 0x000200f9u, 0x00000021u,
    0x000200f8u, 0x00000021u, 0x0004003du, 0x00000006u, 0x00000022u, 0x0000001cu, 0x000500b0u, 0x00000024u,
    0x00000025u, 0x00000022u, 0x00000023u, 0x000400fau, 0x00000025u, 0x0000001eu, 0x0000001fu, 0x000200f8u,
    0x0000001eu, 0x0004003du, 0x00000006u, 0x00000026u, 0x00000011u, 0x000500c7u, 0x00000006u, 0x00000028u,
    0x00000026u, 0x00000027u, 0x000500abu, 0x00000024u, 0x00000029u, 0x00000028u, 0x0000000cu, 0x000300f7u,
    0x0000002bu, 0x00000000u, 0x000400fau, 0x00000029u, 0x0000002au, 0x00000031u, 0x000200f8u, 0x0000002au,
    0x0004003du, 0x00000006u, 0x0000002cu, 0x00000011u, 0x0004003du, 0x00000006u, 0x0000002du, 0x0000001cu,
    0x00050084u, 0x00000006u, 0x0000002eu, 0x0000002cu, 0x0000002du, 0x0004003du, 0x00000006u, 0x0000002fu,
    0x00000010u, 0x00050080u, 0x00000006u, 0x00000030u, 0x0000002fu, 0x0000002eu, 0x0003003eu, 0x00000010u,
    0x00000030u, 0x000200f9u, 0x0000002bu, 0x000200f8u, 0x00000031u, 0x0004003du, 0x00000006u, 0x00000032u,
    0x00000011u, 0x0004003du, 0x00000006u, 0x00000033u, 0x0000001cu, 0x00050080u, 0x00000006u, 0x00000034u,
    0x00000032u, 0x00000033u, 0x0004003du, 0x00000006u, 0x00000035u, 0x00000010u, 0x000500c6u, 0x00000006u,
    0x00000036u, 0x00000035u, 0x00000034u, 0x0003003eu, 0x00000010u, 0x00000036u, 0x000200f9u, 0x0000002bu,
    0x000200f8u, 0x0000002bu, 0x0004003du, 0x00000006u, 0x00000037u, 0x00000011u, 0x00050084u, 0x00000006u,
    0x00000039u, 0x00000037u, 0x00000038u, 0x00050080u, 0x00000006u, 0x0000003bu, 0x00000039u, 0x0000003au,
    0x0003003eu, 0x00000011u, 0x0000003bu, 0x000200f9u, 0x00000020u, 0x000200f8u, 0x00000020u, 0x0004003du,
    0x00000006u, 0x0000003cu, 0x0000001cu, 0x00050080u, 0x00000006u, 0x0000003eu, 0x0000003cu, 0x0000003du,
    0x0003003eu, 0x0000001cu, 0x0000003eu, 0x000200f9u, 0x0000001du, 0x000200f8u, 0x0000001fu, 0x0004003du,
    0x00000006u, 0x0000003fu, 0x00000008u, 0x0004003du, 0x00000006u, 0x00000040u, 0x00000010u, 0x00060041u,
    0x00000019u, 0x00000041u, 0x00000015u, 0x00000017u, 0x0000003fu, 0x0003003eu, 0x00000041u, 0x00000040u,
    0x000100fdu, 0x00010038u
};

std::vector<std::uint32_t> Optimized;
std::exception_ptr Failure;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> Optimize() {
    return ShaderRecompiler::ValidateAndOptimizeSpirv(Module, VulkanVersion, SpirvVersion);
}

void APS5_VABI OptimizeOnFiber(std::uint64_t, std::uint64_t) {
    try {
        Optimized = Optimize();
    } catch (...) {
        Failure = std::current_exception();
    }
    sceFiberReturnToThread(0, nullptr);
}

std::size_t FiberStackUsed() {
    std::size_t untouched = 0;
    while (untouched < FiberContext.size() && FiberContext[untouched] == Fill) ++untouched;
    return (FiberContext.size() - untouched) * sizeof(std::uint64_t);
}

}

int main() {
    try {
        FiberContext.fill(Fill);
        auto* fiber = reinterpret_cast<FiberObject*>(&FiberObjectStorage);
        Require(_sceFiberInitializeImpl_nid_postfix(fiber, "optimize", OptimizeOnFiber, 0, FiberContext.data(), ContextBytes, nullptr, 0) == 0, "SPIR-V optimization on a fiber: the fiber was not initialized");
        const auto before = ShaderRecompiler::OptimizerHostThreads();
        Require(sceFiberRun_nid_postfix(fiber, 0, nullptr) == 0, "SPIR-V optimization on a fiber: the fiber did not run");
        const auto onFiber = ShaderRecompiler::OptimizerHostThreads() - before;
        Require(sceFiberFinalize(fiber) == 0, "SPIR-V optimization on a fiber: the fiber was not finalized");
        if (Failure) std::rethrow_exception(Failure);
        const auto used = FiberStackUsed();
        const auto beforeThread = ShaderRecompiler::OptimizerHostThreads();
        const auto onThreadResult = Optimize();
        const auto onThread = ShaderRecompiler::OptimizerHostThreads() - beforeThread;
        Require(Optimized == onThreadResult, "SPIR-V optimization on a fiber returned another module than on the thread");
        Require(used <= FiberStackLimit, "SPIR-V optimization on a fiber running on its context used " + std::to_string(used) + " bytes of it, more than " + std::to_string(FiberStackLimit) + ": the optimizer ran on it");
        Require(onFiber == 1, "SPIR-V optimization on a fiber running on its context started " + std::to_string(onFiber) + " host threads instead of one");
        Require(onThread == 0, "SPIR-V optimization on an ordinary thread started " + std::to_string(onThread) + " host threads instead of optimizing on its own stack");
        std::cout << "SPIR-V optimization on a fiber used " << used << " bytes of its context\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#else
int main() {
    return 77;
}
#endif
