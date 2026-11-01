#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t WideThreads = 64;
alignas(256) std::array<std::uint32_t, WideThreads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 26> Code{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x00010004, 0xd7610003, 0x00010205, 0xd7610003, 0x00010406,
    0xd7610003, 0x00010607, 0xbe840380, 0xbe850380, 0xbe860380, 0xbe870380, 0xd7600008, 0x00010103, 0xd7600009,
    0x00010303, 0xd760000a, 0x00010503, 0xd760000b, 0x00010703, 0xe0702000, 0x80020001, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 27> IndexedStoreCode{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x00010004, 0xd7610003, 0x00010205, 0xd7610003, 0x00010406,
    0xd7610003, 0x00010607, 0x7e048500, 0xbe840380, 0xbe850380, 0xbe860380, 0xbe870380, 0xd7600008, 0x00010103,
    0xd7600009, 0x00010303, 0xd760000a, 0x00010503, 0xd760000b, 0x00010703, 0xe0702000, 0x80020001, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 28> ReusedRegisterCode{
    0x7e060300, 0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x00010004, 0xd7610003, 0x00010205, 0xd7610003,
    0x00010406, 0xd7610003, 0x00010607, 0x7e048500, 0xbe840380, 0xbe850380, 0xbe860380, 0xbe870380, 0xd7600008,
    0x00010103, 0xd7600009, 0x00010303, 0xd760000a, 0x00010503, 0xd760000b, 0x00010703, 0xe0702000, 0x80020001,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 14> OverwrittenSpillCode{
    0x877c8104, 0x7e040280, 0x7e060280, 0xd7610004, 0x00010008, 0x7e048500, 0x7e0802f2, 0xd7600008, 0x00010104,
    0xf09c0f08, 0x00400802, 0xe0700000, 0x80030800, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> AliasingStoreCode{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x0001000d, 0x7e040287, 0x7e048500,
    0xd7600008, 0x00010103, 0x7e040208, 0xe0702000, 0x80010201, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> WideAliasingStoreCode{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x0001500d, 0x7e040287, 0x7e048500,
    0xd7600008, 0x00015103, 0x7e040208, 0xe0702000, 0x80010201, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> StoreAfterReadCode{
    0x7e020300, 0xbefc030c, 0x7e048500, 0xd7610003, 0x0001000d, 0xd7600008,
    0x00010103, 0x7e048500, 0x7e040208, 0xe0702000, 0x80010201, 0xbf810000,
};

constexpr std::uint32_t Spilled = 0x1234u;

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = TemporaryFile::Open();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::uint32_t waveSize = Threads) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::string Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t relativeIndex = 0, std::uint32_t waveSize = Threads) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[12] = relativeIndex;
    userData[13] = Spilled;
    const auto result = Compile(device, code, userData, waveSize);
    StderrCapture log;
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return log.Finish();
}

constexpr std::string_view Overwritten = "which holds a scalar spilled with v_writelane_b32 that a later v_readlane_b32 was replaced with";

void CheckSpilled(const std::string& log, const std::string& name, std::uint32_t lanes = Threads) {
    Require(log.find(Overwritten) == std::string::npos, name + ": reported an overwritten spill: " + log);
    for (std::uint32_t tid = 0; tid < lanes; ++tid) {
        Require(Output[tid] == Spilled, name + ": lane " + std::to_string(tid) + " stored " + std::to_string(Output[tid]) + ", expected the spilled " + std::to_string(Spilled));
    }
}

void Check(const std::string& name) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Require(Output[tid] == tid, name + ": lane " + std::to_string(tid) + " stored " + std::to_string(Output[tid]) + " through the V# read back from a window register");
    }
}

void CheckRefused(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& name) {
    const std::vector<std::uint32_t> userData(16, 0u);
    std::string error;
    try {
        static_cast<void>(Compile(device, code, userData));
    } catch (const std::exception& thrown) {
        error = thrown.what();
    }
    Require(error.find("GetSamplerResource dword 0 is not a valid runtime value") != std::string::npos, name + ": " + (error.empty() ? std::string("recompiled") : error));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < Threads) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device, Code);
        Check("window lane spill");
        Run(*device, IndexedStoreCode);
        Check("window lane spill past an indexed store");
        CheckRefused(*device, OverwrittenSpillCode, "window lane spill overwritten by an ordinary write before its v_readlane");
        Run(*device, ReusedRegisterCode);
        Check("window lane spill into a register with ordinary writes past an indexed store");
        CheckSpilled(Run(*device, AliasingStoreCode, 0), "window lane spill past an indexed store with m0 = 0");
        const auto aliased = Run(*device, AliasingStoreCode, 1);
        Require(aliased.find(Overwritten) != std::string::npos, "window lane spill overwritten by an indexed store with m0 = 1 was not reported: " + aliased);
        CheckSpilled(Run(*device, StoreAfterReadCode, 1), "window lane spill overwritten by an indexed store with m0 = 1 after its v_readlane");
        if (device->Target().subgroupSize >= WideThreads) {
            CheckSpilled(Run(*device, WideAliasingStoreCode, 0, WideThreads), "wave64 window lane spill in lane 40 past an indexed store with m0 = 0", WideThreads);
            const auto wide = Run(*device, WideAliasingStoreCode, 1, WideThreads);
            Require(wide.find("stored 40 in lane 40 of vector window slot 1") != std::string::npos, "wave64 window lane spill in lane 40 overwritten by an indexed store with m0 = 1 was not reported: " + wide);
        } else {
            std::printf("wave64 cases skipped, subgroup size %u\n", device->Target().subgroupSize);
        }
        std::puts("window lane spill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
