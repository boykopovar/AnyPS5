#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
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
using namespace ShaderRecompiler;

constexpr std::uint32_t Records = 4;
constexpr std::uint32_t Textures = 3;

struct alignas(4096) GuestData {
    std::array<std::array<std::uint32_t, 8>, Records> table{};
    std::array<std::uint32_t, 64> output{};
};

alignas(256) std::array<std::array<std::uint32_t, 64>, Textures> textures{};
std::array<GuestData, 3> guest;

alignas(256) constexpr std::array<std::uint32_t, 46> WholeWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au,
    0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu,
    0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu,
    0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u,
    0xbefe0318u, 0xbf85ffe0u, 0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 60> MaskedTableWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0x7e040280u, 0x7e060280u, 0x7e080280u, 0x7e0a0280u,
    0x7e0c0280u, 0x7e0e0280u, 0x7e100280u, 0x7e120280u, 0xbe9c037eu, 0xe0302000u, 0x80011000u, 0xbf8c3f70u,
    0x7daa2087u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u, 0xbf8c3f70u, 0xbefe031cu, 0xbe94037eu,
    0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au, 0x00002d04u, 0xd760000bu,
    0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu, 0x00002d08u, 0xd760000fu,
    0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu, 0x7da40e0du,
    0x7da4100eu, 0x7da4120fu, 0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xbf85ffe0u,
    0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 50> PartialWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0x36200087u, 0xbe9c037eu, 0x7daa2087u, 0xe0382000u,
    0x80000201u, 0xe0382010u, 0x80000601u, 0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u,
    0xd7600009u, 0x00002d03u, 0xd760000au, 0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u,
    0xd760000du, 0x00002d07u, 0xd760000eu, 0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u,
    0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf0000f08u,
    0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xbf85ffe0u, 0xbf8c3f70u, 0xbefe031cu, 0xe0702000u,
    0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 50> PartialWave64{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0x36200087u, 0xbe9c047eu, 0x7daa2087u, 0xe0382000u,
    0x80000201u, 0xe0382010u, 0x80000601u, 0xbf8c3f70u, 0xbe94047eu, 0xbe961414u, 0xd7600008u, 0x00002d02u,
    0xd7600009u, 0x00002d03u, 0xd760000au, 0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u,
    0xd760000du, 0x00002d07u, 0xd760000eu, 0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98047eu, 0x7da40408u,
    0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf0000f08u,
    0x00020a0eu, 0xbe9a047eu, 0x8a941a14u, 0xbefe0418u, 0xbf85ffe0u, 0xbf8c3f70u, 0xbefe041cu, 0xe0702000u,
    0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 50> GuardedWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e1402c1u, 0xbe9c037eu, 0x7da80202u, 0xbf880026u, 0xe0382000u,
    0x80000201u, 0xe0382010u, 0x80000601u, 0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u,
    0xd7600009u, 0x00002d03u, 0xd760000au, 0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u,
    0xd760000du, 0x00002d07u, 0xd760000eu, 0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u,
    0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf0000f08u,
    0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xbf85ffe0u, 0xbf8c3f70u, 0xbefe031cu, 0xe0702000u,
    0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 59> StaleWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0x7e040208u, 0x7e060209u, 0x7e08020au, 0x7e0a020bu,
    0x7e0c020cu, 0x7e0e020du, 0x7e10020eu, 0x7e12020fu, 0x36200087u, 0xbe9c037eu, 0x7daa2087u, 0xe0382000u,
    0x80000201u, 0xe0382010u, 0x80000601u, 0xbf8c3f70u, 0xbefe031cu, 0xbe94037eu, 0xbe961314u, 0xd7600008u,
    0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au, 0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu,
    0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu, 0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu,
    0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu,
    0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xbf85ffe0u, 0xbf8c3f70u, 0xbefe031cu,
    0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 37> FirstLaneWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0x7e100502u, 0x7e120503u, 0x7e140504u, 0x7e160505u, 0x7e180506u, 0x7e1a0507u,
    0x7e1c0508u, 0x7e1e0509u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu,
    0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a7e1a18u, 0xbf85ffeau,
    0xbf8c3f70u, 0xbefe0314u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 42> Compare64Wave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au,
    0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu,
    0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7de40408u, 0x7de4080au, 0x7de40c0cu, 0x7de4100eu,
    0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xbf85ffe4u, 0xbf8c3f70u, 0xe0702000u,
    0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 68> MaskedStaleWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0x7e040280u, 0x7e060280u, 0x7e080280u, 0x7e0a0280u,
    0x7e0c0280u, 0x7e0e0280u, 0x7e100280u, 0x7e120280u, 0x36200087u, 0xbe9c037eu, 0x7da42087u, 0x7e040208u,
    0x7e060209u, 0x7e08020au, 0x7e0a020bu, 0x7e0c020cu, 0x7e0e020du, 0x7e10020eu, 0x7e12020fu, 0xbefe031cu,
    0x7daa2087u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u, 0xbf8c3f70u, 0xbefe031cu, 0xbe94037eu,
    0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au, 0x00002d04u, 0xd760000bu,
    0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu, 0x00002d08u, 0xd760000fu,
    0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu, 0x7da40c0cu, 0x7da40e0du,
    0x7da4100eu, 0x7da4120fu, 0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xbf85ffe0u,
    0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 45> SevenDwordsWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au,
    0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu,
    0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu,
    0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0xf0000f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u,
    0xbf85ffe1u, 0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 46> OutsideWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au,
    0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu,
    0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu,
    0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xbe9a037eu, 0x8a141a14u, 0xbefe0318u, 0xf0000f08u,
    0x00020a0eu, 0xbf85ffe0u, 0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 46> StoreWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au,
    0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu,
    0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu,
    0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf0200f08u, 0x00020a0eu, 0xbe9a037eu, 0x8a141a14u,
    0xbefe0318u, 0xbf85ffe0u, 0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 46> LodWave32{
    0x36020083u, 0x2c1c0082u, 0x7e1e0280u, 0x7e140280u, 0xe0382000u, 0x80000201u, 0xe0382010u, 0x80000601u,
    0xbf8c3f70u, 0xbe94037eu, 0xbe961314u, 0xd7600008u, 0x00002d02u, 0xd7600009u, 0x00002d03u, 0xd760000au,
    0x00002d04u, 0xd760000bu, 0x00002d05u, 0xd760000cu, 0x00002d06u, 0xd760000du, 0x00002d07u, 0xd760000eu,
    0x00002d08u, 0xd760000fu, 0x00002d09u, 0xbe98037eu, 0x7da40408u, 0x7da40609u, 0x7da4080au, 0x7da40a0bu,
    0x7da40c0cu, 0x7da40e0du, 0x7da4100eu, 0x7da4120fu, 0xf1800308u, 0x00820a0eu, 0xbe9a037eu, 0x8a141a14u,
    0xbefe0318u, 0xbf85ffe0u, 0xbf8c3f70u, 0xe0702000u, 0x80010a00u, 0xbf810000u,
};


std::array<std::uint32_t, 4> Buffer(const void* pointer, std::uint32_t stride, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), records, 0x01016facu};
}

