#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>


namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::array<std::uint8_t, 4> Texel{0x10u, 0x80u, 0xc0u, 0xffu};
constexpr std::array<std::uint8_t, 4> OtherTexel{0xf0u, 0x20u, 0x00u, 0x40u};

alignas(256) constexpr std::array<std::uint32_t, 8> SampleCode{0x7e0002f0u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xe0780000u, 0x80030000u, 0xbf810000u};

alignas(256) std::array<std::uint8_t, 1024> TextureMemory{};
alignas(256) std::array<std::uint8_t, 1024> OtherTextureMemory{};
alignas(4096) std::array<std::array<float, 1024>, 4> Outputs{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header SampleHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = 0;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

std::uint32_t Low(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

std::array<std::uint32_t, 8> Image(const void* pointer) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
}

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Sample(std::vector<std::uint32_t>& words, const void* image, float* output) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(words, offset, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, 16u << 1u);
    std::vector<std::uint32_t> userData;
    const auto descriptor = Image(image);
    userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    userData.insert(userData.end(), {0u, 0u, 0u, 0u});
    userData.insert(userData.end(), {Low(output), High(output) & 0xffffu, static_cast<std::uint32_t>(4u * sizeof(float)), 0x31016facu});
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "the compute submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void RequireSampled(const float* output, const std::array<std::uint8_t, 4>& expected, const std::string& what, bool heap) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const float wanted = static_cast<float>(expected[channel]) / 255.0f;
        Require(std::fabs(output[channel] - wanted) < 1e-6f, what + ": channel " + std::to_string(channel) + " is " + std::to_string(output[channel]) + ", expected " + std::to_string(wanted) + (heap ? " with the heap on" : ""));
    }
}

void Fill(std::array<std::uint8_t, 1024>& memory, const std::array<std::uint8_t, 4>& texel) {
    for (std::size_t offset = 0; offset < memory.size(); offset += texel.size()) std::memcpy(memory.data() + offset, texel.data(), texel.size());
}

void DriverSample(bool heap) {
    Outputs[0].fill(-1.0f);
    std::vector<std::uint32_t> words;
    Sample(words, TextureMemory.data(), Outputs[0].data());
    Sample(words, TextureMemory.data(), Outputs[0].data());
    Submit(words);
    RequireSampled(Outputs[0].data(), Texel, "the sampled texture", heap);
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const void* image, float* output) {
    std::vector<std::uint32_t> userData;
    const auto descriptor = Image(image);
    userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    userData.insert(userData.end(), {0u, 0u, 0u, 0u});
    userData.insert(userData.end(), {Low(output), High(output) & 0xffffu, static_cast<std::uint32_t>(4u * sizeof(float)), 0x31016facu});
    const std::span<const std::uint32_t> code(SampleCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}}, {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory}, device.ComputeTarget(32u), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void DeviceSample(AgcDriver::VulkanDevice& device, bool heap) {
    for (std::size_t index = 1; index < Outputs.size(); ++index) Outputs[index].fill(-1.0f);
    const auto first = Compile(device, OtherTextureMemory.data(), Outputs[1].data());
    const auto second = Compile(device, TextureMemory.data(), Outputs[2].data());
    Require(!first.gpuResolvedImages && !second.gpuResolvedImages, "a sampling compute stage took the heap");
    device.Dispatch(first, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    device.Dispatch(second, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    device.WaitIdle();
    RequireSampled(Outputs[1].data(), OtherTexel, "the first of two textures in one batch", heap);
    RequireSampled(Outputs[2].data(), Texel, "the second of two textures in one batch", heap);
    const auto again = Compile(device, TextureMemory.data(), Outputs[3].data());
    device.Dispatch(again, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    device.WaitIdle();
    RequireSampled(Outputs[3].data(), Texel, "the first texture again in a later batch", heap);
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        const bool heap = std::getenv("APS5_GPU_HEAP") != nullptr;
        Fill(TextureMemory, Texel);
        Fill(OtherTextureMemory, OtherTexel);
        Registered(SampleHeader, SampleCode.data(), sizeof(SampleCode));
        DriverSample(heap);
        AgcDriverShutdown_nid_postfix();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            DeviceSample(*device, heap);
        }
        std::cout << "compute stages sampled their textures off the heap" << (heap ? " with the heap on" : "") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
