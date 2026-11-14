#include "OracleDispatch.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t PageBytes = 4096;
constexpr std::size_t BlockAlignment = 65536;
constexpr std::uint32_t MaxThreads = 1024;
constexpr std::uint32_t RowBytes = 16;
constexpr std::uint32_t OutputRowBytes = 64;

std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t size) : size(size) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockAlignment, size));
#endif
        if (block == nullptr) throw std::runtime_error("cannot allocate the guest block of " + std::to_string(size) + " bytes");
        GuestAllocations::Mutation().Add(block, size, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::size_t Size() const { return size; }

private:
    std::uint8_t* block = nullptr;
    std::size_t size;
};

}

std::vector<std::byte> RunOracleKernel(AgcDriver::VulkanDevice& device, const OracleKernel& kernel, std::uint32_t threads, std::span<const std::byte> input) {
    if (threads == 0) throw std::runtime_error("threads must be at least 1");
    if (threads > MaxThreads) throw std::runtime_error("threads must not exceed " + std::to_string(MaxThreads));
    if (kernel.waveSize == 0 || threads % kernel.waveSize != 0) throw std::runtime_error("threads must be a multiple of the wave size " + std::to_string(kernel.waveSize));
    const std::size_t rowBytes = static_cast<std::size_t>(threads) * RowBytes;
    const std::size_t outputBytes = static_cast<std::size_t>(threads) * OutputRowBytes;
    if (input.size() < rowBytes) throw std::runtime_error("input holds " + std::to_string(input.size()) + " bytes, " + std::to_string(rowBytes) + " are needed");
    if (kernel.text.empty()) throw std::runtime_error("kernel has no instructions");

    const std::size_t inputOffset = PageBytes;
    const std::size_t outputOffset = AlignUp(inputOffset + input.size(), PageBytes);
    GuestBlock guest(AlignUp(outputOffset + outputBytes, BlockAlignment));
    std::memset(guest.Data(), 0, guest.Size());
    std::memcpy(guest.Data() + inputOffset, input.data(), input.size());
    std::memset(guest.Data() + outputOffset, 0xee, outputBytes);
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::array<std::uint64_t, 2> kernarg{base + inputOffset, base + outputOffset};
    std::memcpy(guest.Data(), kernarg.data(), sizeof(kernarg));

    const std::span<const std::uint32_t> code(kernel.text);
    const std::array<ShaderRecompiler::MemoryRegion, 2> regions{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
        {base, std::span<const std::byte>(reinterpret_cast<const std::byte*>(guest.Data()), guest.Size())}
    }};
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u)};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, (kernel.ldsBytes + 3u) / 4u, kernel.groupIdEnable, kernel.tgSizeEnable, kernel.threadIdComponentCount};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {kernel.waveSize, 0, userData, compute, std::nullopt, std::nullopt, regions, kernel.floatMode},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::vector<std::byte> output(outputBytes);
    std::memcpy(output.data(), guest.Data() + outputOffset, outputBytes);
    return output;
}