std::array<std::uint32_t, 8> Image(std::uint32_t texture) {
    const auto address = reinterpret_cast<std::uintptr_t>(textures[texture].data());
    const std::uint32_t format = texture == 1u ? 22u : 20u;
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format << 20u) | (3u << 30u), 7u, 0x90000facu, 0u, 0u, 0u, 0u};
}

std::uint32_t TextureOf(std::uint32_t iteration, std::uint32_t key) {
    return key == Records - 1u - iteration ? Textures : (key + iteration) % Textures;
}

bool DecoratesNonUniform(std::span<const std::uint32_t> words) {
    for (std::size_t index = 5u; index < words.size();) {
        const auto count = words[index] >> 16u;
        if (count == 0u) return false;
        if ((words[index] & 0xffffu) == spv::OpDecorate && count >= 3u && index + 2u < words.size() && words[index + 2u] == spv::DecorationNonUniform) return true;
        index += count;
    }
    return false;
}

RecompileRequest Request(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t wave, std::uint32_t lanes, std::span<const std::uint32_t> userData, const std::array<MemoryRegion, 1>& regions) {
    const ShaderComputeStageInfo compute{{lanes, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    return {{ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}}, {wave, 0u, userData, compute, std::nullopt, std::nullopt, regions}, device.Target(), {0u, 0u, 0u, 128u}};
}

std::array<std::uint32_t, 20> UserData(GuestData& data, std::uint32_t lanes, std::uint32_t records = Records) {
    const auto table = Buffer(data.table.data(), 32u, records);
    const auto output = Buffer(data.output.data(), 4u, lanes);
    const auto fixed = Image(0u);
    constexpr std::array<std::uint32_t, 4> sampler{0x00000092u, 0x00fff000u, 0u, 0u};
    std::array<std::uint32_t, 20> userData{};
    std::copy(table.begin(), table.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    std::copy(fixed.begin(), fixed.end(), userData.begin() + 8u);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16u);
    return userData;
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
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

struct Outcome {
    std::string log;
    std::string failure;

    std::string Text() const {
        return log + failure;
    }
};

Outcome Dispatch(AgcDriver::VulkanDevice& device, const std::vector<RecompileResult>& shaders) {
    Outcome outcome;
    StderrCapture capture;
    try {
        for (const auto& shader : shaders) {
            device.Dispatch(shader, 1u, 1u, 1u);
            device.SubmitRecorded(false);
        }
        device.WaitIdle();
    } catch (const std::exception& error) {
        outcome.failure = error.what();
    }
    outcome.log = capture.Finish();
    return outcome;
}

bool Faulted(const Outcome& outcome) {
    return outcome.Text().find("image table:") != std::string::npos;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t wave, bool partial, const std::string& what, std::uint32_t records = Records) {
    for (std::uint32_t texture = 0u; texture < Textures; ++texture) {
        for (std::uint32_t x = 0u; x < textures[texture].size(); ++x) textures[texture][x] = 0x1000u * (texture + 1u) + x;
    }
    const auto lanes = wave == 64u ? 64u : std::min(32u, device.Target().subgroupSize);
    std::vector<RecompileResult> shaders;
    for (std::uint32_t iteration = 0u; iteration < guest.size(); ++iteration) {
        auto& data = guest[iteration];
        for (std::uint32_t key = 0u; key < Records; ++key) {
            const auto texture = TextureOf(iteration, key);
            data.table[key] = texture == Textures ? std::array<std::uint32_t, 8>{} : Image(texture);
        }
        data.output.fill(0xdeadbeefu);
        const auto userData = UserData(data, lanes, records);
        const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(&data), std::as_bytes(std::span(&data, 1u))}};
        auto shader = Recompile(Request(device, code, wave, lanes, userData, regions));
        Require(DecoratesNonUniform(shader.spirv.Words()), what + ": the per-lane table slot is not decorated NonUniform");
        Require(shaders.empty() || shader.variantId == shaders.front().variantId, what + ": a new table changed the compiled artifact");
        shaders.push_back(std::move(shader));
    }
    const auto outcome = Dispatch(device, shaders);
    Require(outcome.failure.empty() && !Faulted(outcome), what + " failed:\n" + outcome.Text());
    for (std::uint32_t iteration = 0u; iteration < guest.size(); ++iteration) {
        for (std::uint32_t lane = 0u; lane < lanes; ++lane) {
            const auto key = lane % Records;
            const auto texture = TextureOf(iteration, key);
            const bool skipped = partial && lane % 8u == 7u;
            const auto expected = skipped || key >= records || texture == Textures ? 0u : textures[texture][lane / 4u];
            const auto actual = guest[iteration].output[lane];
            Require(actual == expected, what + ": iteration " + std::to_string(iteration) + " lane " + std::to_string(lane) + " read " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void RunMaskedTable(AgcDriver::VulkanDevice& device, bool invalid, bool unbounded = false, bool partial = false) {
    const std::string what = (partial ? invalid ? "partially skipped descriptor loads from a short-stride table" : "partially skipped descriptor loads from a valid table" : invalid ? "skipped descriptor loads from a nonempty swizzled table" : "skipped descriptor loads from a valid table") + std::string(unbounded ? " with an unbounded key" : "");
    auto& data = guest[0];
    for (std::uint32_t x = 0u; x < textures[0].size(); ++x) textures[0][x] = 0x1000u + x;
    for (auto& record : data.table) record = Image(0u);
    data.output.fill(7u);
    const auto lanes = std::min(32u, device.Target().subgroupSize);
    if (partial) for (std::uint32_t lane = 0u; lane < lanes; ++lane) data.output[lane] = lane % 2u == 0u ? 0u : 7u;
    auto userData = UserData(data, lanes);
    if (invalid && partial) userData[1] = (userData[1] & 0xc000ffffu) | (28u << 16u);
    else if (invalid) userData[1] |= 1u << 31u;
    alignas(256) auto code = MaskedTableWave32;
    if (unbounded) code[0] = 0x7e020300u;
    const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(&data), std::as_bytes(std::span(&data, 1u))}};
    auto request = Request(device, code, 32u, lanes, userData, regions);
    request.useCache = false;
    std::vector<RecompileResult> shaders;
    shaders.push_back(Recompile(request));
    Require(DecoratesNonUniform(shaders.front().spirv.Words()), what + ": the per-lane table slot is not decorated NonUniform");
    const auto outcome = Dispatch(device, shaders);
    if (invalid && partial) {
        Require(Faulted(outcome) && outcome.Text().find("records smaller than the descriptor") != std::string::npos, what + " did not fault for the lanes that loaded the invalid descriptor:\n" + outcome.Text());
        return;
    }
    Require(outcome.failure.empty() && !Faulted(outcome), what + (partial ? " faulted for supported descriptor loads:\n" : " faulted although no descriptor load executed:\n") + outcome.Text());
    for (std::uint32_t lane = 0u; lane < lanes; ++lane) {
        const auto expected = partial && lane % 2u == 0u ? textures[0][lane / 4u] : 0u;
        Require(data.output[lane] == expected, what + ": lane " + std::to_string(lane) + " did not retain the descriptor selected by its load predicate");
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& what) {
    auto& data = guest[0];
    const auto userData = UserData(data, 32u);
    const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(&data), std::as_bytes(std::span(&data, 1u))}};
    auto request = Request(device, code, 32u, std::min(32u, device.Target().subgroupSize), userData, regions);
    request.useCache = false;
    bool refused = false;
    try {
        static_cast<void>(Recompile(request));
    } catch (const std::exception& error) {
        refused = std::string_view(error.what()).find("GetImageResource dword 0 is not a valid runtime value") != std::string_view::npos;
    }
    Require(refused, what + " was accepted");
}

void RunWithoutRecords(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 4>& table, const std::string& what) {
    auto& data = guest[0];
    data.output.fill(0xdeadbeefu);
    const auto lanes = std::min(32u, device.Target().subgroupSize);
    auto userData = UserData(data, lanes);
    std::copy(table.begin(), table.end(), userData.begin());
    const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(&data), std::as_bytes(std::span(&data, 1u))}};
    std::vector<RecompileResult> shaders;
    shaders.push_back(Recompile(Request(device, GuardedWave32, 32u, lanes, userData, regions)));
    const auto outcome = Dispatch(device, shaders);
    Require(outcome.failure.empty() && !Faulted(outcome), what + " failed:\n" + outcome.Text());
    for (std::uint32_t lane = 0u; lane < lanes; ++lane) {
        const auto actual = data.output[lane];
        Require(actual == 0xffffffffu, what + ": lane " + std::to_string(lane) + " read " + std::to_string(actual) + ", expected the value it holds outside the loop");
    }
}

void RequireFault(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 4>& table, std::string_view reason, const std::string& what) {
    auto& data = guest[0];
    data.output.fill(0xdeadbeefu);
    const auto lanes = std::min(32u, device.Target().subgroupSize);
    auto userData = UserData(data, lanes);
    std::copy(table.begin(), table.end(), userData.begin());
    const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(&data), std::as_bytes(std::span(&data, 1u))}};
    auto request = Request(device, code, 32u, lanes, userData, regions);
    request.useCache = false;
    std::vector<RecompileResult> shaders;
    shaders.push_back(Recompile(request));
    Require(DecoratesNonUniform(shaders.front().spirv.Words()), what + ": the per-lane table slot is not decorated NonUniform");
    const auto outcome = Dispatch(device, shaders);
    Require(Faulted(outcome) && outcome.Text().find(reason) != std::string::npos, what + " was not refused:\n" + outcome.Text());
}

void RequireActiveInvalidKey(AgcDriver::VulkanDevice& device, std::uint32_t mode) {
    auto& data = guest[0];
    for (auto& record : data.table) record = Image(0u);
    alignas(256) auto code = WholeWave32;
    code[0] = 0x7e0202c1u;
    code[5] = 0xa0000201u;
    code[7] = 0xa0000601u;
    auto table = Buffer(data.table.data(), 32u, mode == 2u ? Records : static_cast<std::uint32_t>(sizeof(data.table)));
    table[3] = (table[3] & ~(3u << 28u)) | (mode << 28u);
    RequireFault(device, code, table, "table V# indexed per lane is swizzled", "an active UINT_MAX key with unsupported OOB_SELECT " + std::to_string(mode));
}

void RunLod(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, Records>& records, bool refused, const std::string& what) {
    auto& data = guest[0];
    for (std::uint32_t key = 0u; key < Records; ++key) data.table[key] = records[key] == Textures ? std::array<std::uint32_t, 8>{} : Image(records[key]);
    const auto table = Buffer(data.table.data(), 32u, Records);
    if (refused) {
        RequireFault(device, LodWave32, table, "records indexed per lane need different image modes under implicit derivatives", what);
        return;
    }
    const auto lanes = std::min(32u, device.Target().subgroupSize);
    const auto userData = UserData(data, lanes);
    const std::array regions{MemoryRegion{reinterpret_cast<std::uintptr_t>(&data), std::as_bytes(std::span(&data, 1u))}};
    auto request = Request(device, LodWave32, 32u, lanes, userData, regions);
    request.useCache = false;
    std::vector<RecompileResult> shaders;
    shaders.push_back(Recompile(request));
    Require(DecoratesNonUniform(shaders.front().spirv.Words()), what + ": the per-lane table slot is not decorated NonUniform");
    const auto outcome = Dispatch(device, shaders);
    Require(outcome.failure.empty() && !Faulted(outcome), what + " failed:\n" + outcome.Text());
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--masked-invalid-table-only"), "unknown waterfall test argument");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunMaskedTable(*device, false);
        RunMaskedTable(*device, true);
        RunMaskedTable(*device, false, true);
        RunMaskedTable(*device, true, true);
        RunMaskedTable(*device, false, false, true);
        RunMaskedTable(*device, true, false, true);
        RequireActiveInvalidKey(*device, 2u);
        RequireActiveInvalidKey(*device, 3u);
        if (argc == 2) {
            std::cout << "masked invalid table tests passed\n";
            return 0;
        }
        Run(*device, WholeWave32, 32u, false, "wave32 waterfall over the whole wave");
        Run(*device, WholeWave32, 32u, false, "wave32 waterfall over a table V# with fewer records than keys", Records - 1u);
        Run(*device, PartialWave32, 32u, true, "wave32 waterfall under a partial EXEC");
        if (device->Target().subgroupSize >= 32u) Run(*device, PartialWave64, 64u, true, "wave64 waterfall under a partial EXEC");
        Run(*device, FirstLaneWave32, 32u, false, "wave32 waterfall picking lanes with v_readfirstlane");
        Run(*device, Compare64Wave32, 32u, false, "wave32 waterfall comparing the T# with v_cmpx_eq_u64");
        Run(*device, GuardedWave32, 32u, false, "wave32 waterfall behind a branch over the lanes inside the table");
        RunWithoutRecords(*device, {0u, 0u, 0u, 0u}, "a waterfall no lane enters over a null table V#");
        RunWithoutRecords(*device, Buffer(guest[0].table.data(), 0u, 0u), "a waterfall no lane enters over a table V# with stride 0 and no records");
        auto unchecked = Buffer(guest[0].table.data(), 32u, 0u);
        unchecked[1] |= 1u << 31u;
        unchecked[3] |= (1u << 23u) | (2u << 28u);
        RunWithoutRecords(*device, unchecked, "a waterfall no lane enters over a swizzled ADD_TID table V# with OOB_SELECT 2 and no records");
        RequireRefused(*device, StaleWave32, "a waterfall over a table T# and a T# left in the VGPRs of lanes the load skipped");
        RequireRefused(*device, MaskedStaleWave32, "a waterfall over a table T# and a T# written under EXEC to the VGPRs of lanes the load skipped");
        RequireRefused(*device, SevenDwordsWave32, "a waterfall that compares seven of the eight T# dwords");
        RequireRefused(*device, OutsideWave32, "an image load after the waterfall restores EXEC");
        RequireRefused(*device, StoreWave32, "an image store in a waterfall");
        auto swizzled = Buffer(guest[0].table.data(), 32u, Records);
        swizzled[1] |= 1u << 31u;
        RequireFault(*device, WholeWave32, swizzled, "table V# indexed per lane is swizzled", "a waterfall over a swizzled table V# with records");
        RequireFault(*device, WholeWave32, Buffer(guest[0].table.data(), 28u, Records), "records smaller than the descriptor", "a waterfall over a table V# whose records are smaller than a T#");
        RunLod(*device, {1u, 1u, 1u, 1u}, false, "an LOD query in a waterfall over a table of one image mode");
        RunLod(*device, {0u, 1u, 0u, 1u}, true, "an LOD query in a waterfall over a table of float and integer images");
        RunLod(*device, {Textures, 0u, 0u, 0u}, false, "an LOD query in a waterfall over a null record 0 and integer images");
        RunLod(*device, {Textures, 0u, 1u, 0u}, true, "an LOD query in a waterfall over a null record 0 and float and integer images");
        std::cout << "waterfall image tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
